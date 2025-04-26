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
    bool this_cycle;
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
vector<vector<vector<int>>> tag(4);
vector<vector<pair<char, int>>> inst_proc(4);
vector<vector<bool>> is_full(4);
vector<struct BusTransaction> bus;
vector<int> curr_inst(4, 0);
vector<bool> stall(4, false);
int num_sets, num_ways, block_size;
void snoop(int proc_id) {
    // Snoop on the bus using data in its cache
    for (int i = 0; i < bus.size(); i++) {
        if (bus[i].proc_id != proc_id) {
            int address = bus[i].address;
            int offset = address % block_size;
            address /= block_size;
            int index = address % num_sets;
            address /= num_sets;
            int tag_value = address;
            for (int j = 0; j < num_ways; j++) {
                if (tag[proc_id][index][j] == tag_value) {
                    // Found the address in the cache
                    if (bus[i].this_cycle) {
                        // Update the state of the cache line
                        if (bus[i].cycles_remaining == 0) {
                            // Update the state to I
                            tag[proc_id][index][j] = I;
                        }
                    }
                }
            }
        }
    }
}

bool comp(const pair<int, int>& a, const pair<int, int>& b) {
    return a.second < b.second; // max-heap by second
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
bool find_addr (int address, vector<vector<int>> &cache, vector<vector<int>> &tag) {
    int num_sets = cache.size();
    int num_ways = tag[0].size();
    int block_size = cache[0].size() / num_ways;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    for (int i = 0; i < num_ways; i++) {
        if (tag[index][i] == tag_value) {
            return true;
        }
    }
    return false;
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
        for (auto req:bus) {
            req.cycles_remaining--;
            req.this_cycle = false;
        }
        // Snooping
        // Core 0
        // Snoop on the bus using data in its cache
        // Core 1
        // Snoop on the bus using data in its cache
        // Core 2
        // Snoop on the bus using data in its cache
        // Core 3
        // Snoop on the bus using data in its cache
        // Initialize bus transactions if any
        // Core 0
    
        // Core 1

        // Core 2

        // Core 3
        

    }
    return 0;
}
