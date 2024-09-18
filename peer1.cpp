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

const int PORT = 8888;
const char* MESSAGE = "READY";

std::vector<std::string> readHostfile(const std::string& filename) {
    std::vector<std::string> hosts;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        hosts.push_back(line);
    }
    return hosts;
}

int main(int argc, char* argv[]) {

    std::this_thread::sleep_for(std::chrono::seconds(5));

    if (argc != 3 || std::string(argv[1]) != "-h") {
        std::cerr << "Usage: " << argv[0] << " -h <hostfile>\n";
        return 1;
    }

    std::vector<std::string> hosts = readHostfile(argv[2]);
    int numPeers = hosts.size();

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    struct sockaddr_in myaddr;
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        std::cerr << "Error binding socket\n";
        return 1;
    }

    std::vector<bool> receivedFrom(numPeers, false);
    int numReceived = 0;

    char own_hostname[256];
    gethostname(own_hostname, sizeof(own_hostname));
    // std::cout << own_hostname << std::endl;

    // Send READY message to all peers
    for (const auto& host : hosts) {

        if (host == own_hostname) {
            std::cout << "Skipping self: " << host << std::endl;
            continue;
        }

        struct sockaddr_in peeraddr;
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        int status;
        if ((status = getaddrinfo(host.c_str(), std::to_string(PORT).c_str(), &hints, &res)) != 0) {
            std::cerr << "Error resolving hostname: " << host << ": " << gai_strerror(status) << std::endl;
            continue;
        }
        memcpy(&peeraddr, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);

        ssize_t sent = sendto(sockfd, MESSAGE, strlen(MESSAGE), 0, (struct sockaddr *)&peeraddr, sizeof(peeraddr));

         
        if (sent < 0)
            std::cerr << "Error sending message to " << host << ": " << strerror(errno) << std::endl;
        else
            std::cout << "Sent READY to " << host << " (" << sent << " bytes)" << std::endl;   
    }

    // std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // std::cout<< 1 << std::endl;

    // Receive READY messages from peers
    while (numReceived < numPeers - 1) {
        char buffer[1024];
        struct sockaddr_in peeraddr;
        socklen_t addrlen = sizeof(peeraddr);

        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peeraddr, &addrlen);
        if (n < 0) {
            std::cerr << "Error receiving message\n";
            continue;
        }

        buffer[n] = '\0';
        if (strcmp(buffer, MESSAGE) == 0) {
            char hostname[NI_MAXHOST];
            if (getnameinfo((struct sockaddr *)&peeraddr, addrlen, hostname, NI_MAXHOST, NULL, 0, 0) == 0) {
                for (int i = 0; i < numPeers; i++) {
                    if (hosts[i] == hostname && !receivedFrom[i]) {
                        receivedFrom[i] = true;
                        numReceived++;
                        std::cout << "Recieved message from: " << hostname << std::endl;
                        break;
                    }
                }
            }
        }
    }

    std::cout<< 2 << std::endl;

    std::cerr << "READY\n";

    close(sockfd);
    return 0;
}