#include "peer.hpp"

int own_id;
std::string own_hostname;
std::string own_role;


std::unordered_map<int, Peer> peers;
std::vector<std::string> hosts;
std::unordered_map<std::string, std::vector<int>> acceptors;


void readHostsFile(const std::string& filename) {
    char own_hostname[32];
    gethostname(own_hostname, sizeof(own_hostname));
    std::ifstream file(filename);
    std::string line;
    int id = 0;
    while (std::getline(file, line)) {
        id++;
        size_t colon = line.find(':');
        std::string hostname = line.substr(0, colon);
        hosts.push_back(hostname);
        std::string roles_str = line.substr(colon + 1);
        if (hostname == own_hostname) {
            own_id = id;
            if (roles_str == "proposer1" || roles_str == "proposer2")
                own_role = roles_str;
            continue;
        }
        
        int pos = 0;
        std::string role;
        while ((pos = roles_str.find(',')) != std::string::npos) {
            role = roles_str.substr(0, pos);
            if (role == "acceptor1")
                acceptors["proposer1"].push_back(id);
            else if (role == "acceptor2")
                acceptors["proposer2"].push_back(id);
            roles_str.erase(0, pos + 1);
        }
        // last role
        if (!roles_str.empty()) {
            if (roles_str == "acceptor1")
                acceptors["proposer1"].push_back(id);
            else if (roles_str == "acceptor2")
                acceptors["proposer2"].push_back(id);
        }
        peers[id] = {hostname, -1, -1};
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
            std::cerr << "client: connect" << strerror(errno) << std::endl;
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

void handleTCPConnection(int tcp_sockfd) {
    struct sockaddr_storage peeraddr;
    socklen_t peeraddr_len = sizeof(peeraddr);
    int new_sockfd = accept(tcp_sockfd, (struct sockaddr*)&peeraddr, &peeraddr_len);
    
    if (new_sockfd < 0) {
        std::cerr << "accept: " << strerror(errno) << std::endl;
        return;
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

void receiveAllMessages(int tcp_sockfd) {

    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(tcp_sockfd, &readfds);
        int maxFd = tcp_sockfd;
        
        for (const auto& peer : peers) {
            if (peer.second.incoming_sockfd > 0) {
                FD_SET(peer.second.incoming_sockfd, &readfds);
                maxFd = std::max(maxFd, peer.second.incoming_sockfd);
            }
        } 

        int rv = select(maxFd + 1, &readfds, NULL, NULL, &tv);
        
        if (rv < 0) {
            std::cerr << "select: " << strerror(errno) << std::endl;
            continue;
        }

        if(FD_ISSET(tcp_sockfd, &readfds)) handleTCPConnection(tcp_sockfd);

        for (auto &peer : peers) {
            if (FD_ISSET(peer.second.incoming_sockfd, &readfds)){
                receivePaxosMessage(peer.second.incoming_sockfd, peer.first);
            }
        }
    }
}