#include <bits/stdc++.h>
#include "L1simulate.hpp"
using namespace std;


int main(int argc, char* argv[]) {
    string tracefile;
    string outfile;
    int s = -1, E = -1, b = -1;

    int opt;
    while ((opt = getopt(argc, argv, "t:s:E:b:o:h")) != -1) {
        switch (opt) {
            case 't':
                tracefile = optarg;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'h':
                print_help();
                return 0;
            default:
                print_help();
                return 1;
        }
    }

    // Validate required arguments
    if (tracefile.empty() || s == -1 || E == -1 || b == -1 || outfile.empty()) {
        cerr << "Missing required arguments.\n";
        print_help();
        return 1;
    }

    inst_proc[0] = read_trace_files(tracefile, 0);
    inst_proc[1] = read_trace_files(tracefile, 1);
    inst_proc[2] = read_trace_files(tracefile, 2);
    inst_proc[3] = read_trace_files(tracefile, 3);
    for (int i = 0; i < 4; i++) {
        tag[i] = create_tag(1 << s, E);
        is_full[i] = create_is_full(1 << s);
        states[i] = initialize_states(1 << s, E);
        access_times[i] = create_access_times(1 << s, E);
    }
    num_sets = tag[0].size();
    num_ways = tag[0][0].size();
    block_size = tag[0][0].size() / num_ways;
    bool bus_free = true;
    while (curr_inst[0] < inst_proc[0].size() || curr_inst[1] < inst_proc[1].size() || curr_inst[2] < inst_proc[2].size() || curr_inst[3] < inst_proc[3].size()) {
        
        if (!bus_free){
            if (current_transaction.cycles_remaining==0){
                bus_free = true;
                if(!current_transaction.to_mem){
                    
                }
            }
            else{
                current_transaction.cycles_remaining--;
            }
        } 
        for(int i = 0; i < 4 ; i++){
            if (curr_inst[i] < inst_proc[i].size()) {
                if(!stall[i]) {
                    char inst = inst_proc[i][curr_inst[i]].first;
                    int address = inst_proc[i][curr_inst[i]].second;
                    if (inst == 'R') {
                        int state = obtain_state(address, tag[i], states[i]);
                        if (state == U || state == I ){
                            if (bus_free) {
                                for (int j = 0; j < 4; i++) {
                                    if (j==i) continue;
                                    
                            } else {
                                stall[i] = true;
                            }
                        } else if (state == S || state == E) {
                            update_access_time(address, i);
                        }
                }
            }
        }
        }
    return 0;
}
