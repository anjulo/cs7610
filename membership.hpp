#ifndef MEMBERSHIP_HPP
#define MEMBERSHIP_HPP

#include "peer.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>

#define HEARTBEAT_MESSAGE "HEARTBEAT"
#define HEARTBEAT_INTERVAL 3

enum Operation { ADD, DEL, PENDING, NOTHING, UNKOWN };

struct Message {
    enum Type { JOIN, REQ, OK, NEWVIEW, NEWLEADER, NL_RESPONSE, UNKNOWN } type;
    int req_id;
    int view_id;
    int peer_id;
    int sender_id;
    std::vector<int> memb_list;
    Operation operation;
};

struct PendingOperation {
    int req_id;
    int view_id;
    int peer_id;
    Operation type;
};

extern int leader_id;
extern int view_id;
extern int req_id;
extern std::vector<int> memb_list;
extern std::map<std::pair<int, int>, int> oks_recieved;
extern std::map<std::pair<int, int>, PendingOperation> pending_operations;

extern std::unordered_map<int, std::chrono::steady_clock::time_point> last_heartbeat;
extern std::mutex heartbeat_mutex;

void joinGroup();
void handleTCPMessage(int sockfd, int src_id);

void sendHeartbeat(int udp_sockfd);
void checkFailures();
void handleUDPMessage(int udp_sockfd);

void sendREQBeforeCrash();


#endif