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
#include <algorithm>
#include <string>
#include <errno.h>

#define PORT "3490"
#define INITIAL_DELAY 5 // seconds
#define BACKLOG 5

extern int own_id;

struct Peer {
    std::string hostname;
    int incoming_sockfd;
    int outgoing_sockfd;
};

extern std::map<int, Peer> peers;
extern std::vector<std::string> hosts;

extern std::atomic<bool> should_exit;
extern std::vector<std::thread> threads;

void readHostsfile(const std::string& filename);
void configurePeers(std::vector<std::string> hosts);
int setupSocketTCP();
void handleTCPConnection(int tcp_sockfd);
int connectToPeerTCP(const std::string& hostname);

int setupSocketUDP();
void sendMessageUDP(int sockfd, const std::string& dst_host, const char* message);
void receiveAllMessages(int tcp_sockfd, int udp_sockfd);

#endif