network P2PNetwork
{
    parameters:
        int N = default(5);

    submodules:
        client[N]: Client;

    connections allowunconnected:
        for i=0..N-1 {
            client[i].out++ --> client[(i+1)%N].in++;
        }
}