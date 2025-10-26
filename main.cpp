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


    //parse optional command line arguments
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
            default:
                cerr << "Usage: " << argv[0]
                     << " [-n numAccesses] [-f availFrames] [-b bitUpdateInterval] [-l logMode] trace.tr <levelBits...>" << endl;
                exit(0);
        }
    }

    // Remaining (mandatory) args: trace file and level bits 
    if (optind >= argc) {
        cerr << "Usage: " << argv[0]
             << " [-n numAccesses] [-f availFrames] [-b bitUpdateInterval] [-l logMode] trace.tr <levelBits...>" << endl;
        exit(0);
    }

    // sets name of trace file and then increments optind to point to next arg
    string traceFile = argv[optind++];

    // check if trace file opens and exists
    ifstream infile(traceFile);
    if (!infile.is_open() || infile.fail()) {
        cerr << "Unable to open " << traceFile << endl;
        exit(0);
    }

    // parse remaining args: level bits
    int totalBits = 0;
    for (int i = optind, level = 0; i < argc; i++, level++) {
        int bits = atoi(argv[i]);
        if (bits < 1) {
            cerr << "Level " << level << " page table must be at least 1 bit" << endl;
            exit(0);
        }
        totalBits += bits;
        levelBits.push_back(bits);
    }

    // total number of bits used in all page tables combined cannot exceed 28
    if (totalBits > 28) {
        cerr << "Too many bits used in page tables" << endl;
        exit(0);
    }

    // testing
    cout << "Trace file: " << traceFile << endl;
    cout << "Levels: ";
    for (int b : levelBits) cout << b << " ";
    cout << endl;
    cout << "n=" << numAccesses << ", f=" << availFrames 
         << ", b=" << bitUpdateInterval << ", l=" << logMode << endl;

    return 0;
}