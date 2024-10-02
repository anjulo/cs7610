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
#include <queue>

#define PORT "3490"
#define INITIAL_DELAY 5 // seconds
#define BACKLOG 5

int own_id;
int predecessor_id;
int successor_id;
int state = 0;
const int TOKEN = 1;
std::atomic<bool> has_token(false);

struct Peer {
    std::string hostname;
    int incoming_sockfd;
    int outgoing_sockfd;
};
std::map<int, Peer> peers;

const int MARKER = 2;
struct Snapshot {
    int id;
    bool active;
    int state;
    bool has_token;
    std::map<int, bool> channel_recording;
    std::map<int, std::vector<int>> channel_state;
};

Snapshot current_snapshot = {-1, false};

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

    predecessor_id = (own_id - 1 + hosts.size()) % hosts.size();
    successor_id = own_id % hosts.size() + 1;

    std::cerr << "{proc_id: " << own_id << ", state: " << state << ", predecessor: " << predecessor_id << ", successor: " << successor_id << "}" << std::endl;
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

void handleConnections(int server_sockfd) {
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

// handles start snapshotting for all processes(including the initator)
void startSnapshot(int snapshot_id, int marker_sender_id = -1){

    // log snapshot started
    std::cerr << "{proc_id: " << own_id  << ", snapshot_id: " << snapshot_id 
              << ", snapshot: \"started\"" << std::endl;

    // 1. record local state
    current_snapshot = {snapshot_id, true, state, has_token.load(), {}, {}};

    // 2. send markers on all outgoing channels
    for (const auto& peer : peers){
        std::cerr << "{proc_id: " << own_id << ", snapshot_id: " << snapshot_id 
              << ", sender: " << own_id << ", receiver: " << peer.first 
              << ", msg: \"marker\", state: " << state << ", has_token: " 
              << (current_snapshot.has_token ? "YES" : "NO") << "}" << std::endl;

        int message[2] = {MARKER, snapshot_id};
        send(peers[peer.first].outgoing_sockfd, message, sizeof(message), 0);
    }

    // 3. start recording on all incoming channels(except for the channel the marker came on)
    for(const auto& peer : peers){
        if(peer.first == marker_sender_id)
            current_snapshot.channel_recording[peer.first] = false;
        else {
            current_snapshot.channel_recording[peer.first] = true;
            current_snapshot.channel_state[peer.first] = {};
        }
    }
}

void handleMarker(int sender_id, int snapshot_id, float marker_delay){
    
    if(!current_snapshot.active){ // / first time receiving the marker
        std::this_thread::sleep_for(std::chrono::duration<float>(marker_delay));
        startSnapshot(snapshot_id, sender_id);
    } else {  // not the first time receiving the marker
        // stop recording on channel C_ki
        current_snapshot.channel_recording[sender_id] = false;

        // log channel closed
        std::cerr << "{proc_id: " << own_id << ":, snapshot_id: " << snapshot_id
                  << ", snapshot: " << "channel closed, " 
                  << "channel: " << sender_id << "-" << own_id
                  << ", queue: [";
        for(size_t i = 0; i < current_snapshot.channel_state[sender_id].size(); i++){
            if(i > 0) std::cerr << ",";
            std::cerr << current_snapshot.channel_state[sender_id][i];
        }
        std::cerr << "]}" << std::endl; 


        // if all channels closed, finish the snapshot
        auto isFalse = [](const auto& pair) { return !pair.second; };
        if (std::all_of(current_snapshot.channel_recording.begin(), current_snapshot.channel_recording.end(), isFalse)) {
            std::cerr << "{proc_id:" << own_id << ", snapshot_id: " << current_snapshot.id 
                      << ", snapshot: \"finished\"}" << std::endl;
            current_snapshot.active = false;
        }
    }
}


void processToken(float token_delay, int snapshot_state, int snapshot_id) {
    while (true) {
        if (has_token.load()) {
            state++;
            std::cerr << "{proc_id: " << own_id << ", state: " << state << "}" << std::endl;

            if(state == snapshot_state)
                startSnapshot(snapshot_id);
            
            std::this_thread::sleep_for(std::chrono::duration<float>(token_delay));
            
            std::cerr << "{proc_id: " << own_id << ", sender: " << own_id << ", receiver: " << successor_id << ", message: \"token\"" << "}" << std::endl;
            int message[2] = {TOKEN, 0};
            send(peers[successor_id].outgoing_sockfd, message, sizeof(message), 0);
            
            has_token.store(false);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void receiveMessages(float marker_delay) {
    // beej's guid: ch. 7
    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxFd = -1;
        
        for (const auto& pair : peers) {
            FD_SET(pair.second.incoming_sockfd, &readfds);
            maxFd = std::max(maxFd, pair.second.incoming_sockfd);
        }
        
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int rv = select(maxFd + 1, &readfds, NULL, NULL, &tv);
        
        if (rv < 0) {
            std::cerr << "Select error" << std::endl;
            continue;
        }
        
        for (const auto& peer : peers) {
            if (FD_ISSET(peer.second.incoming_sockfd, &readfds)) {
                int message[2];
                ssize_t bytes_read = recv(peer.second.incoming_sockfd, message, sizeof(message), 0);
                if (bytes_read == sizeof(message)) {
                    if(message[0] == TOKEN){
                        std::cerr << "{proc_id: " << own_id << ", sender: " << peer.first 
                                  << ", receiver: " << own_id << ", message: \"token\"" <<  "}"
                                  << std::endl;
                        if(current_snapshot.active && current_snapshot.channel_recording[peer.first]){
                            current_snapshot.channel_state[peer.first].push_back(TOKEN);
                        }
                        has_token.store(true);
                    } else if(message[0] = MARKER){
                        handleMarker(peer.first, message[1], marker_delay);
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // process cli arguments
    std::string hostsfile;
    float token_delay, marker_delay;
    int snapshot_state = -1, snapshot_id;
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
    std::thread receiveThread(receiveMessages, marker_delay);
    std::thread processThread(processToken, token_delay, snapshot_state, snapshot_id);

    // join
    serverThread.join();
    receiveThread.join();
    processThread.join();

    return 0;
}