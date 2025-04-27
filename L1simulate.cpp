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
vector<vector<priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>>>> lru(4); // (last access time, index of way in tag)
vector<vector<vector<int>>> cache(4);
vector<vector<vector<int>>> states(4);
vector<vector<vector<int>>> tag(4);
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
    int num_sets = tag.size();
    int num_ways = tag[0].size();
    int block_size = tag[0].size();
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
    int num_sets = tag.size();
    int num_ways = tag[0].size();
    int block_size = tag[0].size()/ num_ways;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    for (int i = 0; i < num_ways; i++) {
        if (tag[index][i] == tag_value) {
            states[index][i] = state;
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
vector<bool> create_is_full (int num_sets) {
    vector<bool> is_full (num_sets, false);
    return is_full;
}
vector<vector<int>> initialize_states (int num_sets, int num_ways) {
    vector<vector<int>> states(num_sets, vector<int>(num_ways, U));
    return states;
}
void insert_cache_line (vector<vector<int>> &cache, vector<vector<int>> &tag, int address, vector<bool> &is_full, int proc_id) {
    int num_sets = cache.size();
    int num_ways = tag[0].size();
    int block_size = cache[0].size() / num_ways;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    if (is_full[index]) {
        LRU(address, cache, tag, proc_id);
    } else {
        int i = 0;
        for (i = 0; i < num_ways; i++) {
            if (tag[index][i] == -1) {
                tag[index][i] = tag_value;
                lru[proc_id][index].push({curr_cycle, i}); // update LRU
                break;
            }
        }
        if (i == num_ways) {
            is_full[index] = true;
        }
    }
}

void LRU (int address, vector<vector<int>> &cache, vector<vector<int>> &tag, int proc_id) {
    int num_sets = cache.size();
    int num_ways = tag[0].size();
    int block_size = cache[0].size() / num_ways;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    int way_to_remove = (lru[proc_id][index].top()).second;

    lru[proc_id][index].pop();
    int old_tag = tag[index][way_to_remove];
    int old_addr = old_tag*num_sets*block_size + index*block_size + offset;
    int old_state = states[proc_id][index][way_to_remove];
    if (old_state==M){
        struct BusTransaction new_req = {proc_id,old_addr, 100, false,false, false, false, true};
        pending.push_front(new_req);
        stall[proc_id] = true;
    }
    tag[index][way_to_remove] = tag_value; // insert new cache block

    lru[proc_id][index].push({curr_cycle, way_to_remove}); // update LRU
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
        lru[i] = vector<priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>>> (1 << s);
        states[i] = initialize_states(num_sets, num_ways);
    }
    num_sets = cache.size();
    num_ways = tag[0].size();
    block_size = cache[0].size() / num_ways;
    cout<<"Number of read instructions per core: "<<"\n";
    for (int i = 0; i < 4; i++) {
        int read_count = 0;
        for (const auto& line : inst_proc[i]) {
            if (line.first == 'R') {
                read_count++;
            }
        }
        cout << "Core " << i << ": " << read_count << "\n";
    }
    cout<<"Number of write instructions per core: "<<"\n";
    for (int i = 0; i < 4; i++) {
        int write_count = 0;
        for (const auto& line : inst_proc[i]) {
            if (line.first == 'W') {
                write_count++;
            }
        }
        cout << "Core " << i << ": " << write_count << "\n";
    }
    
    while (curr_inst[0] < inst_proc[0].size() || curr_inst[1] < inst_proc[1].size() || curr_inst[2] < inst_proc[2].size() || curr_inst[3] < inst_proc[3].size()) {
        // Snooping
        // Core 0
        // Snoop on the bus using data in its cache
        if (!pending.empty()) { // transactions on the bus
            if (pending.front().cycles_remaining == 0) {
                struct BusTransaction completed = pending.front();
                int comp_id = completed.proc_id;

                stall[comp_id] = false;
                if(completed.invalidate){
                    set_state(comp_id, tag[comp_id], states[comp_id], M);
                    for(int i = 0; i<4; i++){
                        if(i!=comp_id) set_state(i, tag[i], states[i], I);
                    }
                }
                if (completed.rwitm)  { // set state to M (and others to I)
                    set_state(completed.address, tag[comp_id], states[comp_id], M);
                    for (int i = 0; i < 4; i++) {
                        if (i == comp_id) continue;
                        set_state(completed.address, tag[i], states[i], I);
                    }
                    insert_cache_line(cache[comp_id], tag[comp_id], completed.address, is_full[comp_id], comp_id);
                }
                else if (completed.read) {
                    set_state(completed.address, tag[comp_id], states[comp_id], E);
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
                    insert_cache_line(cache[comp_id], tag[comp_id], completed.address, is_full[comp_id], comp_id);
                }
                pending.pop_front();
                if (!pending.empty()) { // initiating transaction
                    struct BusTransaction trans = pending.front();
                    bool found = false;
                    int ind = 0;
                    if (trans.read) {
                        for (int i = 0; i < 4; i++) {
                            if (i == trans.proc_id) continue; // this processor cannot snoop on self request
                            int state = obtain_state(trans.address, tag[i], states[i]);
                            if (state == E || state == S) {
                                found = true;
                                ind = i;
                                break;
                            } else if (state == M) { // 2N then 100
                                found = true;
                                ind = i;
                                trans.mem = true;
                                trans.cycles_remaining = 2*block_size + 100; // stall shifting
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
            } else { // transaction in progress
                pending.front().cycles_remaining--;
                if (pending.front().mem && pending.front().cycles_remaining == 100 && pending.front().rwitm == false) { // now snooping continues writing to mem, seeker gets value
                    stall[pending.front().proc_id] = false;
                }
            }
        }
        // Core 1
        // Snoop on the bus using data in its cache
        // Core 2
        // Snoop on the bus using data in its cache
        // Core 3
        // Snoop on the bus using data in its cache


        // Initialize bus transactions if any
        // Core 0
        for (int i = 0; i < 4; i++) {
            if (curr_inst[i] < inst_proc[i].size()) {
                pair<char, int> this_inst = inst_proc[i][curr_inst[i]];
                if (!stall[i]) {
                    if (this_inst.first == 'R') {
                        int state = obtain_state(this_inst.second, tag[i], states[i]);
                        if (state == I || state == U) {
                            struct BusTransaction new_req = {i, inst_proc[i][curr_inst[i]].second, (inst_proc[i][curr_inst[i]].first == 'R'), false, false, false, false};
                            stall[i] = true;
                            pending.push_back(new_req);
                        } else {
                            curr_inst[i]++;
                        }
                    } else {
                        int state = obtain_state(this_inst.second, tag[i], states[i]);
                        if (state == I || state == U) {
                            struct BusTransaction new_req = {i, inst_proc[i][curr_inst[i]].second, (inst_proc[i][curr_inst[i]].first == 'R'), false, false, false, false};
                            stall[i] = true;
                            pending.push_back(new_req);
                        }
                        else if (state == M || state==E){
                            set_state(this_inst.second, tag[i], states[i], M);
                            curr_inst[i]++;
                        }
                        else {
                            struct BusTransaction new_req = {i, inst_proc[i][curr_inst[i]].second, (inst_proc[i][curr_inst[i]].first == 'R'), false, false, true, false};
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
