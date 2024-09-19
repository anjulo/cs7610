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

const int PORT = 8888;
const char* ALIVE_MESSAGE = "ALIVE?";
const char* ACK_MESSAGE = "ACK!";
const int INITIAL_DELAY = 5; // seconds
const int RETRY_DELAY = 2;
const int MAX_RETRIES = 10;

std::atomic<bool> ready(false);

std::vector<std::string> readHostfile(const std::string& filename) {
    std::vector<std::string> hosts;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        hosts.push_back(line);
    }
    return hosts;
}

void sendMessage(int sockfd, const std::string& dst_host, const char* message) {
    struct sockaddr_in dst_addr;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int status = getaddrinfo(dst_host.c_str(), std::to_string(PORT).c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "Error resolving hostname: " << dst_host << ": " << gai_strerror(status) << std::endl;
        return;
    }

    memcpy(&dst_addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    // std::cerr << "Sent " << message << " to " << dst_host << std::endl;
    sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr));
}

void receiveMessages(int sockfd, std::vector<std::string>& hosts, std::string& own_hostname) {
    std::vector<bool> receivedFrom(hosts.size(), false);
    int numReceived = 0;

    // while (numReceived < hosts.size() - 1) {
     while (1) {
        char buffer[1024];
        struct sockaddr_in src_addr;
        socklen_t addrlen = sizeof(src_addr);

        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr, &addrlen);
        if (n < 0) {
            std::cerr << "Error receiving message\n";
            continue;
        }

        buffer[n] = '\0';
        char hostname_[NI_MAXHOST];
        getnameinfo((struct sockaddr *)&src_addr, addrlen, hostname_, NI_MAXHOST, NULL, 0, 0);
        std::string hostname(hostname_);

        // std::cerr << "Received " << buffer << " from " << hostname << std::endl;

        auto it = std::find(hosts.begin(), hosts.end(), hostname.substr(0, hostname.find('.')));
        if (it != hosts.end() && !receivedFrom[it - hosts.begin()]) {
            receivedFrom[it - hosts.begin()] = true;
            numReceived++;
        }

        // if the message received is an "alive?" message
        if (strcmp(buffer, ALIVE_MESSAGE) == 0)
            sendMessage(sockfd, hostname, ACK_MESSAGE);

        if (numReceived == hosts.size() - 1 && !ready.load()) {
            std::cerr << "Ready" << std::endl;
            ready.store(true);
        }
    }

}

int main(int argc, char* argv[]) {
    if (argc != 3 || std::string(argv[1]) != "-h") {
        std::cerr << "Usage: " << argv[0] << " -h <hostfile>\n";
        return 1;
    }

    std::vector<std::string> hosts = readHostfile(argv[2]);


    // networking
    struct addrinfo hints, *res;
    int sockfd, status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if((status = getaddrinfo(NULL, std::to_string(PORT).c_str(), &hints, &res)) != 0){
        std::cerr << "Error getting address info: " << gai_strerror(status) << std::endl;
        return 1;
    }

    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0){
        std::cerr << "Error creating socket\n";
        freeaddrinfo(res);
        return 1;
    }

    if(bind(sockfd, res->ai_addr, res->ai_addrlen) < 0){
        std::cerr << "Error binding socket\n";
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }   

    freeaddrinfo(res);


    // get current program name
    char own_hostname_[256];
    gethostname(own_hostname_, sizeof(own_hostname_));
    std::string own_hostname(own_hostname_);

    std::thread receiveThread(receiveMessages, sockfd, std::ref(hosts), std::ref(own_hostname));

    // initial delay to allow all programs to start and set-up listening thread
    std::this_thread::sleep_for(std::chrono::seconds(INITIAL_DELAY));

    for (int retry = 0; retry < MAX_RETRIES && !ready.load(); retry++) {
        for (const auto& dst_host : hosts) {
            if (dst_host != own_hostname) {
                sendMessage(sockfd, dst_host, ALIVE_MESSAGE);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY));
    }

    receiveThread.join();


    close(sockfd);
    return 0;
}