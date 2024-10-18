#ifndef PEER_HPP
#define PEER_HPP

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

#define PORT "3490"
#define INITIAL_DELAY 5 // seconds
#define BACKLOG 5

extern int own_id;
extern int delay;

struct Peer {
    std::string hostname;
    int incoming_sockfd;
    int outgoing_sockfd;
};

extern std::map<int, Peer> peers;

std::vector<std::string> readHostsfile(const std::string& filename);
void configurePeers(std::vector<std::string> hosts);
int initializeConnectionListener();
int connectToPeer(const std::string& hostname, const char* port);
void handleIncomingConnections(int server_sockfd);

#endif