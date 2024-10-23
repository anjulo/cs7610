#include "peer.hpp"
#include "membership.hpp"

std::atomic<bool> should_exit(false);
std::vector<std::thread> threads;


void crashGracefully() {
    should_exit.store(true);

    for (auto& thread : threads){
        if (thread.joinable()) thread.join();
    }
    for (auto& peer : peers) {
        if (peer.second.incoming_sockfd != -1) close(peer.second.incoming_sockfd);
        if (peer.second.outgoing_sockfd != -1) close(peer.second.outgoing_sockfd);
    }
    std::cerr << "{peer_id: " << own_id << ", view_id: " << view_id
                << ", leader: " << leader_id.load() << " message: \"crashing\"" 
                << std::endl;
    exit(0);
}
int main(int argc, char* argv[]) {
    // process cli arguments
    std::string hostsfile;
    int d = 0, c = 0, t = 0;
    for (int i = 1; i < argc; i += 2) {
        std::string arg(argv[i]);
        if (arg == "-h")
            hostsfile = argv[i+1];
        else if (arg == "-d")
            d = std::stoi(argv[i+1]);
        else if (arg == "-c")
            c = std::stoi(argv[i+1]);
        else if (arg == "-t")
            t = 1;
    }

    // configure hosts and their ids
    readHostsfile(hostsfile);
    configurePeers(hosts);

    // prepare TCP and UDP socket
    int tcp_sockfd = setupSocketTCP();
    int udp_sockfd = setupSocketUDP();
    threads.push_back(std::thread(receiveAllMessages, tcp_sockfd, udp_sockfd));

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
    std::this_thread::sleep_for(std::chrono::seconds(d));

    joinGroup();

    std::this_thread::sleep_for(std::chrono::seconds(15));

    threads.push_back(std::thread(checkFailures));
    threads.push_back(std::thread(sendHeartbeat, udp_sockfd));

    // crash if c defined
    if (c != 0){
        std::this_thread::sleep_for(std::chrono::seconds(c));
        crashGracefully();
    }
    if (t != 0) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        sendREQBeforeCrash();
        crashGracefully();
    }

    for (auto& thread : threads){
            if (thread.joinable()) thread.join();
    }
    return 0;
}