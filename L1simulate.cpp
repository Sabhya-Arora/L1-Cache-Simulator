#include <bits/stdc++.h>
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
    ofstream out(outfile);


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
    // for (int i = 0; i < 4; ++i) {
    //     int read_count = 0, write_count = 0;
    //     for (const auto& inst : inst_proc[i]) {
    //         if (inst.first == 'R') {
    //             read_count++;
    //         } else if (inst.first == 'W') {
    //             write_count++;
    //         }
    //     }
    //     out << "Core " << i << " - Reads: " << read_count << ", Writes: " << write_count << endl;
    // }


    int block_size_bytes = 1 << b;
    int num_sets = 1 << s;
    int cache_size_bytes = num_sets * num_ways * block_size_bytes;
    int cache_size_kb = cache_size_bytes / 1024;

    out << "Simulation Parameters:\n";
    out << "Trace Prefix: " << tracefile << "\n";
    out << "Set Index Bits: " << s << "\n";
    out << "Associativity: " << num_ways << "\n";
    out << "Block Bits: " << b << "\n";
    out << "Block Size (Bytes): " << block_size_bytes << "\n";
    out << "Number of Sets: " << num_sets << "\n";
    out << "Cache Size (KB per core): " << cache_size_kb << "\n";
    out << "MESI Protocol: Enabled\n";
    out << "Write Policy: Write-back, Write-allocate\n";
    out << "Replacement Policy: LRU\n";
    out << "Bus: Central snooping bus\n\n";

    while (curr_inst[0] < inst_proc[0].size() || curr_inst[1] < inst_proc[1].size() || curr_inst[2] < inst_proc[2].size() || curr_inst[3] < inst_proc[3].size()) {
        // out<<curr_inst[0]<<" "<<curr_inst[1]<<" "<<curr_inst[2]<<" "<<curr_inst[3]<<endl;
        // out<<curr_cycle<<endl;
        // out<<stall[0]<<" "<<stall[1]<<" "<<stall[2]<<" "<<stall[3]<<endl;
        // out<<bus_stall<<endl;
        
        for (int i = 0; i < 4; i++) {
            if (total_cycles[i] == -1) {
                if (curr_inst[i] >= inst_proc[i].size()) {
                    total_cycles[i] = curr_cycle + stall[i];
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
                                        execution_cycles[i] += 2*block_size/4 + 1; // ith core gets data and performs execution
                                        total_bus_trans += 2;
                                        bus_stall = 100 + 2*block_size/4 + 1;
                                        writebacks[j]++;
                                        stall[i] = 2*block_size/4 + 1;
                                        trafficpercore[j] += block_size;
                                        trafficpercore[i] += block_size;
                                        bustraffic += block_size*2;
                                        insert_cache_line(tag[i], address, is_full[i], i);
                                        update_access_time(address, i, curr_cycle + 2*block_size/4);
                                        found = true;
                                        break;
                                    } else if (otherstate == E || otherstate == S) { 
                                        total_bus_trans++;
                                        stall[i] = 2*block_size/4 + 1;
                                        execution_cycles[i] += 2*block_size/4 + 1; // ith core gets data
                                        bus_stall = 2*block_size/4 + 1;
                                        bustraffic += block_size;
                                        trafficpercore[j] += block_size;
                                        trafficpercore[i] += block_size;
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
                                    stall[i] = 101;
                                    execution_cycles[i] += 101; // ith core gets data
                                    bus_stall = 101;
                                    trafficpercore[i] += block_size;
                                    total_bus_trans++;
                                    bustraffic += block_size;
                                    insert_cache_line(tag[i], address, is_full[i], i);
                                    set_state(address, tag[i], states[i], E, i);
                                    update_access_time(address, i, curr_cycle + 100);
                                }
                                curr_inst[i]++;
                            } else {
                                idle_cycles[i]++;
                            }
                        } else { // read hit
                            // out<<"Read hit "<<i<<" "<<address<<endl;
                            execution_cycles[i]++;
                            curr_inst[i]++;
                            update_access_time(address, i, curr_cycle);
                        }
                        
                    } else { // write 
                        // out<<"Write "<<i<<" "<<address<<endl;
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
                                        bus_stall = 201;
                                        stall[i] = 201;
                                        idle_cycles[i] += 100;
                                        total_bus_trans += 2;
                                        execution_cycles[i] += 101; // ith core gets data and performs execution
                                        writebacks[j]++;
                                        bustraffic += block_size*2;
                                        trafficpercore[j] += block_size;
                                        trafficpercore[i] += block_size;
                                        insert_cache_line(tag[i], address, is_full[i], i);
                                        update_access_time(address, i, curr_cycle + 200);
                                        found = true;
                                        break;
                                    } else if (otherstate == E || otherstate == S) { 
                                        execution_cycles[i] += 101; // ith core gets data
                                        stall[i] = 101;
                                        total_bus_trans++;
                                        bustraffic += block_size;
                                        trafficpercore[j] += block_size;
                                        trafficpercore[i] += block_size;
                                        bus_stall = 101;
                                        insert_cache_line(tag[i], address, is_full[i], i);
                                        update_access_time(address, i, curr_cycle + 100);
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    // out<<"Not found write address "<<address<<endl;
                                    stall[i] = 101;
                                    bus_stall = 101;
                                    total_bus_trans++;
                                    execution_cycles[i] += 101; // ith core gets data
                                    bustraffic += block_size;
                                    trafficpercore[i] += block_size;
                                    insert_cache_line(tag[i], address, is_full[i], i);
                                    update_access_time(address, i, curr_cycle + 100);
                                } else {
                                    invalidations++;
                                    invalidations_per_core[i]++;
                                }
                                for (int j = 0; j < 4; j++) {
                                    if (i == j) set_state(address, tag[j], states[j], M, j);
                                    else
                                    set_state(address, tag[j], states[j], I, j);
                                }
                                curr_inst[i]++;
                            } else {
                                idle_cycles[i]++;
                            }
                        } else { // write hit
                            // out<<"Write hit "<<i<<" "<<address<<endl;
                            if (state == S) {
                                if (bus_stall <= 0) {
                                    update_access_time(address, i, curr_cycle);
                                    for (int j = 0; j < 4; j++) {
                                        if (i == j) set_state(address, tag[j], states[j], M, j);
                                        else
                                        set_state(address, tag[j], states[j], I, j);
                                    }
                                    invalidations++;
                                    invalidations_per_core[i]++;
                                    curr_inst[i]++;
                                    execution_cycles[i]++;
                                } else {
                                    idle_cycles[i]++;
                                }
                            } else { // E or M
                                update_access_time(address, i, curr_cycle);
                                set_state(address, tag[i], states[i], M, i);
                                curr_inst[i]++;
                                execution_cycles[i]++;
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
        int total_instructions = inst_proc[i].size();
        int total_reads = 0, total_writes = 0;
        for (const auto& inst : inst_proc[i]) {
            if (inst.first == 'R') total_reads++;
            else if (inst.first == 'W') total_writes++;
        }
    
    
        out << "Core " << i << " Statistics:\n";
        out << "Total Instructions: " << total_instructions << "\n";
        out << "Total Reads: " << total_reads << "\n";
        out << "Total Writes: " << total_writes << "\n";
        out << "Total Execution Cycles: " << execution_cycles[i] << "\n";
        out << "Idle Cycles: " << idle_cycles[i] << "\n";
        out << "Cache Misses: " << num_misses[i] << "\n";
        out << "Cache Miss Rate: " << fixed << setprecision(2) << (100.0 * num_misses[i] / total_instructions) << "%\n";
        out << "Cache Evictions: " << evictions[i] << "\n";
        out << "Writebacks: " << writebacks[i] << "\n";
        out<<"Bus Invalidations: "<<invalidations_per_core[i]<<"\n";
        out<<"Data Traffic (Bytes): "<<trafficpercore[i]<<"\n\n";
    }
    
    out << "Overall Bus Summary:\n";
    out << "Total Bus Transactions: " << total_bus_trans << "\n";
    // out << "Bus Invalidations: " << invalidations << "\n";
    out << "Total Bus Traffic (Bytes): " << bustraffic << "\n";
    int max_exec_time = 0;
    for (int i = 0; i < 4; ++i) {
        max_exec_time = max(max_exec_time, total_cycles[i]);
    }
    // out<<"Maximum execution time: "<<max_exec_time<<endl;
    return 0;
}