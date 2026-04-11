#include <omnetpp.h>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <fstream>

using namespace omnetpp;

class TaskMessage : public cMessage {
  public:
    int src;
    int target;
    int type; // 0=subtask, 1=result, 2=gossip
    std::vector<int> data;
    int result;
    std::string gossip;

    TaskMessage(const char *name=nullptr) : cMessage(name) {}
};

class Client : public cSimpleModule {
  private:
    int id, N;
    int totalSubtasks = 10;
    int receivedResults = 0;
    std::vector<int> results;
    std::unordered_set<std::string> seenGossip;
    std::ofstream outfile;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    void sendSubtasks();
    void forwardMessage(TaskMessage *msg);
    void sendGossip();
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

        else if (msg->type == 2) {
            if (seenGossip.insert(msg->gossip).second) {
                EV << "Gossip received: " << msg->gossip << "\n";
                outfile << "Gossip: " << msg->gossip << "\n";

                for (int i = 0; i < gateSize("out"); i++) {
                    send(msg->dup(), "out", i);
                }
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

    std::string g = std::to_string(simTime().dbl()) + ":Node" + std::to_string(id);
    msg->gossip = g;

    seenGossip.insert(g);

    for (int i = 0; i < gateSize("out"); i++) {
        send(msg->dup(), "out", i);
    }

    delete msg;
}