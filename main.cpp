#include "peer.hpp"
#include "membership.hpp"

int main(int argc, char* argv[]) {
    // process cli arguments
    std::string hostsfile;
    int d, c = 0;
    for (int i = 1; i < argc; i += 2) {
        std::string arg(argv[i]);
        if (arg == "-h")
            hostsfile = argv[i+1];
        else if (arg == "-d")
            d = std::stoi(argv[i+1]);
        else if (arg == "-c")
            c = std::stoi(argv[i+1]);
    }

    // configure hosts and their ids
    readHostsfile(hostsfile);
    configurePeers(hosts);

    // prepare TCP socket and listen for incoming connection requests from peers
    int tcp_sockfd = setupSocketTCP();
    std::thread connectionListnerTCP(handleConnectionsTCP, tcp_sockfd);

    // initial delay to allow all processes start
    std::this_thread::sleep_for(std::chrono::seconds(INITIAL_DELAY));

    // connect to other peers TCP
    for (size_t i = 0; i < hosts.size(); i++) {
        int id = i + 1;
        if (id != own_id){
            int peer_sockfd = connectToPeerTCP(hosts[i]);
            if (peer_sockfd != -1) {
                peers[id].outgoing_sockfd = peer_sockfd;
            }
        }
    }


    // membership protocol delay
    std::this_thread::sleep_for(std::chrono::seconds(d+5));

    std::thread membershipListenerTCP(processIncomingMessagesTCP);

    joinGroup();

    std::this_thread::sleep_for(std::chrono::seconds(10));

    // prepare UDP socket for failure detection simulation
    // int udp_sockfd = setupSocketUDP();
    // std::thread messageRecieverUDP(receiveMessagesUDP, udp_sockfd);

    // std::this_thread::sleep_for(std::chrono::seconds(5));

    // std::thread heartBeatSender(sendHeartbeat, udp_sockfd);
    // std::thread failureChecker(checkFailures);
    
    // crash if c defined
    if (c != 0){
        std::this_thread::sleep_for(std::chrono::seconds(c));
        std::cerr << "{peer_id: " << own_id << ", view_id: " << view_id
                  << ", leader: " << leader_id << " message: \"crashing\"";
        exit(1);
    }

    connectionListnerTCP.join();
    // messageRecieverUDP.join();
    membershipListenerTCP.join();
    // heartBeatSender.join();
    // failureChecker.join();
    return 0;
}