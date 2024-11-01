FROM ubuntu:latest

RUN apt-get update && apt-get install -y g++

WORKDIR /app
COPY main.cpp peer.hpp peer.cpp paxos.hpp paxos.cpp hostsfile-testcase1.txt hostsfile-testcase2.txt .
RUN g++ main.cpp peer.cpp paxos.cpp -o peer 

ENTRYPOINT ["/app/peer"]