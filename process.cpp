#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <map>

#define PORT "3490"
#define INITIAL_DELAY 5 // seconds
#define BACKLOG 5

struct PeerInfo {
    std::string hostname;
    int sockfd;
};

int own_id;
int predecessor_id;
int successor_id;
int state = 0;
std::map<int, PeerInfo> peers;
const int TOKEN = 1;
std::atomic<bool> has_token(false);

std::vector<std::string> readHostsfile(const std::string& filename) {
    std::vector<std::string> hosts;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        hosts.push_back(line);
    }
    return hosts;
}

void configureHosts(std::vector<std::string> hosts) {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    for (size_t i = 0; i < hosts.size(); i++) {
        if (hosts[i] == hostname) {
            own_id = i + 1;
            break;
        }
    }

    predecessor_id = (own_id - 1 + hosts.size()) % hosts.size();
    successor_id = own_id % hosts.size() + 1;

    std::cout << "{proc_id: " << own_id << ", state: " << state << ", predecessor: " << predecessor_id << ", successor: " << successor_id << "}" << std::endl;
}

int initializeServer() {
    // prepare socket for listening as a server
    int sockfd, rv, yes=1;
    struct addrinfo hints, *servinfo, *p;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return 1;
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            std::cerr << "server: socket" << std::endl;
            continue;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
            std::cerr <<"server: setsockopt" << std::endl;
            exit(1);
        }

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            std::cerr << "server: bind" << std::endl;
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);
    
    if(p == NULL){
        std::cerr << "server: failed to bind" << std::endl;
    }

    if (listen(sockfd, BACKLOG) < 0) {
        std::cerr << "Error listening on socket" << std::endl;
        return 1;
    }

    return sockfd;
}

int connectToPeer(const std::string& hostname, const char* port) {
    int peer_sockfd, rv;
    struct addrinfo hints, *peerinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname.c_str(), port, &hints, &peerinfo)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return -1;
    }

    for(p = peerinfo; p != NULL; p = p->ai_next) {
        if ((peer_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            std::cerr << "cleint: socket" << std::endl;
            continue;
        }

        if (connect(peer_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            std::cerr <<"client: connect" << std::endl;
            close(peer_sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        std::cerr << "Failed to connect to " << hostname << std::endl;
        return -1;
    }

    freeaddrinfo(peerinfo);
    return peer_sockfd;
}

void handleConnections(int server_sockfd) {
    while (true) {
        struct sockaddr_storage peeraddr;
        socklen_t peeraddr_len = sizeof(peeraddr);
        int client_sockfd = accept(server_sockfd, (struct sockaddr*)&peeraddr, &peeraddr_len);
        
        if (client_sockfd < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        char hostname_[NI_MAXHOST];
        getnameinfo((struct sockaddr*)&peeraddr, peeraddr_len, hostname_, NI_MAXHOST, NULL, 0, NI_NOFQDN);
        std::string hostname(hostname_);
        
        for (auto& pair : peers) {
            if (pair.second.hostname == hostname.substr(0, hostname.find('.'))) {
                pair.second.sockfd = client_sockfd;
                break;
            }
        }
    }
}

void receiveToken() {
    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxFd = -1;
        
        for (const auto& pair : peers) {
            // if(pair.first != own_id) {
                FD_SET(pair.second.sockfd, &readfds);
                maxFd = std::max(maxFd, pair.second.sockfd);
            // }
        }
        
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int rv = select(maxFd + 1, &readfds, NULL, NULL, &tv);
        
        if (rv < 0) {
            std::cerr << "Select error" << std::endl;
            continue;
        }
        
        for (const auto& pair : peers) {
            if (FD_ISSET(pair.second.sockfd, &readfds)) {
                int recieved_token = 0;
                int bytes_read = recv(pair.second.sockfd, &recieved_token, sizeof(int), 0);
                if (bytes_read == sizeof(int) && recieved_token == TOKEN ) {
                    std::cout << "{proc_id: " << own_id 
                              << ", sender: " << pair.first 
                              << ", receiver: " << own_id 
                              << ", message:"  << recieved_token <<  "}" 
                              << std::endl;
                    has_token.store(true);
                }
            }
        }
    }
}

void processToken(float t) {
    while (true) {
        if (has_token.load()) {
            state++;
            std::cout << "{proc_id: " << own_id << ", state: " << state << "}" << std::endl;
            
            std::this_thread::sleep_for(std::chrono::duration<float>(t));
            
            std::cout << "{proc_id: " << own_id << ", sender: " << own_id << ", receiver: " << successor_id << ", message: " << TOKEN << "}" << std::endl;
            send(peers[successor_id].sockfd, &TOKEN, sizeof(TOKEN), 0);
            
            has_token.store(false);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char* argv[]) {
    // process cli arguments
    if (argc != 5 && argc != 6) {
        std::cerr << "Usage: " << argv[0] << " -h <hostfile> -t <sleep_time> [-x]" << std::endl;
        return 1;
    }

    std::string hostsfile;
    double t;

    for (int i = 1; i < argc; i += 2) {
        std::string arg(argv[i]);
        if (arg == "-h")
            hostsfile = argv[i + 1];
        else if (arg == "-t")
            t = std::stof(argv[i + 1]);
        else if (arg == "-x")
            has_token.store(true);
    }


    // Configure hosts and their ids
    std::vector<std::string> hosts = readHostsfile(hostsfile);
    configureHosts(hosts);

    // prepare socket and listening for incoming connection requests
    int sockfd = initializeServer();
    std::thread serverThread(handleConnections, sockfd);



    // Initial delay to allow all processes start
    std::this_thread::sleep_for(std::chrono::seconds(INITIAL_DELAY));

    // Connect to all other peers as a client
    for (size_t i = 0; i < hosts.size(); i++) {
        int id = i + 1;
        if (id != own_id) {
            int peer_sockfd = connectToPeer(hosts[i], PORT);
            if (peer_sockfd != -1)
                peers[id] = {hosts[i], peer_sockfd};
        }
        
    }



    // recieve and handle token
    std::thread receiveThread(receiveToken);
    std::thread processThread(processToken, t);

    // join
    serverThread.join();
    receiveThread.join();
    processThread.join();

    return 0;
}