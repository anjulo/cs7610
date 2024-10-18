#ifndef MEMBERSHIP_HPP
#define MEMBERSHIP_HPP

#include "peer.hpp"
#include <vector>
#include <map>
#include <set>

enum Operation { ADD };

struct Message {
    enum Type { JOIN, REQ, OK, NEWVIEW } type;
    int request_id;
    int view_id;
    int peer_id;
    int sender_id;
    std::vector<int> membership_list;
    Operation operation;
};

struct PendingOperation {
    int request_id;
    int view_id;
    int peer_id;
    Operation type;
};

extern int leader_id;
extern int view_id;
extern int request_id;
extern std::vector<int> membership_list;
extern std::map<std::pair<int, int>, int> oks_recieved;
extern std::map<std::pair<int, int>, PendingOperation> pending_operations;

Message receiveMessage(int sockfd);
void processIncomingMessages();
void joinGroup();

#endif