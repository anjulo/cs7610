FROM ubuntu:latest

RUN apt-get update && apt-get install -y g++

WORKDIR /app
COPY . .
RUN g++ peer.cpp -o peer

ENTRYPOINT ["/app/peer"]
