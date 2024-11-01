#include "paxos.hpp"

int n = -1;
int max_round = 0;

int min_proposal = 0;
int accepted_proposal = 0;
char accepted_value = '\0';
int max_accepted_proposal = 0;

int prepare_acks = 0;
int accept_acks = 0;
char chosen_value = '\0';


void printMessage(Message &msg, int action_flag) {
    std::string mgs_type;
    switch (msg.type) {
        case MessageType::Prepare: mgs_type = "prepare"; break;
        case MessageType::PrepareAck: mgs_type = "prepare_ack"; break;
        case MessageType::Accept: mgs_type = "accept"; break;
        case MessageType::AcceptAck: mgs_type = "accept_ack"; break;
        case MessageType::Chose: mgs_type = "chose"; break;
    }
    std::string action_type;
    switch (action_flag) {
        case 0: action_type = "sent"; break;
        case 1: action_type = "received"; break;
        case 2: action_type = "chose"; break;
    }
    std::cerr << "{\"peer_id\": " << own_id << ", \"action\": " << action_type
              << ", \"message_type\": " << mgs_type << ", \"message_value\": "  << msg.value 
              << ", \"proposal_num\": " << msg.proposal << "}" << std::endl;
}
void receivePaxosMessage(int sockfd, int sender_id) { 
    Message msg;
    ssize_t bytes_received = recv(sockfd, &msg, sizeof(msg), 0);

    if (bytes_received < 0){
        std::cerr << "recv: " << strerror(errno) << std::endl;
        return;
    }
    else if (bytes_received < sizeof(msg))
        std:: cerr << "recv: " << "didn't receive full message";
    
    printMessage(msg, 1);
    handlePaxosMessage(sender_id, msg);
}

void sendPaxosMessage(int sockfd, Message &msg) {
    ssize_t bytes_sent = send(sockfd, &msg, sizeof(msg), 0);

    if (bytes_sent < 0){
        std::cerr << "send: " << strerror(errno) << std::endl;
        return;
    }
    else if (bytes_sent < sizeof(msg))
        std:: cerr << "send: " << "didn't send full message";

    printMessage(msg, 0);
}

void preparePaxos() {

    max_round++;
    n = ((max_round & 0xFF) << 8) | (own_id & 0xFF); // 8 bits(max_round) --> 8bits(own_id)

    prepare_acks = 0;
    accept_acks = 0;

    Message prep_msg{MessageType::Prepare, n, '\0'};
    for (const auto& peer : peers){
        if (peer.second.role == Role::Acceptor)
            sendPaxosMessage(peer.second.outgoing_sockfd, prep_msg);
    }

}

void handlePrepare(int sender_id, Message &msg){

    if (msg.proposal > min_proposal){
        min_proposal = msg.proposal;
    }

    Message prep_ack_msg{MessageType::PrepareAck, accepted_proposal, accepted_value};
    sendPaxosMessage(peers[sender_id].outgoing_sockfd, prep_ack_msg);
}

void handlePrepareAck(int sender_id, Message &msg){
    
    if (msg.value != '\0' && msg.proposal > max_accepted_proposal) {
        value = msg.value;
        max_accepted_proposal = msg.proposal;
    }

    // // update max_round in case
    // int received_max_round = (msg.proposal >> 8) & 0xFF;
    // max_round = std::max(max_round, received_max_round);

    prepare_acks += 1;
    if (prepare_acks > peers.size() / 2) {
        Message accept_msg{MessageType::Accept, n, value};
        for (const auto& peer : peers) {
            if (peer.second.role == Role::Acceptor)
                sendPaxosMessage(peer.second.outgoing_sockfd, accept_msg);
        }
        max_accepted_proposal = -1;
    }

}

void handleAccept(int sender_id, Message &msg){
    
    if (msg.proposal >= min_proposal) {
        accepted_proposal = msg.proposal;
        min_proposal = msg.proposal;
        accepted_value = msg.value;
    }

    Message accept_ack_msg{MessageType::AcceptAck, min_proposal, '\0'};
    sendPaxosMessage(peers[sender_id].outgoing_sockfd, accept_ack_msg);
}
void handleAcceptAck(int sender_id, Message &msg){
    
    // update max_round in case
    // int received_max_round = (msg.proposal >> 8) & 0xFF;
    // max_round = std::max(max_round, (msg.proposal >> 8) & 0xFF);

    if (msg.proposal > n) {
        max_round = std::max(max_round, (msg.proposal >> 8) & 0xFF); // update max_round
        preparePaxos(); // new proposal
    } else {
        accept_acks += 1;
        if (accept_acks > hosts.size() / 2) {
            chosen_value = value;
            Message learn_msg{MessageType::Chose, msg.proposal, chosen_value};
            // for (const auto& peer : peers){
            //     if (peer.second.role == Role::Learner)
            //         sendPaxosMessage(peer.second.outgoing_sockfd, learn_msg);
            // }
            printMessage(learn_msg, 2);
        }
    }
    
}
void handlePaxosMessage(int sender_id, Message &msg) {
    switch (msg.type) {
        case MessageType::Prepare:
            handlePrepare(sender_id, msg);
            break;
        case MessageType::PrepareAck:
            handlePrepareAck(sender_id, msg);
            break;
        case MessageType::Accept:
            handleAccept(sender_id, msg);
            break;
        case MessageType::AcceptAck:
            handleAcceptAck(sender_id, msg);
            break;
    }
}



