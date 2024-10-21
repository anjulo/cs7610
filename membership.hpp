#ifndef MEMBERSHIP_HPP
#define MEMBERSHIP_HPP

#include "peer.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>

#define HEARTBEAT_MESSAGE "HEARTBEAT"
#define HEARTBEAT_INTERVAL 5
#define FAILURE_TIMEOUT 10.0

enum Operation { ADD };

struct Message {
    enum Type { JOIN, REQ, OK, NEWVIEW, UNKNOWN } type;
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

extern std::unordered_map<int, std::chrono::steady_clock::time_point> last_heartbeat;
extern std::mutex heartbeat_mutex;

void joinGroup();
void processIncomingMessagesTCP();

void sendHeartbeat(int udp_sockfd);
void checkFailures();


#endif