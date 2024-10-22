#include "peer.hpp"
#include "membership.hpp"

int own_id;
std::string own_hostname;

std::map<int, Peer> peers;
std::vector<std::string> hosts;

void readHostsfile(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        hosts.push_back(line);
    }
}

void configurePeers(std::vector<std::string> hosts) {
    char own_hostname_[10];
    gethostname(own_hostname_, sizeof(own_hostname_));
    own_hostname = own_hostname_;

    for (size_t i = 0; i < hosts.size(); i++) {
        int id = i + 1;
        if (hosts[i] == own_hostname) {
            own_id = id;
        } else {
            peers[id] = {hosts[i], -1, -1};
        }
    }
}

int setupSocketTCP() {
    // prepare socket to listen to connection from servers
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

void handleConnectionsTCP(int server_sockfd) {
    while (!should_exit.load()) {
        struct sockaddr_storage peeraddr;
        socklen_t peeraddr_len = sizeof(peeraddr);
        int new_sockfd = accept(server_sockfd, (struct sockaddr*)&peeraddr, &peeraddr_len);
        
        if (new_sockfd < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        char hostname_[NI_MAXHOST];
        getnameinfo((struct sockaddr*)&peeraddr, peeraddr_len, hostname_, NI_MAXHOST, NULL, 0, NI_NOFQDN);
        std::string hostname(hostname_);
        
        for (auto& peer : peers) {
            if (peer.second.hostname == hostname.substr(0, hostname.find('.'))) {
                peer.second.incoming_sockfd = new_sockfd;
                break;
            }
        }
    }
}

int connectToPeerTCP(const std::string& hostname) {
    int sockfd, rv;
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname.c_str(), PORT, &hints, &res)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return -1;
    }

    for(p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            std::cerr << "cleint: socket" << strerror(errno) <<std::endl;
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            std::cerr <<"client: connect" << strerror(errno) << std::endl;
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(res);
    
    if (p == NULL) {
        std::cerr << "Failed to connect to " << hostname << std::endl;
        return -1;
    }

    return sockfd;
}

int setupSocketUDP() {
    // networking
    struct addrinfo hints, *res, *p;
    int sockfd, rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0){
        std::cerr << "setup UDP socket - getaddrinfo: " << gai_strerror(rv) << std::endl;
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0){
            std::cerr << "udp socket: " << strerror(errno) << std::endl;
            continue;
        }

        if(bind(sockfd, res->ai_addr, res->ai_addrlen) < 0){
            std::cerr << "udp bind: " << strerror(errno) << std::endl;
            close(sockfd);
            continue;
        } 
        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        std::cerr << "server: failed to bind" << std::endl;
        return -1;
    }
    
    return sockfd;

}

void sendMessageUDP(int sockfd, const std::string& dst_host, const char* message) {
    struct addrinfo hints, *res;
    int rv, n;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(dst_host.c_str(), PORT, &hints, &res)) != 0) {
        // std::cerr << "sendMessageUDP - getaddrinfo: " << dst_host << " - " << gai_strerror(rv) << std::endl;
        return;
    }

    if ((n = sendto(sockfd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen)) == -1) {
        std::cerr << "sendto: " << dst_host << " - " << strerror(errno);
    }

    freeaddrinfo(res);
}


void receiveAllMessages(int udp_sockfd) {
    while (!should_exit.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(udp_sockfd, &readfds);
        int maxFd = udp_sockfd;
        
        for (const auto& peer : peers) {
            FD_SET(peer.second.incoming_sockfd, &readfds);
            maxFd = std::max(maxFd, peer.second.incoming_sockfd);
        } 

        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int rv = select(maxFd + 1, &readfds, NULL, NULL, &tv);
        
        if (rv < 0) {
            std::cerr << "select: " << strerror(errno) << std::endl;
            continue;
        }

        if (FD_ISSET(udp_sockfd, &readfds)) {
            handleUDPMessage(udp_sockfd);
        }

        for (auto &peer : peers) {
            if (FD_ISSET(peer.second.incoming_sockfd, &readfds)){
                handleTCPMessage(peer.second.incoming_sockfd, peer.first);
            }
        }
    }
}