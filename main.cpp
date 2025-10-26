/**
 * Author: Gilad Bitton
 * RedID: 130621085
 */
#include <iostream>
#include <vector>
#include <pthread.h>
#include <fstream>
#include <unistd.h>
#include <sstream>


using namespace std;

int main(int argc, char** argv) {
    int opt = 0;
    int numAccesses = -1; // Processes only the first numAccesses memory references
    int availFrames = 999999; // Number of available physical frames
    int bitUpdateInterval = 10; // Interval for updating the bitstring in memory accesses
    string logMode = "summary"; // Specifies what to be printed to the standard output
    vector<int> levelBits; // Number of bits for each level in the multi-level page


    //parse command line arguments
    while ((opt = getopt(argc, argv, "n:f:b:l:")) != -1) {
        switch (opt) {
            case 'n':
                numAccesses = atoi(optarg);
                if (numAccesses < 1) {
                    cerr << "Number of memory accesses must be a number and greater than 0" << endl;
                    exit(0);
                }
                break;
            case 'f':
                availFrames = atoi(optarg);
                if (availFrames < 1) {
                    cerr << "Number of available frames must be a number and greater than 0" << endl;
                    exit(0);
                }
                break;
            case 'b':
                bitUpdateInterval = atoi(optarg);
                if (bitUpdateInterval < 1) {
                    cerr << "Bit string update interval must be a number and greater than 0" << endl;
                    exit(0);
                }
                break;
            case 'l':
                logMode = optarg;
                break;
        }
    }
    if (optind >= argc) {
        cout << "Usage: " << argv[0] << " <input_file> [-s strategy] [-q quantum]\n";
        exit(0);
    }

    // get text file and check if it opens and exists
    string input_file = argv[optind];
    ifstream infile(input_file);
    if (!infile.is_open() || infile.fail()) {
        cout << "Unable to open " << input_file << endl;
        exit(0);
    }

    vector<vector<int>> processes;
    string line;

    // parse input file for the processes
    while(getline(infile, line)) {
        if(line.empty()) continue;
        istringstream iss(line);
        vector<int> process;
        int burst;
        while (iss >> burst) {
            if (burst <= 0) {
                cout << "A burst number must be bigger than 0" << endl;
                exit(0);
            }
            process.push_back(burst);
        }
        if (process.size() % 2 == 0) {
            cout << "There must be an odd number of bursts for each process" << endl;
            exit(0);
        }
        processes.push_back(process);
    }

    return 0;
}