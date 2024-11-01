#include "peer.hpp"
#include "paxos.hpp"

char value = '\0';

int main(int argc, char* argv[]) {
    // process cli arguments
    std::string hostsfile;
    // std::string v;
    int t = 0;
    for (int i = 1; i < argc; i += 2) {
        std::string arg(argv[i]);
        if (arg == "-h")
            hostsfile = argv[i+1];
        else if (arg == "-v")
            value = *argv[i+1];
        else if (arg == "-t")
            t = std::stoi(argv[i+1]);
    }

    // configure hosts and their ids
    readHostsFile(hostsfile);

    // prepare TCP
    int tcp_sockfd = setupSocketTCP();
    std::thread listenerThread(receiveAllMessages, tcp_sockfd);

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

    // std::this_thread::sleep_for(std::chrono::seconds(10));
    // for (const auto& pair : peers) {
    //     const auto& peer = pair.second;
    //     std:: cerr << pair.first << " " << (peer.hostname.empty() ? "N/A" : peer.hostname) << " " << peer.role << " " 
    //                << peer.incoming_sockfd << " " << peer.outgoing_sockfd << std::endl;
    // }
    std::this_thread::sleep_for(std::chrono::seconds(10));

    if (value != '\0')
        preparePaxos();

    if (listenerThread.joinable()) listenerThread.join();

    return 0;
}