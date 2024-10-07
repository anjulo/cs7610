#ifndef SNAPSHOT_HPP
#define SNAPSHOT_HPP

#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <algorithm>
#include <chrono>

const int MARKER = 2;

extern float marker_delay;
extern int snapshot_state;

struct Snapshot {
    int id;
    bool active;
    int state;
    bool has_token;
    std::map<int, bool> channel_recording;
    std::map<int, std::vector<std::string>> channel_state;
};

extern std::map<int, Snapshot> snapshots;

void startSnapshot(int snapshot_id, int marker_sender_id = -1);
void handleMarker(int sender_id, int snapshot_id);

#endif