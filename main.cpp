#include "peer.hpp"
#include "paxos.hpp"

char value = '\0';

int main(int argc, char* argv[]) {
    // process cli arguments
    std::string hostsfile;
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

    // std::cerr << "value: " << value << std::endl;

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

    std::this_thread::sleep_for(std::chrono::seconds(INITIAL_DELAY));

    if (own_id == 5)
        std::this_thread::sleep_for(std::chrono::seconds(t));
    if (value != '\0')
        preparePaxos();

    if (listenerThread.joinable()) listenerThread.join();

    return 0;
}