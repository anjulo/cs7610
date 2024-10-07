#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <map>
#include <queue>

#define PORT "3490"
#define INITIAL_DELAY 5 // seconds
#define BACKLOG 5

const int TOKEN = 1;

extern int own_id;
extern int pre_id;
extern int suc_id;
extern int state;
extern float token_delay;
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
void processToken(int snapshot_id);
void receiveMessages();

#endif