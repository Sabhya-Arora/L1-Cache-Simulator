// #include "L1.simulate.hpp"

#include <iostream>
#include <unistd.h>     // for getopt
#include <cstdlib>      // for atoi
#include <string>

void print_help() {
    std::cout << "Usage: ./L1simulate [options]\n"
              << "-t <tracefile> : name of parallel application (e.g., app1) whose 4 traces are to be used\n"
              << "-s <s>         : number of set index bits (number of sets = 2^s)\n"
              << "-E <E>         : associativity (number of cache lines per set)\n"
              << "-b <b>         : number of block bits (block size = 2^b)\n"
              << "-o <filename>  : output filename to log output for plotting\n"
              << "-h             : print this help message\n";
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <utility> // for std::pair

std::vector<std::pair<char, int>> read_trace_files(const std::string& base_name, int i) {
    std::vector<std::pair<char, int>> lines;

    std::string filename = base_name + "_proc" + std::to_string(i) + ".trace";
    std::ifstream infile(filename);

    if (!infile) {
        std::cerr << "Error: Could not open " << filename << "\n";
        return lines; // instead of continue, since this is not in a loop anymore
    }

    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        char op;
        std::string hex_addr;

        if (!(iss >> op >> hex_addr)) {
            std::cerr << "Warning: Failed to parse line: " << line << "\n";
            continue;
        }

        // Convert hex string to int
        int addr = std::stoi(hex_addr, nullptr, 16);

        lines.emplace_back(op, addr);
    }

    infile.close();
    return lines;
}


int main(int argc, char* argv[]) {
    std::string tracefile;
    std::string outfile;
    int s = -1, E = -1, b = -1;

    int opt;
    while ((opt = getopt(argc, argv, "t:s:E:b:o:h")) != -1) {
        switch (opt) {
            case 't':
                tracefile = optarg;
                break;
            case 's':
                s = std::atoi(optarg);
                break;
            case 'E':
                E = std::atoi(optarg);
                break;
            case 'b':
                b = std::atoi(optarg);
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
        std::cerr << "Missing required arguments.\n";
        print_help();
        return 1;
    }

    // Print parsed arguments (for debugging)
    std::cout << "Tracefile: " << tracefile << "\n";
    std::cout << "s: " << s << ", E: " << E << ", b: " << b << "\n";
    std::cout << "Output file: " << outfile << "\n";

    // Your simulation logic goes here...
    std::vector<std::pair<char, int>> inst_proc0 = read_trace_files(tracefile, 0);
    std::vector<std::pair<char, int>> inst_proc1 = read_trace_files(tracefile, 1);
    std::vector<std::pair<char, int>> inst_proc2 = read_trace_files(tracefile, 2);
    std::vector<std::pair<char, int>> inst_proc3 = read_trace_files(tracefile, 3);
    

    return 0;
}
