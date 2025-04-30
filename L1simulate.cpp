#include <bits/stdc++.h>
#include "L1simulate.hpp"
#include "L1simulate.hpp"
using namespace std;


int main(int argc, char* argv[]) {
    string tracefile;
    string outfile;
    int s = -1, b = -1;

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
                num_ways = atoi(optarg);
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
    if (tracefile.empty() || s == -1 || num_ways == -1 || b == -1 || outfile.empty()) {
        cerr << "Missing required arguments.\n";
        print_help();
        return 1;
    }

    inst_proc[0] = read_trace_files(tracefile, 0);
    inst_proc[1] = read_trace_files(tracefile, 1);
    inst_proc[2] = read_trace_files(tracefile, 2);
    inst_proc[3] = read_trace_files(tracefile, 3);
    for (int i = 0; i < 4; i++) {
        tag[i] = create_tag(1 << s, num_ways);
        is_full[i] = create_is_full(1 << s);
        states[i] = initialize_states(1 << s, num_ways);
        access_times[i] = create_access_times(1 << s, num_ways);
    }
    num_sets = 1 << s;
    block_size = 1 << b;
    for (int i = 0; i < 4; ++i) {
        int read_count = 0, write_count = 0;
        for (const auto& inst : inst_proc[i]) {
            if (inst.first == 'R') {
                read_count++;
            } else if (inst.first == 'W') {
                write_count++;
            }
        }
        cout << "Core " << i << " - Reads: " << read_count << ", Writes: " << write_count << endl;
    }
    vector<int> execution_cycles(4, -1), idle_cycles(4, 0), num_misses(4, 0);
    long long int bustraffic = 0;
    while (curr_inst[0] < inst_proc[0].size() || curr_inst[1] < inst_proc[1].size() || curr_inst[2] < inst_proc[2].size() || curr_inst[3] < inst_proc[3].size()) {
        // cout<<curr_inst[0]<<" "<<curr_inst[1]<<" "<<curr_inst[2]<<" "<<curr_inst[3]<<endl;
        // cout<<curr_cycle<<endl;
        // cout<<stall[0]<<" "<<stall[1]<<" "<<stall[2]<<" "<<stall[3]<<endl;
        // cout<<bus_stall<<endl;
        
        for (int i = 0; i < 4; i++) {
            if (execution_cycles[i] == -1) {
                if (curr_inst[i] >= inst_proc[i].size()) {
                    execution_cycles[i] = curr_cycle + stall[i];
                }
            }
        }
        for(int i = 0; i < 4 ; i++){
            if (curr_inst[i] < inst_proc[i].size()) {
                if(stall[i] <= 0) {
                    char inst = inst_proc[i][curr_inst[i]].first;
                    int address = inst_proc[i][curr_inst[i]].second;
                    if (inst == 'R') {
                        int state = obtain_state(address, tag[i], states[i]);
                        if (state == U || state == I ){
                            if (bus_stall <= 0) {
                                num_misses[i]++;
                                bool found = false;
                                for (int j = 0; j < 4; j++) { // search in other cache
                                    if (j==i) {
                                        continue;
                                    }
                                    int otherstate = obtain_state(address, tag[j], states[j]);
                                    if (otherstate == M) { // write to mem and change the states
                                        bus_stall = 100 + 2*block_size/4;
                                        writebacks[i]++;
                                        stall[i] = 2*block_size/4;
                                        bustraffic += block_size*2;
                                        insert_cache_line(tag[i], address, is_full[i], i);
                                        update_access_time(address, i, curr_cycle + 2*block_size/4);
                                        found = true;
                                        break;
                                    } else if (otherstate == E || otherstate == S) { 
                                        stall[i] = 2*block_size/4;
                                        bus_stall = 2*block_size/4;
                                        bustraffic += block_size;
                                        insert_cache_line(tag[i], address, is_full[i], i);
                                        update_access_time(address, i, curr_cycle + 2*block_size/4);
                                        found = true;
                                        break;
                                    }
                                }
                                if (found) {
                                    for (int j = 0; j < 4; j++) {
                                        set_state(address, tag[j], states[j], S, j);
                                    }
                                } else {
                                    stall[i] = 100;
                                    bus_stall = 100;
                                    bustraffic += block_size;
                                    insert_cache_line(tag[i], address, is_full[i], i);
                                    set_state(address, tag[i], states[i], E, i);
                                    update_access_time(address, i, curr_cycle + 100);
                                }
                                curr_inst[i]++;
                            }
                        } else { // read hit
                            cout<<"Read hit "<<i<<" "<<address<<endl;
                            curr_inst[i]++;
                            update_access_time(address, i, curr_cycle);
                        }
                        
                    } else { // write 
                        cout<<"Write "<<i<<" "<<address<<endl;
                        int state = obtain_state(address, tag[i], states[i]);
                        if (state == U || state == I ){
                            if (bus_stall <= 0) {
                                num_misses[i]++;
                                bool found = false;
                                for (int j = 0; j < 4; j++) { // search in other cache
                                    if (j==i) {
                                        continue;
                                    }
                                    int otherstate = obtain_state(address, tag[j], states[j]);
                                    if (otherstate == M) { // write to mem and change the states
                                        bus_stall = 200;
                                        stall[i] = 200;
                                        writebacks[i]++;
                                        bustraffic += block_size*2;
                                        insert_cache_line(tag[i], address, is_full[i], i);
                                        update_access_time(address, i, curr_cycle + 200);
                                        found = true;
                                        break;
                                    } else if (otherstate == E || otherstate == S) { 
                                        stall[i] = 100;
                                        bustraffic += block_size;
                                        bus_stall = 100;
                                        insert_cache_line(tag[i], address, is_full[i], i);
                                        update_access_time(address, i, curr_cycle + 100);
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    cout<<"Not found write address "<<address<<endl;
                                    stall[i] = 100;
                                    bus_stall = 100;
                                    bustraffic += block_size;
                                    insert_cache_line(tag[i], address, is_full[i], i);
                                    update_access_time(address, i, curr_cycle + 100);
                                }
                                for (int j = 0; j < 4; j++) {
                                    if (i == j) set_state(address, tag[j], states[j], M, j);
                                    else
                                    set_state(address, tag[j], states[j], I, j);
                                }
                                curr_inst[i]++;
                            }
                        } else { // write hit
                            cout<<"Write hit "<<i<<" "<<address<<endl;
                            if (state == S) {
                                if (bus_stall <= 0) {
                                    update_access_time(address, i, curr_cycle);
                                    for (int j = 0; j < 4; j++) {
                                        if (i == j) set_state(address, tag[j], states[j], M, j);
                                        else
                                        set_state(address, tag[j], states[j], I, j);
                                    }
                                    curr_inst[i]++;
                                } else {
                                }
                            } else { // E or M
                                update_access_time(address, i, curr_cycle);
                                set_state(address, tag[i], states[i], M, i);
                                curr_inst[i]++;
                            }
                        }
                    }
                }
            }
            stall[i]--;
        }
        bus_stall --;
        curr_cycle++;
    }
    for (int i = 0; i < 4; ++i) {
        if (execution_cycles[i] == -1) {
            execution_cycles[i] = curr_cycle + stall[i];
        }
    }
    // cout execution cycles
    for (int i = 0; i < 4; ++i) {
        cout<<"Core " << i <<":"<<endl;
        cout << "\tExecution cycles: " << execution_cycles[i]<< endl;
        idle_cycles[i] = execution_cycles[i] - inst_proc[i].size();
        cout << "\tIdle cycles: " << idle_cycles[i]<< endl;
        cout << "\tMisses: " << num_misses[i]<< endl;
        cout<<"\tMiss rate "<<setprecision(2)<<fixed<<((float)num_misses[i]/inst_proc[i].size())<<endl;
        cout<<"\tEvictions "<<evictions[i]<<endl;
        cout<<"\tWritebacks "<<writebacks[i]<<endl;
        cout<<"\tInvalidations "<<invalidations[i]<<endl;
    }
    cout<<"Bus traffic "<<bustraffic<<" bytes"<<endl;
    return 0;
}