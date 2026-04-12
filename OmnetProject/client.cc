#include <omnetpp.h>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <fstream>

using namespace omnetpp;

class TaskMessage : public cMessage {
  public:
    int src;
    int target;
    int type; // 0=subtask, 1=result, 2=gossip
    std::vector<int> data;
    int result;
    int gossipOrigin;
    int gossipHops;
    std::string gossipId;
    std::string gossip;

    TaskMessage(const char *name=nullptr) : cMessage(name) {
        src = -1;
        target = -1;
        type = -1;
        result = 0;
        gossipOrigin = -1;
        gossipHops = 0;
    }
};

class Client : public cSimpleModule {
  private:
    int id, N;
    int totalSubtasks = 10;
    int receivedResults = 0;
        int gossipSequence = 0;
    std::vector<int> results;
        std::unordered_map<std::string, std::unordered_set<int>> gossipCoverage;
    std::ofstream outfile;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    void sendSubtasks();
    void forwardMessage(TaskMessage *msg);
    void sendGossip();
    int getSuccessorGate() const;
};

Define_Module(Client);

void Client::initialize() {
    id = getIndex();
    N = par("N");

    outfile.open("outputfile.txt", std::ios::app);

    if (id == 0) {
        sendSubtasks();
    }
}

int Client::getSuccessorGate() const {
    int successorId = (id + 1) % N;

    for (int i = 0; i < gateSize("out"); i++) {
        cGate *outGate = gate("out", i);
        if (outGate->getNextGate() == nullptr) {
            continue;
        }

        cModule *nextModule = outGate->getNextGate()->getOwnerModule();
        if (nextModule->par("id").intValue() == successorId) {
            return i;
        }
    }

    return gateSize("out") > 0 ? 0 : -1;
}

void Client::sendSubtasks() {
    std::vector<int> arr(20);
    for (int i = 0; i < 20; i++) arr[i] = intuniform(1, 100);

    int chunkSize = arr.size() / totalSubtasks;

    for (int i = 0; i < totalSubtasks; i++) {
        TaskMessage *msg = new TaskMessage("Subtask");
        msg->type = 0;
        msg->src = id;
        msg->target = i % N;

        int start = i * chunkSize;
        int end = std::min(start + chunkSize, (int)arr.size());

        msg->data = std::vector<int>(arr.begin()+start, arr.begin()+end);

        forwardMessage(msg);
    }
}

void Client::handleMessage(cMessage *m) {
    TaskMessage *msg = check_and_cast<TaskMessage *>(m);

    if (msg->type == 2) {
        auto &coveredNodes = gossipCoverage[msg->gossipId];
        bool coverageExpanded = false;

        for (int nodeId : msg->data) {
            if (coveredNodes.insert(nodeId).second) {
                coverageExpanded = true;
            }
        }

        if (coveredNodes.insert(id).second) {
            coverageExpanded = true;
        }

        if (!coverageExpanded && coveredNodes.size() < static_cast<size_t>(N)) {
            delete msg;
            return;
        }

        std::vector<int> expandedCoverage(coveredNodes.begin(), coveredNodes.end());
        std::sort(expandedCoverage.begin(), expandedCoverage.end());
        msg->data = expandedCoverage;

        EV << "Gossip: " << msg->gossip << " | Local Time: " << simTime() << " | From: Node " << msg->src << "\n";
        outfile << "Gossip: " << msg->gossip << " | Local Time: " << simTime() << " | From: Node " << msg->src << "\n";

        if (coveredNodes.size() == static_cast<size_t>(N)) {
            EV << "Client " << id << " observed gossip coverage from all clients. Terminating.\n";
            outfile << "Client " << id << " observed gossip coverage from all clients. Terminating.\n";
            delete msg;
            endSimulation();
            return;
        }

        int successorGate = getSuccessorGate();
        if (successorGate >= 0) {
            send(msg->dup(), "out", successorGate);
        }

        delete msg;
        return;
    }

    if (msg->target == id) {

        if (msg->type == 0) {
            // Process subtask
            int maxVal = *std::max_element(msg->data.begin(), msg->data.end());

            TaskMessage *res = new TaskMessage("Result");
            res->type = 1;
            res->src = id;
            res->target = msg->src;
            res->result = maxVal;

            forwardMessage(res);
        }

        else if (msg->type == 1) {
            results.push_back(msg->result);
            receivedResults++;

            EV << "Client " << id << " received result: " << msg->result << "\n";
            outfile << "Result received: " << msg->result << "\n";

            if (receivedResults == totalSubtasks) {
                int finalMax = *std::max_element(results.begin(), results.end());
                EV << "Final Max: " << finalMax << "\n";
                outfile << "Final Max: " << finalMax << "\n";

                sendGossip();
            }
        }

        delete msg;
    }
    else {
        forwardMessage(msg);
    }
}

void Client::forwardMessage(TaskMessage *msg) {
    int nextGate = intuniform(0, gateSize("out")-1);
    send(msg, "out", nextGate);
}

void Client::sendGossip() {
    TaskMessage *msg = new TaskMessage("Gossip");
    msg->type = 2;
    msg->src = id;
    msg->target = -1;
    msg->gossipOrigin = id;
    msg->gossipHops = 0;
    msg->gossipId = std::to_string(id) + ":" + std::to_string(++gossipSequence) + ":" + std::to_string(simTime().raw());

    std::string g = std::to_string(simTime().dbl()) + ":Node" + std::to_string(id);
    msg->gossip = g;

    gossipCoverage[msg->gossipId].insert(id);
    msg->data = {id};

    int successorGate = getSuccessorGate();
    if (successorGate >= 0) {
        send(msg->dup(), "out", successorGate);
    }

    delete msg;
}