#include <bits/stdc++.h>
using namespace std;

void print_help() {
    cout << "Usage: ./L1simulate [options]\n"
              << "-t <tracefile> : name of parallel application (e.g., app1) whose 4 traces are to be used\n"
              << "-s <s>         : number of set index bits (number of sets = 2^s)\n"
              << "-E <E>         : associativity (number of cache lines per set)\n"
              << "-b <b>         : number of block bits (block size = 2^b)\n"
              << "-o <filename>  : output filename to log output for plotting\n"
              << "-h             : print this help message\n";
}

int curr_time = 0;

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

bool find_addr (int address, vector<vector<int>> cache, vector<vector<int>> tag) {
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

void LRU (int address, vector<vector<int>> &cache, vector<vector<int>> &tag, priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> &lru) {
    int num_sets = cache.size();
    int num_ways = tag[0].size();
    int block_size = cache[0].size() / num_ways;
    int offset = address % block_size;
    address/=block_size;
    int index = address % num_sets;
    address/=num_sets;
    int tag_value = address;
    int way_to_remove = (lru.top()).second;
    lru.pop();
    tag[index][way_to_remove] = tag_value; // insert new cache block
    lru.push({curr_time, way_to_remove}); // update LRU
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
    vector<vector<pair<char, int>>> inst_proc(4);
    inst_proc[0] = read_trace_files(tracefile, 0);
    inst_proc[1] = read_trace_files(tracefile, 1);
    inst_proc[2] = read_trace_files(tracefile, 2);
    inst_proc[3] = read_trace_files(tracefile, 3);
    vector<vector<vector<int>>> cache(4);
    vector<vector<vector<int>>> tag(4);
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> lru; // (last access time, index of way in tag)
    for (int i = 0; i < 4; i++) {
        cache[i] = create_cache(1 << s, E, 1 << b);
    }
    for (int i = 0; i < 4; i++) {
        tag[i] = create_tag(1 << s, E);
    }
    int num_sets = cache.size();
    int num_ways = tag[0].size();
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

    return 0;
}
