// client.ned
// Module definition for a Peer-to-Peer client node.
// Gates are declared as dynamic arrays (in[], out[]) so that
// the ring + Chord finger connections can be wired in network.ned.

simple Client
{
    parameters:
        int id;            // unique node identifier (set to module index in omnetpp.ini)
        int N;             // total number of clients in the network
        int totalSubtasks; // number of subtasks the originator distributes

    gates:
        input  in[];
        output out[];
}
