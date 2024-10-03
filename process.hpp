#ifndef PROCESS_HPP
#define PROCESS_HPP

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

const int TOKEN = 1;

extern int own_id;
extern int predecessor_id;
extern int successor_id;
extern int state;
extern std::atomic<bool> has_token;

struct Peer {
    std::string hostname;
    int incoming_sockfd;
    int outgoing_sockfd;
};

extern std::map<int, Peer> peers;

std::vector<std::string> readHostsfile(const std::string& filename);
void configurePeers(std::vector<std::string> hosts);
int initializeListener();
int connectToPeer(const std::string& hostname, const char* port);
void handleConnections(int server_sockfd);
void processToken(float token_delay, int snapshot_state, int snapshot_id);
void receiveMessages(float marker_delay);

#endif
