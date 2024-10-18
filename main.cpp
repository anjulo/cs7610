#include "peer.hpp"
#include "membership.hpp"

int delay;
int main(int argc, char* argv[]) {
    // process cli arguments
    std::string hostsfile;
    int snapshot_id;
    for (int i = 1; i < argc; i += 2) {
        std::string arg(argv[i]);
        if (arg == "-h")
            hostsfile = argv[i + 1];
        else if (arg == "-d")
            delay = std::stof(argv[i + 1]);
    }

    // configure hosts and their ids
    std::vector<std::string> hosts = readHostsfile(hostsfile);
    configurePeers(hosts);
    std::cerr << "peer_id: " << own_id << std::endl;


    // prepare socket and listen for incoming connection requests from peers with lower IDs
    int sockfd = initializeConnectionListener();
    std::thread connectionListnerThread(handleIncomingConnections, sockfd);


    // initial delay to allow all processes start
    std::this_thread::sleep_for(std::chrono::seconds(INITIAL_DELAY));

    // connect to other peers
    for (size_t i = 0; i < hosts.size(); i++) {
        int id = i + 1;
        if (id != own_id){
            int peer_sockfd = connectToPeer(hosts[i], PORT);
            if (peer_sockfd != -1) {
                peers[id].outgoing_sockfd = peer_sockfd;
            }
        }
    }



    // membership protocol delay
    std::this_thread::sleep_for(std::chrono::seconds(delay));

    std::thread membershipListenerThread(processIncomingMessages);
    // sleep for peers to peers to join in-order
    if (own_id != leader_id) {
        std::this_thread::sleep_for(std::chrono::seconds(own_id * 5 - 5));
    }

    joinGroup();

    membershipListenerThread.join();
    return 0;
}