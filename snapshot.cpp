#include "snapshot.hpp"
#include "process.hpp"

float marker_delay;
int snapshot_state = -1;
Snapshot current_snapshot = {-1, false};


// handles start snapshotting for all processes(including the initator)
void startSnapshot(int snapshot_id, int marker_sender_id) {

    // log snapshot started
    std::cerr << "{proc_id: " << own_id  << ", snapshot_id: " << snapshot_id 
              << ", snapshot: \"started\"" << std::endl;

    // 1. record local state
    current_snapshot = {snapshot_id, true, state, has_token.load(), {}, {}};

    // 2. send markers on all outgoing channels
    for (const auto& peer : peers){
        std::cerr << "{proc_id: " << own_id << ", snapshot_id: " << snapshot_id 
              << ", sender: " << own_id << ", receiver: " << peer.first 
              << ", msg: \"marker\", state: " << state << ", has_token: " 
              << (current_snapshot.has_token ? "YES" : "NO") << "}" << std::endl;

        int message[2] = {MARKER, snapshot_id};
        send(peers[peer.first].outgoing_sockfd, message, sizeof(message), 0);
    }

    // 3. start recording on all incoming channels(if not initator, except for the channel the marker came on)
    for(const auto& peer : peers){
        if(peer.first == marker_sender_id)
            current_snapshot.channel_recording[peer.first] = false;
        else {
            current_snapshot.channel_recording[peer.first] = true;
            current_snapshot.channel_state[peer.first] = {};
        }
    }
}

void handleMarker(int sender_id, int snapshot_id) {
    
    if(!current_snapshot.active){ // / first time receiving the marker
        std::this_thread::sleep_for(std::chrono::duration<float>(marker_delay));
        startSnapshot(snapshot_id, sender_id);
    } else {  // not the first time receiving the marker
        // stop recording on channel C_ki
        current_snapshot.channel_recording[sender_id] = false;

        // log channel closed
        std::cerr << "{proc_id: " << own_id << ":, snapshot_id: " << snapshot_id
                  << ", snapshot: " << "channel closed, " 
                  << "channel: " << sender_id << "-" << own_id
                  << ", queue: [";
        for(size_t i = 0; i < current_snapshot.channel_state[sender_id].size(); i++){
            if(i > 0) std::cerr << ",";
            std::cerr << current_snapshot.channel_state[sender_id][i];
        }
        std::cerr << "]}" << std::endl; 


        // if all channels closed, finish the snapshot
        auto isFalse = [](const auto& pair) { return !pair.second; };
        if (std::all_of(current_snapshot.channel_recording.begin(), current_snapshot.channel_recording.end(), isFalse)) {
            std::cerr << "{proc_id:" << own_id << ", snapshot_id: " << current_snapshot.id 
                      << ", snapshot: \"finished\"}" << std::endl;
            current_snapshot.active = false;
        }
    }
}