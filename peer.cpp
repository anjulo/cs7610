#include "peer.hpp"

int own_id;

std::map<int, Peer> peers;

std::vector<std::string> readHostsfile(const std::string& filename) {
    std::vector<std::string> hosts;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        hosts.push_back(line);
    }
    return hosts;
}

void configurePeers(std::vector<std::string> hosts) {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    for (size_t i = 0; i < hosts.size(); i++) {
        int id = i + 1;
        if (hosts[i] == hostname) {
            own_id = id;
        } else {
            peers[id] = {hosts[i], -1, -1};
        }
    }
}

int initializeListener() {
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

void handleIncomingConnections(int server_sockfd) {
    while (true) {
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
        
        for (auto& pair : peers) {
            if (pair.second.hostname == hostname.substr(0, hostname.find('.'))) {
                pair.second.incoming_sockfd = new_sockfd;
                break;
            }
        }
    }
}