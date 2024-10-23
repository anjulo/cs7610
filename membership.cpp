#include "membership.hpp"
#include "peer.hpp"

#include <sstream>

int leader_id = 1;
int view_id = 1;
int request_id = 1;
std::vector<int> memb_list;
std::map<std::pair<int, int>, int> oks_recieved;
std::map<std::pair<int, int>, PendingOperation> pending_operations;
// failure detector
std::unordered_map<int, std::chrono::steady_clock::time_point> last_heartbeat;
std::mutex heartbeat_mutex;


void printMessage(const Message& msg) {
    std::cerr << "Message{";
    
    std::cerr << "type: ";

    switch (msg.type) {
        case Message::JOIN: std::cerr << "JOIN"; break;
        case Message::REQ: std::cerr << "REQ"; break;
        case Message::OK: std::cerr << "OK"; break;
        case Message::NEWVIEW: std::cerr << "NEWVIEW"; break;
        default: std::cerr << "UNKNOWN"; break;
    }
    
    if (msg.request_id > 0) std::cerr << ", req_id: " << msg.request_id;
    if (msg.view_id > 0) std::cerr << ", view_id: " << msg.view_id;
    if (msg.peer_id > 0) std::cerr << ", peer_id: " << msg.peer_id;
    if (msg.sender_id > 0) std::cerr << ", sender_id: " << msg.sender_id;
    
    // Print Operation only for NEWVIEW messages
    if (msg.type == Message::NEWVIEW) {
        std::cerr << ", memb_list: [";
        for (size_t i = 0; i < msg.memb_list.size(); ++i) {
            if (i > 0) std::cerr << ",";
            std::cerr << msg.memb_list[i];
        }
        std::cerr << "]";
    }


    // Print Operation only for REQ messages
    if (msg.type == Message::REQ) {
        std::cerr << ", op: ";
        switch (msg.operation) {
            case Operation::ADD: std::cerr << "ADD"; break;
            case Operation::DEL: std::cerr << "DEL"; break;
            default: std::cerr << "UNKNOWN"; break;
        }
    }
    
    std::cerr << "}" << std::endl;
}

void sendMessage(int sockfd, const Message& msg, int dest_id) {
    std::vector<int> buffer;

    buffer.push_back(static_cast<int>(msg.type));
    buffer.push_back(msg.request_id);
    buffer.push_back(msg.view_id);
    buffer.push_back(msg.peer_id);
    buffer.push_back(msg.sender_id);


    if (msg.type == Message::NEWVIEW) {
        buffer.push_back(msg.memb_list.size());
        buffer.insert(buffer.end(), msg.memb_list.begin(), msg.memb_list.end());
    }
    
    if (msg.type == Message::REQ) {
        buffer.push_back(static_cast<int>(msg.operation));
    }

    int bytes_sent;
    if ((bytes_sent = send(sockfd, buffer.data(), buffer.size() * sizeof(int), 0)) < 0){
        std::cerr << "send to " << dest_id << " : " << strerror(errno) << std::endl;
    }
}

Message receiveMessage(int sockfd, int src_id) {
    int buffer[32 * sizeof(int)];
    int bytes_recieved;
    bytes_recieved = recv(sockfd, buffer, sizeof(buffer), MSG_PEEK);
    if (bytes_recieved <= 0) return Message{Message::UNKNOWN};

    bytes_recieved = recv(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_recieved < (5 * sizeof(int))) return Message{Message::UNKNOWN};

    int elements_read = bytes_recieved / sizeof(int);
    int index = 0;

    Message msg;
    msg.type = static_cast<Message::Type>(buffer[index++]);
    msg.request_id = buffer[index++];
    msg.view_id = buffer[index++];
    msg.peer_id = buffer[index++];
    msg.sender_id = buffer[index++];

    // validate socker not closed on the other side
    if (msg.type < Message::JOIN || msg.type > Message::NEWVIEW) return Message{Message::UNKNOWN};

    if (msg.type == Message::NEWVIEW) { //&& index < elements_read)
        int list_size = buffer[index++];
        for (int i = 0; i < list_size && index < elements_read; i++) {
            msg.memb_list.push_back(buffer[index++]);
        }
    }

    if (msg.type == Message::REQ) {
        msg.operation = static_cast<Operation>(buffer[index++]);
    }

    return msg;

}

void printNewView() {
    std::cerr << "{peer_id: " << own_id  << ", view_id: " << view_id
              << ", leader: " << leader_id << ", memb_list: [";
    
    for (size_t i = 0; i < memb_list.size(); i++) {
        std::cerr << memb_list[i];
        if (i < memb_list.size() - 1) {
            std::cerr << ", ";
        }
    }

    std::cerr << "]}" << std::endl;
}

void joinGroup() {
    if (own_id == leader_id) {
        view_id = 1;
        memb_list.push_back(own_id);
        printNewView();
    } else {
        Message join_msg{Message::JOIN, -1, -1, own_id, own_id};
        sendMessage(peers[leader_id].outgoing_sockfd, join_msg, leader_id);
    }
}

void handleJoinMessage(const Message& msg) {
    if (own_id != leader_id) return;

    if (memb_list.size() == 1 && memb_list[0] == leader_id) {
        view_id++;
        memb_list.push_back(msg.sender_id);
        printNewView();
        Message newview_msg{Message::NEWVIEW, request_id++, view_id, -1, -1, memb_list};
        sendMessage(peers[msg.sender_id].outgoing_sockfd, newview_msg, msg.sender_id);
        return;
    }

    Message req_msg{Message::REQ, request_id, view_id, msg.sender_id, own_id, {}, Operation::ADD};
    for (int id : memb_list){
        if (id != own_id) {
            sendMessage(peers[id].outgoing_sockfd, req_msg, id);
        }
    }
    pending_operations.insert({{request_id, view_id}, {request_id, view_id, msg.sender_id, Operation::ADD}});
    oks_recieved[{request_id, view_id}] = 0;
    request_id++;

}

void handleReqMessage(const Message& msg) {
    if (own_id == leader_id) return;

    pending_operations.insert({{msg.request_id, msg.view_id}, {msg.request_id, msg.view_id, msg.peer_id, msg.operation}});

    Message ok_msg{Message::OK, msg.request_id, msg.view_id};
    sendMessage(peers[leader_id].outgoing_sockfd, ok_msg, leader_id);
}

void handleOkMessage(const Message& msg) {
    if (own_id != leader_id) return;
    
    oks_recieved[{msg.request_id, msg.view_id}]++;
    const auto &op = pending_operations[{msg.request_id, msg.view_id}];

    if (oks_recieved[{msg.request_id, msg.view_id}] >= memb_list.size() - 1) {
        view_id++;
        if (op.type == Operation::ADD){
            memb_list.push_back(op.peer_id);
        } 
        
        printNewView();
        Message newview_msg{Message::NEWVIEW, -1, view_id, -1, -1, memb_list};

        for (int id : memb_list) {
            if (id != own_id)
                sendMessage(peers[id].outgoing_sockfd, newview_msg, id);
        }
        
        // remove pending operation
        oks_recieved.erase({msg.request_id, msg.view_id});
        pending_operations.erase({msg.request_id, msg.view_id});
        
    }

}

void handleNewViewMessage(const Message& msg) {
    view_id = msg.view_id;
    memb_list = msg.memb_list;
    printNewView();

}
    

void sendHeartbeat(int sockfd) {
    while(!should_exit.load()) {
        for (int id : memb_list) {
            if (id != own_id) {
                sendMessageUDP(sockfd, hosts[id-1], HEARTBEAT_MESSAGE);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
    }
    // std::cerr << "exiting sendHeartbeat" << std::endl;
}

void handlePeerFailure(int failed_peer_id) {

    auto it = std::find(memb_list.begin(), memb_list.end(), failed_peer_id);
    if (it == memb_list.end()) return;
    else memb_list.erase(it);

    // if leader is the only peer left
    if (memb_list.size() == 1 && memb_list[0] == leader_id) {
        view_id++;
        printNewView();
    } else {
        Message req_msg{Message::REQ, request_id, view_id, failed_peer_id, own_id, {}, Operation::DEL};

        pending_operations.insert({{request_id, view_id}, {request_id, view_id, failed_peer_id, Operation::DEL}});
        oks_recieved[{request_id, view_id}] = 0;
        request_id++;

        for (int id : memb_list) {
            if (id != own_id && id != failed_peer_id)
                sendMessage(peers[id].outgoing_sockfd, req_msg, id);
        }

    }


}
void checkFailures() {
    while(!should_exit.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(2 * HEARTBEAT_INTERVAL));
        std::lock_guard<std::mutex> lock(heartbeat_mutex);
        auto now = std::chrono::steady_clock::now();

        for  (int id : memb_list) {
            if (id == own_id) continue;

            if (last_heartbeat.find(id) == last_heartbeat.end() || 
                std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat[id]).count() > (2 * HEARTBEAT_INTERVAL)) {

                    std::cerr << "{peer_id: " << own_id << ", view_id: " << view_id
                              << ", leader: " << leader_id << ", message: \"peer " << id;
                    
                    if (id == leader_id) std::cerr << " (leader)";
                    std::cerr << " unreachable\"" << std::endl;

                    if (own_id == leader_id) {
                        handlePeerFailure(id);
                    }
                    last_heartbeat.erase(id);
                }
        }
    }
}

void handleTCPMessage(int sockfd, int src_id) {
    Message msg = receiveMessage(sockfd, src_id);
    switch (msg.type) {
        case Message::JOIN:
            handleJoinMessage(msg);
            break;
        case Message::REQ:
            handleReqMessage(msg);
            break;
        case Message::OK:
            handleOkMessage(msg);
            break;
        case Message::NEWVIEW:
            handleNewViewMessage(msg);
            break;
        default:
            break;
    }
}

void handleUDPMessage(int udp_sockfd) {
    char buffer[15 * sizeof(char)];
    struct sockaddr_in peeraddr;
    socklen_t addrlen = sizeof(peeraddr);
    int n;

    if ((n = recvfrom(udp_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peeraddr, &addrlen)) < 0) {
        std::cerr << "recvfrom: " << strerror(errno) << std::endl;
        return;
    } 

    buffer[n] = '\0';
    char peer_name_[NI_MAXHOST];
    getnameinfo((struct sockaddr *)&peeraddr, addrlen, peer_name_, NI_MAXHOST, NULL, 0, 0);
    std::string peer_name(peer_name_);


    auto it = std::find(hosts.begin(), hosts.end(), peer_name.substr(0, peer_name.find('.')));
    if (it != hosts.end()) {
        int peer_id = it - hosts.begin() + 1;
        if (strcmp(buffer, HEARTBEAT_MESSAGE) == 0) {
            std::lock_guard<std::mutex> lock(heartbeat_mutex);
            last_heartbeat[peer_id] = std::chrono::steady_clock::now();
        }
    }
}
