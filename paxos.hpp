#ifndef PAXOS_HPP
#define PAXOS_HPP

#include "peer.hpp"

extern int n;
extern char value;
extern int max_round;
extern int min_proposal;
extern int accepted_proposal;
extern char accepted_value;
extern int max_accepted_proposal;

extern int prepare_acks;
extern int accept_acks;
extern char proposed_value;
extern char chosen_value;

enum MessageType { Prepare, PrepareAck, Accept, AcceptAck, Chose };

struct Message {
    MessageType type;
    int proposal;
    char value;
};

void preparePaxos();

void receivePaxosMessage(int sockfd, int sender_id);
void sendPaxosMessage(int sockfd, Message &msg);
void handlePaxosMessage(int sender_id, Message &msg);

// void chosenNotify();


#endif