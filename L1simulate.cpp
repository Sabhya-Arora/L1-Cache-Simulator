#include <bits/stdc++.h>
using namespace std;
enum Cache_Line_State {
    U = -1,
    M,
    E,
    S,
    I
};

struct BusTransaction {
    int proc_id;        // ID of processor issuing the request
    int address;        // memory address
    int cycles_remaining; // time remaining for the transaction
    bool read;
    bool mem;
    bool rwitm;
    bool invalidate;
    bool eviction;
};

void print_help() {
    cout << "Usage: ./L1simulate [options]\n"
              << "-t <tracefile> : name of parallel application (e.g., app1) whose 4 traces are to be used\n"
              << "-s <s>         : number of set index bits (number of sets = 2^s)\n"
              << "-E <E>         : associativity (number of cache lines per set)\n"
              << "-b <b>         : number of block bits (block size = 2^b)\n"
              << "-o <filename>  : output filename to log output for plotting\n"
              << "-h             : print this help message\n";
}

int curr_cycle = 0;
vector<vector<vector<int>>> cache(4);
vector<vector<vector<int>>> states(4);
vector<vector<vector<int>>> tag(4);
vector<vector<vector<int>>> access_times(4);
vector<vector<pair<char, int>>> inst_proc(4);
vector<vector<bool>> is_full(4);
vector<int> curr_inst(4, 0);
vector<bool> stall(4, false);
int num_sets, num_ways, block_size;
deque <struct BusTransaction> pending;

bool comp(const pair<int, int>& a, const pair<int, int>& b) {
    return a.second < b.second; // max-heap by second
}

int obtain_state (int address, vector<vector<int>> &tag, vector<vector<int>> &states) {
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    for (int i = 0; i < num_ways; i++) {
        if (tag[index][i] == tag_value) {
            return states[index][i];
        }
    }
    return U;
}

void set_state (int address, vector<vector<int>> &tag, vector<vector<int>> &states, int state) {
    int temp = address;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    for (int i = 0; i < num_ways; i++) {
        if (tag[index][i] == tag_value) {
            states[index][i] = state;
            cout<<"Successfully changed state"<<endl;
            cout << "State of address in set " << temp << " in processor " << i << ": ";
            if (states[index][i] == M) cout << "M\n";    
            else if (states[index][i] == E) cout << "E\n";
            else if (states[index][i] == S) cout << "S\n";
            else if (states[index][i] == I) cout << "I\n";
            else cout << "U\n";
            return;
        }
    }
    
}

vector<pair<char, int>> read_trace_files(const string& base_name, int i) {
    vector<pair<char, int>> lines;

    string filename = "traces/" + base_name + "_proc" + to_string(i) + ".trace";
    ifstream infile(filename);

    if (!infile) {
        cerr << "Error: Could not open " << filename << "\n";
        return lines; // instead of continue, since this is not in a loop anymore
    }

    string line;
    while (getline(infile, line)) {
        istringstream iss(line);
        char op;
        string hex_addr;

        if (!(iss >> op >> hex_addr)) {
            cerr << "Warning: Failed to parse line: " << line << "\n";
            continue;
        }

        // Convert hex string to int
        int addr = stoi(hex_addr, nullptr, 16);

        lines.emplace_back(op, addr);
    }

    infile.close();
    return lines;
}

vector<vector<int>> create_cache (int num_sets, int num_ways, int block_size) {
    vector<vector<int>> cache (num_sets, vector<int>(num_ways*block_size, 0));
    return cache;
}

vector<vector<int>> create_tag (int num_sets, int num_ways) {
    vector<vector<int>> tag (num_sets, vector<int>(num_ways, -1));
    return tag;
}
vector<vector<int>> create_access_times (int num_sets, int num_ways) {
    vector<vector<int>> access_times (num_sets, vector<int>(num_ways, -1));
    return access_times;
}
vector<bool> create_is_full (int num_sets) {
    vector<bool> is_full (num_sets, false);
    return is_full;
}
vector<vector<int>> initialize_states (int num_sets, int num_ways) {
    vector<vector<int>> states(num_sets, vector<int>(num_ways, U));
    return states;
}

void LRU(int address, vector<vector<int>> &cache, vector<vector<int>> &tag, int proc_id) {
    cout<<address<<endl;
    int offset = address % block_size;
    address /= block_size;
    int index = address % num_sets;
    address /= num_sets;
    int tag_value = address;

    // Find the way to replace: least recently used (smallest access time)
    int way_to_remove = 0;
    int min_time = INT_MAX;
    for (int way = 0; way < num_ways; ++way) {
        if (access_times[proc_id][index][way] < min_time) {
            min_time = access_times[proc_id][index][way];
            way_to_remove = way;
        }
    }
    cout<<"LRU way is "<<way_to_remove<<endl;
    int old_tag = tag[index][way_to_remove];
    int old_addr = old_tag * num_sets * block_size + index * block_size + offset;
    int old_state = states[proc_id][index][way_to_remove];
    
    if (old_state == M) {
        struct BusTransaction new_req = {proc_id, old_addr, 100, false, false, false, false, true};
        pending.pop_front();
        pending.push_front(new_req);
        stall[proc_id] = true;
        cout<<"Stalling processor " << proc_id << " for eviction\n";
        cout<<pending.front().cycles_remaining<<endl;
        // exit(1);
    }
    // Replace with new block
    // cout address replaced with new address
    cout << "Replacing address " << old_addr << " with address " << address * num_sets * block_size + index * block_size + offset << "\n";
    cout<<address<<" "<<index<<" "<<offset<<"\n";
    tag[index][way_to_remove] = tag_value;
    
    // Update access time for this way
    access_times[proc_id][index][way_to_remove] = curr_cycle;
    // exit(1);
}

void update_access_time(int address, int proc_id) {
        // Update access time for LRU
    int offset = address % block_size;
    int addr = address / block_size;
    int index = addr % num_sets;
    addr /= num_sets;
    int tag_val = addr;
    for (int way = 0; way < num_ways; ++way) {
        if (tag[proc_id][index][way] == tag_val) {
            access_times[proc_id][index][way] = curr_cycle;
            cout<<"Access time updated for address " << address << " in processor " << proc_id << "\n";
            cout << "New access time: " << access_times[proc_id][index][way] << "\n";
            break;
        }
    }

}

void insert_cache_line (vector<vector<int>> &cache, vector<vector<int>> &tag, int address, vector<bool> &is_full, int proc_id) {
    int temp = address;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    if (is_full[index]) {
        // cout information
        cout << "Cache is full for index " << index << "\n";
        cout << "Evicting line with tag " << tag[index][0] << "\n";
        cout << "Address in tag with this index: " << tag[index][1] * num_sets * block_size + index * block_size + offset << "\n";
        cout << "Processor ID: " << proc_id << "\n";
        // exit(1);
        LRU(temp, cache, tag, proc_id);
    } else {
        int i = 0;
        for (i = 0; i < num_ways; i++) {
            if (tag[index][i] == -1) {
                tag[index][i] = tag_value;
                access_times[proc_id][index][i] = curr_cycle;
                break;
            }
        }
        if (i == num_ways) {
            is_full[index] = true;
        }
    }
}



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

    // // Print parsed arguments (for debugging)
    // cout << "Tracefile: " << tracefile << "\n";
    // cout << "s: " << s << ", E: " << E << ", b: " << b << "\n";
    // cout << "Output file: " << outfile << "\n";

    // Your simulation logic goes here...
    inst_proc[0] = read_trace_files(tracefile, 0);
    inst_proc[1] = read_trace_files(tracefile, 1);
    inst_proc[2] = read_trace_files(tracefile, 2);
    inst_proc[3] = read_trace_files(tracefile, 3);
    for (int i = 0; i < 4; i++) {
        cache[i] = create_cache(1 << s, E, 1 << b);
        tag[i] = create_tag(1 << s, E);
        is_full[i] = create_is_full(1 << s);
        states[i] = initialize_states(1 << s, E);
        access_times[i] = create_access_times(1 << s, E);
    }
    num_sets = cache[0].size();
    num_ways = tag[0][0].size();
    block_size = cache[0][0].size() / num_ways;
    while (curr_inst[0] < inst_proc[0].size() || curr_inst[1] < inst_proc[1].size() || curr_inst[2] < inst_proc[2].size() || curr_inst[3] < inst_proc[3].size()) {
        // cout current instruction
        cout << "Current cycle: " << curr_cycle << "\n";
        for (int i = 0; i < 4; i++) {
            cout << "Core " << i << ": " << curr_inst[i] << "\n";
        }
        // cout address for top 4 inst
        cout << "Address: " << inst_proc[0][curr_inst[0]].second << "\n";
        cout << "Address: " << inst_proc[1][curr_inst[1]].second << "\n";
        cout << "Address: " << inst_proc[2][curr_inst[2]].second << "\n";
        cout << "Address: " << inst_proc[3][curr_inst[3]].second << "\n";
        if (!pending.empty()) { // transactions on the bus
            // cout information of top instr
            cout << "Processor ID: " << pending.front().proc_id << "\n";
            cout << "Address: " << pending.front().address << "\n";
            cout << "Cycles remaining: " << pending.front().cycles_remaining << "\n";
            cout << "Read: " << (pending.front().read ? "true" : "false") << "\n";
            cout << "Memory: " << (pending.front().mem ? "true" : "false") << "\n";
            cout << "RWITM: " << (pending.front().rwitm ? "true" : "false") << "\n";
            cout << "Invalidate: " << (pending.front().invalidate ? "true" : "false") << "\n";
            cout << "Eviction: " << (pending.front().eviction ? "true" : "false") << "\n";
            if (pending.front().cycles_remaining == -1) { // initiating transactions
                struct BusTransaction &trans = pending.front();
                bool found = false;
                int ind = 0;
                if (trans.read) {
                    for (int i = 0; i < 4; i++) {
                        if (i == trans.proc_id) continue; // this processor cannot snoop on self request
                        int state = obtain_state(trans.address, tag[i], states[i]);
                        if (state == E || state == S) {
                            cout<<"Get from another cache"<<endl;
                            found = true;
                            trans.cycles_remaining = 2 * block_size;
                            trans.mem = false;
                            ind = i;
                            break;
                        } else if (state == M) { // 2N then 100
                            found = true;
                            ind = i;
                            trans.mem = true;
                            trans.cycles_remaining = 2*block_size + 100;
                            break;
                        }
                    }
                    if (!found) {
                        trans.cycles_remaining = 100;
                        trans.mem = true;
                    }
                } else { // write miss
                    for (int i = 0; i < 4; i++) {
                        if (i == trans.proc_id) continue; // this processor cannot snoop on self request
                        int state = obtain_state(trans.address, tag[i], states[i]);
                        if (state == E || state == S) {
                            found = true;
                            ind = i;
                            trans.mem = true;
                            trans.cycles_remaining = 100;
                            trans.rwitm = true;
                            break;
                        } else if (state == M) { // 100 to mem then 100 from mem
                            found = true;
                            ind = i;
                            trans.mem = true;
                            trans.cycles_remaining = 200;
                            trans.rwitm = true;
                        }
                    }
                    if (!found) {
                        trans.mem = true;
                        trans.rwitm = true;
                        trans.cycles_remaining = 100;
                    }
                }
            }
            if (pending.front().cycles_remaining == 0) {
                struct BusTransaction completed = pending.front();
                int comp_id = completed.proc_id;

                stall[comp_id] = false;
                cout<<"Stall is "<<stall[comp_id]<<endl;
                if(completed.invalidate){
                    set_state(comp_id, tag[comp_id], states[comp_id], M);
                    for(int i = 0; i<4; i++){
                        if(i!=comp_id) set_state(i, tag[i], states[i], I);
                    }
                }
                if (completed.rwitm)  { // set state to M (and others to I)
                    insert_cache_line(cache[comp_id], tag[comp_id], completed.address, is_full[comp_id], comp_id);
                    // cout states
                    // if (curr_inst[2] == 10) exit(1);
                    set_state(completed.address, tag[comp_id], states[comp_id], M);
                    for (int i = 0; i < 4; i++) {
                        if (i == comp_id) continue;
                        set_state(completed.address, tag[i], states[i], I);
                    }
                    
                }
                else if (completed.read) {
                    insert_cache_line(cache[comp_id], tag[comp_id], completed.address, is_full[comp_id], comp_id);
                    
                    // here when eviction
                    set_state(completed.address, tag[comp_id], states[comp_id], E);
                    for (int i = 0; i < 4; i++) {
                        int state = obtain_state(completed.address, tag[i], states[i]);
                        cout << "State of address " << completed.address << " in processor " << i << ": ";
                        if (state == M) cout << "M\n";
                        else if (state == E) cout << "E\n";
                        else if (state == S) cout << "S\n";
                        else if (state == I) cout << "I\n";
                        else cout << "U\n";
                    } 
                    bool found = false;
                    for (int i = 0; i < 4; i++) {
                        if (i == comp_id) continue;
                        int other_state = obtain_state(completed.address, tag[i], states[i]);
                        if (other_state != U && other_state!=I) {
                            set_state(completed.address,tag[0],states[0],S);
                            set_state(completed.address,tag[1],states[1],S);
                            set_state(completed.address,tag[2],states[2],S);
                            set_state(completed.address,tag[3],states[3],S);
                            break;
                        }
                    } 
                    // cout state of this address in all processors
                    for (int i = 0; i < 4; i++) {
                        int state = obtain_state(completed.address, tag[i], states[i]);
                        cout << "State of address " << completed.address << " in processor " << i << ": ";
                        if (state == M) cout << "M\n";
                        else if (state == E) cout << "E\n";
                        else if (state == S) cout << "S\n";
                        else if (state == I) cout << "I\n";
                        else cout << "U\n";
                    } 
                    
                }
                // cout popped information
                cout << "Completed transaction for processor " << completed.proc_id << "\n";
                cout << "Stall "<<stall[completed.proc_id]<<endl;
                cout << "Address: " << completed.address << "\n";   
                cout << "Read: " << (completed.read ? "true" : "false") << "\n";
                cout << "Memory: " << (completed.mem ? "true" : "false") << "\n";   
                cout << "RWITM: " << (completed.rwitm ? "true" : "false") << "\n";
                cout << "Invalidate: " << (completed.invalidate ? "true" : "false") << "\n";
                cout << "Eviction: " << (completed.eviction ? "true" : "false") << "\n";
                if (pending.front().cycles_remaining == 0) {
                    pending.pop_front(); 
                }
            } else { // transaction in progress
                pending.front().cycles_remaining--;
                if (pending.front().mem && pending.front().cycles_remaining == 100 && pending.front().rwitm == false) { // now snooping continues writing to mem, seeker gets value
                    stall[pending.front().proc_id] = false;
                }
            }
        }
        cout<<"OUT"<<endl;
        for (int i = 0; i < 4; i++) {
            if (curr_inst[i] < inst_proc[i].size()) {
                pair<char, int> this_inst = inst_proc[i][curr_inst[i]];
                cout << "Processor " << i << ": " << this_inst.first << " " << this_inst.second << "\n";
                if (!stall[i]) {
                    cout<<"NOT stall"<<endl;
                    if (this_inst.first == 'R') {
                        int state = obtain_state(this_inst.second, tag[i], states[i]);
                        if (state == I || state == U) {
                            cout<<"Stalling processor " << i << " for read\n";
                            struct BusTransaction new_req = {i, inst_proc[i][curr_inst[i]].second, -1, (inst_proc[i][curr_inst[i]].first == 'R'), false, false, false, false};
                            stall[i] = true;
                            pending.push_back(new_req);
                        } else {
                            update_access_time(this_inst.second, i);
                            cout<<"Read hit on processor " << i << "\n";
                            cout << "Address: " << this_inst.second << "\n";
                            curr_inst[i]++;
                        }
                    } else {
                        int state = obtain_state(this_inst.second, tag[i], states[i]);
                        if (state == I || state == U) {
                            struct BusTransaction new_req = {i, inst_proc[i][curr_inst[i]].second, -1, (inst_proc[i][curr_inst[i]].first == 'R'), false, false, false, false};
                            stall[i] = true;
                            pending.push_back(new_req);
                        }
                        else if (state == M || state==E){
                            set_state(this_inst.second, tag[i], states[i], M);
                            update_access_time(this_inst.second, i);
                            curr_inst[i]++;
                            cout<<"Write hit on processor " << i << "\n";
                            cout << "Address: " << this_inst.second << "\n";
                        }
                        else {
                            struct BusTransaction new_req = {i, inst_proc[i][curr_inst[i]].second, 0, (inst_proc[i][curr_inst[i]].first == 'R'), false, false, true, false};
                            pending.push_back(new_req);
                            stall[i] = true;
                        }
                    }
                }
            }
        }
        curr_cycle++;
    }
    return 0;
}
