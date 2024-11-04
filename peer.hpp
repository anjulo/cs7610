#ifndef PEER_HPP
#define PEER_HPP

#include "paxos.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <errno.h>

#define PORT "3490"
#define INITIAL_DELAY 5 // seconds
#define BACKLOG 5


extern int own_id;
extern std::string own_role;

struct Peer {
    std::string hostname;
    int incoming_sockfd;
    int outgoing_sockfd;
};

extern std::unordered_map<int, Peer> peers;
extern std::vector<std::string> hosts;
extern std::unordered_map<std::string, std::vector<int>> acceptors;

void readHostsFile(const std::string& filename);

int setupSocketTCP();
void handleTCPConnection(int tcp_sockfd);
int connectToPeerTCP(const std::string& hostname);

void receiveAllMessages(int tcp_sockfd);

#endif