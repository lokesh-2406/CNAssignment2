/**
 * client.cc
 * CSL3080 Assignment-2 — Peer-to-Peer Task Distribution with Chord-Inspired Routing
 *
 * Each client node:
 *   1. (Node 0 only) Splits an array into x subtasks and routes each to client i%N
 *      using Chord finger-table-based O(log N) routing.
 *   2. Processes subtasks (finds local max) and returns the result to the originator.
 *   3. After collecting all results, computes the global max and initiates gossip.
 *   4. Participates in flooding-based gossip: forwards every first-seen message to
 *      ALL neighbours except the one it arrived from.
 *   5. Terminates once it has seen gossip from every client in the network.
 */

#include <omnetpp.h>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <cmath>
#include <string>

using namespace omnetpp;

// ---------------------------------------------------------------------------
// TaskMessage
// Overrides dup() so it returns TaskMessage* — required for correct casting
// after dup() in floodGossip without needing Register_Class().
// ---------------------------------------------------------------------------
class TaskMessage : public cMessage {
  public:
    int  src;
    int  target;
    int  type;            // 0=subtask  1=result  2=gossip
    std::vector<int> data;
    int  result;
    std::string gossipId;   // "<originId>:1"
    std::string gossipMsg;  // "<simTime>:<nodeId>:Client<id>#"

    TaskMessage(const char *name = nullptr) : cMessage(name) {
        src    = -1;
        target = -1;
        type   = -1;
        result = 0;
    }

    // Override dup() to return TaskMessage* directly — avoids check_and_cast
    // failure when the base-class dup() returns cMessage*.
    virtual TaskMessage *dup() const override {
        return new TaskMessage(*this);
    }
};

// ---------------------------------------------------------------------------
// Client module
// ---------------------------------------------------------------------------
class Client : public cSimpleModule {
  private:
    int id, N, totalSubtasks;
    int receivedResults;
    std::vector<int> subtaskResults;

    std::unordered_set<std::string> seenGossip;
    std::unordered_set<int>         gossipOriginatorsSeen;

    std::vector<int> fingerGates;   // fingerGates[k] = output gate index for finger k

    std::ofstream outfile;          // opened only on node 0

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

  private:
    void buildFingerTable();
    int  bestGateTowards(int target) const;
    void sendSubtasks();
    void routeMessage(TaskMessage *msg);
    void sendGossip();
    void floodGossip(TaskMessage *msg, int excludeOutGate);
    void log(const std::string &line);
};

Define_Module(Client);

// ---------------------------------------------------------------------------
void Client::initialize() {
    id            = getIndex();
    N             = par("N");
    totalSubtasks = par("totalSubtasks");
    receivedResults = 0;

    // Only node 0 writes to the output file to prevent concurrent-write corruption.
    if (id == 0) {
        outfile.open("outputfile.txt", std::ios::out | std::ios::trunc);
        outfile << "=== Simulation Output ===\n";
        outfile.flush();
    }

    buildFingerTable();

    if (id == 0) sendSubtasks();
}

void Client::finish() {
    if (outfile.is_open()) outfile.close();
}

// ---------------------------------------------------------------------------
// buildFingerTable
// Chord finger k → node (id + 2^k) % N, resolved to an output gate index.
// ---------------------------------------------------------------------------
void Client::buildFingerTable() {
    int numFingers = (N > 1) ? (int)std::floor(std::log2(N - 1)) + 1 : 1;
    for (int k = 0; k < numFingers; k++) {
        int fingerNode = (id + (1 << k)) % N;
        bool found = false;
        for (int g = 0; g < gateSize("out"); g++) {
            const cGate *og = gate("out", g);
            if (!og->isConnected()) continue;
            if (og->getPathEndGate()->getOwnerModule()->getIndex() == fingerNode) {
                fingerGates.push_back(g);
                found = true;
                break;
            }
        }
        if (!found) fingerGates.push_back(-1);
    }
}

// ---------------------------------------------------------------------------
// bestGateTowards — greedy Chord routing
// ---------------------------------------------------------------------------
int Client::bestGateTowards(int target) const {
    auto cwDist = [&](int from, int to) -> int {
        return (to - from + N) % N;
    };

    int distToTarget = cwDist(id, target);
    int bestGate = -1;
    int bestDist = distToTarget;

    for (int k = (int)fingerGates.size() - 1; k >= 0; k--) {
        if (fingerGates[k] < 0) continue;
        const cGate *og = gate("out", fingerGates[k]);
        if (!og->isConnected()) continue;
        int peerId    = og->getPathEndGate()->getOwnerModule()->getIndex();
        int distPeer  = cwDist(id, peerId);
        int distAfter = cwDist(peerId, target);
        // Jump to peer only if it moves us strictly closer without overshooting
        if (distPeer < distToTarget && distAfter < bestDist) {
            bestDist = distAfter;
            bestGate = fingerGates[k];
        }
    }

    // Fallback 1: ring successor (finger 0)
    if (bestGate < 0 && !fingerGates.empty() && fingerGates[0] >= 0)
        bestGate = fingerGates[0];
    // Fallback 2: first connected gate
    if (bestGate < 0) {
        for (int g = 0; g < gateSize("out"); g++)
            if (gate("out", g)->isConnected()) { bestGate = g; break; }
    }
    return bestGate;
}

// ---------------------------------------------------------------------------
// sendSubtasks — node 0 distributes work
// ---------------------------------------------------------------------------
void Client::sendSubtasks() {
    int arrSize = totalSubtasks * 2;
    std::vector<int> arr(arrSize);
    for (int i = 0; i < arrSize; i++) arr[i] = intuniform(1, 100);

    EV << "Node 0 array: ";
    for (int v : arr) EV << v << " ";
    EV << "\n";
    log("Node 0 generated array for task distribution.");

    int chunkSize = arrSize / totalSubtasks;

    for (int i = 0; i < totalSubtasks; i++) {
        int tgt   = i % N;
        int start = i * chunkSize;
        int end   = (i == totalSubtasks - 1) ? arrSize : start + chunkSize;

        EV << "Dispatching subtask " << i << " to client " << tgt << "\n";

        if (tgt == id) {
            // Self-subtask: compute inline, inject Result via scheduleAt
            auto chunk = std::vector<int>(arr.begin() + start, arr.begin() + end);
            int localMax = *std::max_element(chunk.begin(), chunk.end());

            std::ostringstream oss;
            oss << "Node " << id << " processed subtask (self) | local max = " << localMax;
            EV << oss.str() << "\n";
            log(oss.str());

            TaskMessage *res = new TaskMessage("Result");
            res->type = 1; res->src = id; res->target = id; res->result = localMax;
            scheduleAt(simTime(), res);
        } else {
            TaskMessage *msg = new TaskMessage("Subtask");
            msg->type = 0; msg->src = id; msg->target = tgt;
            msg->data = std::vector<int>(arr.begin() + start, arr.begin() + end);
            routeMessage(msg);
        }
    }
}

// ---------------------------------------------------------------------------
// handleMessage
// ---------------------------------------------------------------------------
void Client::handleMessage(cMessage *m) {
    TaskMessage *msg = check_and_cast<TaskMessage *>(m);

    // ---- GOSSIP ------------------------------------------------------------
    if (msg->type == 2) {
        if (seenGossip.count(msg->gossipId)) { delete msg; return; }
        seenGossip.insert(msg->gossipId);

        // Find which OUTPUT gate leads back to the sender, so we exclude it
        // from the flood (avoids echoing back the way the message came).
        int excludeOutGate = -1;
        cGate *arrGate = msg->getArrivalGate();   // the input gate we received on
        if (arrGate && arrGate->isConnected()) {
            // The sender is the module on the other end of the input gate's path
            cModule *sender = arrGate->getPathStartGate()->getOwnerModule();
            for (int g = 0; g < gateSize("out"); g++) {
                const cGate *og = gate("out", g);
                if (og->isConnected() &&
                    og->getPathEndGate()->getOwnerModule() == sender) {
                    excludeOutGate = g;
                    break;
                }
            }
        }

        int originId = std::stoi(msg->gossipId.substr(0, msg->gossipId.find(':')));
        gossipOriginatorsSeen.insert(originId);

        std::ostringstream oss;
        oss << "[GOSSIP] Node " << id
            << " | Time: " << simTime()
            << " | From: Node " << originId
            << " | Msg: " << msg->gossipMsg;
        EV << oss.str() << "\n";
        log(oss.str());

        if ((int)gossipOriginatorsSeen.size() == N) {
            std::string term = "Node " + std::to_string(id) +
                               " received gossip from all " + std::to_string(N) +
                               " clients. Terminating.";
            EV << term << "\n";
            log(term);
            delete msg;
            endSimulation();
            return;
        }

        floodGossip(msg, excludeOutGate);
        delete msg;
        return;
    }

    // ---- SUBTASK / RESULT --------------------------------------------------
    if (msg->target == id) {
        if (msg->type == 0) {
            int localMax = *std::max_element(msg->data.begin(), msg->data.end());
            std::ostringstream oss;
            oss << "Node " << id << " processed subtask | local max = " << localMax;
            EV << oss.str() << "\n";
            log(oss.str());

            TaskMessage *res = new TaskMessage("Result");
            res->type = 1; res->src = id; res->target = msg->src; res->result = localMax;
            routeMessage(res);
        }
        else if (msg->type == 1) {
            subtaskResults.push_back(msg->result);
            receivedResults++;

            std::ostringstream oss;
            oss << "Node " << id << " received subtask result: " << msg->result
                << " (" << receivedResults << "/" << totalSubtasks << ")";
            EV << oss.str() << "\n";
            log(oss.str());

            if (receivedResults == totalSubtasks) {
                int globalMax = *std::max_element(subtaskResults.begin(), subtaskResults.end());
                std::ostringstream fin;
                fin << "=== FINAL MAX (Node " << id << "): " << globalMax << " ===";
                EV << fin.str() << "\n";
                log(fin.str());
                sendGossip();
            }
        }
        delete msg;
    }
    else {
        routeMessage(msg);
    }
}

// ---------------------------------------------------------------------------
// routeMessage
// ---------------------------------------------------------------------------
void Client::routeMessage(TaskMessage *msg) {
    int g = bestGateTowards(msg->target);
    if (g < 0) {
        EV << "Node " << id << ": no route to " << msg->target << ", dropping.\n";
        delete msg;
        return;
    }
    send(msg, "out", g);
}

// ---------------------------------------------------------------------------
// sendGossip — flood completion gossip to all neighbours
// ---------------------------------------------------------------------------
void Client::sendGossip() {
    std::ostringstream msgStr;
    msgStr << simTime().dbl() << ":" << id << ":Client" << id << "#";
    std::string gId = std::to_string(id) + ":1";

    seenGossip.insert(gId);
    gossipOriginatorsSeen.insert(id);   // count our own gossip

    for (int g = 0; g < gateSize("out"); g++) {
        const cGate *og = gate("out", g);
        if (!og->isConnected()) continue;
        TaskMessage *msg = new TaskMessage("Gossip");
        msg->type = 2; msg->src = id; msg->target = -1;
        msg->gossipId = gId; msg->gossipMsg = msgStr.str();
        send(msg, "out", g);
    }

    std::ostringstream oss;
    oss << "Node " << id << " sent gossip: " << msgStr.str();
    EV << oss.str() << "\n";
    log(oss.str());
}

// ---------------------------------------------------------------------------
// floodGossip — forward to all gates except the one the message came from
// ---------------------------------------------------------------------------
void Client::floodGossip(TaskMessage *msg, int excludeOutGate) {
    for (int g = 0; g < gateSize("out"); g++) {
        if (g == excludeOutGate) continue;
        const cGate *og = gate("out", g);
        if (!og->isConnected()) continue;
        send(msg->dup(), "out", g);   // dup() returns TaskMessage* (our override)
    }
}

// ---------------------------------------------------------------------------
// log — write to outputfile.txt (node 0 only)
// ---------------------------------------------------------------------------
void Client::log(const std::string &line) {
    if (outfile.is_open()) {
        outfile << line << "\n";
        outfile.flush();
    }
}
