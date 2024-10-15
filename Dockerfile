FROM ubuntu:latest

RUN apt-get update && apt-get install -y g++

WORKDIR /app
COPY main.cpp peer.cpp peer.hpp hostsfile.txt .
RUN g++ main.cpp peer.cpp -o peer 

ENTRYPOINT ["/app/peer"]