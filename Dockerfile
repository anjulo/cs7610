FROM ubuntu:latest

RUN apt-get update && apt-get install -y g++

WORKDIR /app
COPY . .
RUN g++ main.cpp process.cpp snapshot.cpp -o process 

ENTRYPOINT ["/app/process"]
