#include "paxos.hpp"

int n = 0;
int max_round = 2;

int min_proposal = 0;
int accepted_proposal = 0;
char accepted_value = '\0';
int max_accepted_proposal = 0;

int prepare_acks = 0;
int accept_acks = 0;
char chosen_value = '\0';


void printMessage(Message &msg, int id, int action_flag) {
    std::string mgs_type;
    switch (msg.type) {
        case MessageType::Prepare: mgs_type = "\"prepare\""; break;
        case MessageType::PrepareAck: mgs_type = "\"prepare_ack\""; break;
        case MessageType::Accept: mgs_type = "\"accept\""; break;
        case MessageType::AcceptAck: mgs_type = "\"accept_ack\""; break;
        case MessageType::Chose: mgs_type = "\"chose\""; break;
    }
    std::string action_type;
    switch (action_flag) {
        case 0: action_type = "\"sent\""; break;
        case 1: action_type = "\"received\""; break;
        case 2: action_type = "\"chose\""; break;
    }
    std::cerr << "{\"peer_id\": " << id << ", \"action\": " << action_type
              << ", \"message_type\": " << mgs_type; 
    
    if (msg.value != '\0') std::cerr << ", \"message_value\": "  << msg.value;
    if (msg.proposal != 0) std::cerr << ", \"proposal_num\": " << msg.proposal ;
    
    std::cerr << "}" << std::endl;
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
    
    printMessage(msg, sender_id, 1);
    handlePaxosMessage(sender_id, msg);
}

void sendPaxosMessage(int receiver_id, Message &msg) {
    ssize_t bytes_sent = send(peers[receiver_id].outgoing_sockfd , &msg, sizeof(msg), 0);

    if (bytes_sent < 0){
        std::cerr << "send: " << strerror(errno) << std::endl;
        return;
    }
    else if (bytes_sent < sizeof(msg))
        std:: cerr << "send: " << "didn't send full message";

    printMessage(msg,receiver_id, 0);
}

void preparePaxos() {

    max_round++;
    n = ((max_round & 0xF) << 4) | (own_id & 0xF); // 4 bits(max_round) --> 4 bits(own_id)

    prepare_acks = 0;
    accept_acks = 0;

    Message prep_msg{MessageType::Prepare, n, '\0'};
    for (int acceptor_id : acceptors[own_role])
            sendPaxosMessage(acceptor_id, prep_msg);

}

void handlePrepare(int sender_id, Message &msg){

    if (msg.proposal > min_proposal){
        min_proposal = msg.proposal;
    }

    Message prep_ack_msg{MessageType::PrepareAck, accepted_proposal, accepted_value};
    sendPaxosMessage(sender_id, prep_ack_msg);
}

void handlePrepareAck(int sender_id, Message &msg){
    
    if (msg.value != '\0' && msg.proposal > max_accepted_proposal) {
        value = msg.value;
        max_accepted_proposal = msg.proposal;
    }

    prepare_acks += 1;
    if (prepare_acks == hosts.size() / 2 + 1) {
        Message accept_msg{MessageType::Accept, n, value};

        for (int acceptor_id : acceptors[own_role])
            sendPaxosMessage(acceptor_id, accept_msg);
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
    sendPaxosMessage(sender_id, accept_ack_msg);
}
void handleAcceptAck(int sender_id, Message &msg){

    if (msg.proposal > n) {
        max_round = std::max(max_round, (msg.proposal >> 4) & 0xF); // update max_round
        preparePaxos(); // new proposal
    } else {
        accept_acks += 1;
        if (accept_acks == hosts.size() / 2 + 1) {
            chosen_value = value;
            Message learn_msg{MessageType::Chose, msg.proposal, chosen_value};
            printMessage(learn_msg, own_id, 2);
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



