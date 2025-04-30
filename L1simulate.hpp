#include <bits/stdc++.h>
using namespace std;
enum Cache_Line_State {
    U = -1,
    M,
    E,
    S,
    I
};

int bus_stall = 0;

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
vector<vector<vector<int>>> states(4);
vector<vector<vector<int>>> tag(4);
vector<vector<vector<int>>> access_times(4);
vector<vector<pair<char, int>>> inst_proc(4);
vector<vector<bool>> is_full(4);
vector<int> curr_inst(4, 0);
vector<int> stall(4, 0);
vector<int> evictions(4, 0);
vector<int> writebacks(4, 0);
vector<int> execution_cycles(4, 0), idle_cycles(4, 0), num_misses(4, 0), total_cycles(4, -1), invalidations_per_core(4, 0), trafficpercore(4, 0);
long long int bustraffic = 0;
int num_sets, num_ways = -1, block_size;

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

void set_state (int address, vector<vector<int>> &tag, vector<vector<int>> &states, int state, int proc_id) {
    int temp = address;
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

void LRU(int address, vector<vector<int>> &tag, int proc_id) {
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
    int old_tag = tag[index][way_to_remove];
    int old_addr = old_tag * num_sets * block_size + index * block_size + offset;
    int old_state = states[proc_id][index][way_to_remove];
    
    if (old_state == M) { // todo
        trafficpercore[proc_id] += block_size;
        bustraffic += block_size;
        stall[proc_id] += 100;
        writebacks[proc_id]++;
        bus_stall += 100;
    }
    // cout<<proc_id<<" "<<"Evicting address: "<<old_addr<<" with "<<address*num_sets*block_size+index*block_size+offset<<endl;
    tag[index][way_to_remove] = tag_value;
}

void update_access_time(int address, int proc_id, int cycle) { // update if present
    int offset = address % block_size;
    int addr = address / block_size;
    int index = addr % num_sets;
    addr /= num_sets;
    int tag_val = addr;
    for (int way = 0; way < num_ways; ++way) {
        if (tag[proc_id][index][way] == tag_val) {
            // cout<<"Updating access time for address: "<<address<<" to "<<cycle<<endl;
            access_times[proc_id][index][way] = cycle;
            break;
        }
    }

}

void insert_cache_line (vector<vector<int>> &tag, int address, vector<bool> &is_full, int proc_id) {
    int temp = address;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    if (is_full[index]) {
        evictions[proc_id]++;
        LRU(temp, tag, proc_id);    
    } else {
        // cout<<"trying to add address: "<<address*num_sets*block_size+index*block_size+offset<<" to cache line "<<index<<endl;
        int i = 0;
        for (i = 0; i < num_ways; i++) {
            // cout<<tag[index][i]<<" ";
            if (tag[index][i] == -1) {
                // cout<<"Adding address: "<<address*num_sets*block_size+index*block_size+offset<<" to cache line "<<index<<endl;
                tag[index][i] = tag_value;
                break;
            }
        }
        // cout<<endl;
        if (i == num_ways) {
            is_full[index] = true;
        }
    }
}