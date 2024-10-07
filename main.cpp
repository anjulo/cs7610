#include "process.hpp"
#include "snapshot.hpp"

int main(int argc, char* argv[]) {
    // process cli arguments
    std::string hostsfile;
    int snapshot_id;
    for (int i = 1; i < argc; i += 2) {
        std::string arg(argv[i]);
        if (arg == "-h")
            hostsfile = argv[i + 1];
        else if (arg == "-t")
            token_delay = std::stof(argv[i + 1]);
        else if (arg == "-m")
            marker_delay = std::stof(argv[i + 1]);
        else if (arg == "-s")
            snapshot_state = std::stoi(argv[i + 1]);
        else if (arg == "-p")
            snapshot_id = std::stoi(argv[i + 1]);
        else if (arg == "-x")
            has_token.store(true);
    }

    // Configure hosts and their ids
    std::vector<std::string> hosts = readHostsfile(hostsfile);
    configurePeers(hosts);
    std::cerr << "{proc_id: " << own_id << ", state: " << state << ", predecessor: " << pre_id << ", successor: " << suc_id << "}" << std::endl;

    // prepare socket and listen for incoming connection requests from peers with lower IDs
    int sockfd = initializeListener();
    std::thread serverThread(handleConnections, sockfd);


    // Initial delay to allow all processes start
    std::this_thread::sleep_for(std::chrono::seconds(INITIAL_DELAY));

    // Connect to peers with higher IDs
    for (size_t i = 0; i < hosts.size(); i++) {
        int id = i + 1;
        if (id != own_id){
            int peer_sockfd = connectToPeer(hosts[i], PORT);
            if (peer_sockfd != -1)
                peers[id].outgoing_sockfd = peer_sockfd;
        }
        
    }

    // recieve and process token
    std::thread receiveThread(receiveMessages);
    std::thread processThread(processToken, snapshot_id);

    // join
    serverThread.join();
    receiveThread.join();
    processThread.join();

    return 0;
}