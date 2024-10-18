FROM ubuntu:latest

RUN apt-get update && apt-get install -y g++

WORKDIR /app
COPY main.cpp peer.hpp peer.cpp membership.hpp membership.cpp hostsfile.txt .
RUN g++ main.cpp peer.cpp membership.cpp -o peer 

ENTRYPOINT ["/app/peer"]