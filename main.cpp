/**
 * Author: Gilad Bitton
 * RedID: 130621085
 */
#include <iostream>
#include <vector>
#include <pthread.h>
#include <fstream>
#include <cassert>
#include <unistd.h>
#include <sstream>

#include "log_helpers.h"
#include "pageTable.h"
#include "vaddr_tracereader.h"

using namespace std;

// Logs the bitmasks for all page table levels
static int run_bitmasks(PageTable& pt) {
    log_bitmasks(pt.numLevels, pt.bitmasks.data());
    return 0;
}

// Logs the virtual to physical address translations
static int run_va2pa(const string& traceFile, PageTable& pt, int numAccesses, int numFrames) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        std::cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace;
    int count = 0;
    int nextFreePFN = 0;

    // if numAccesses is <= 0, process all accesses, otherwise process only first numAccesses 
    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        unsigned int vaddr = mTrace.addr; // extract virtual address
        Map* mapping = pt.searchMappedPfn(vaddr);

        // if the mapping does not exist, it's a miss
        if (!mapping) {
            // a miss, need to insert new mapping
            if (nextFreePFN >= numFrames) {
                cerr << "Out of physical frames\n";
                fclose(tf);
                return 1;
            }
            pt.insertMapForVpn2Pfn(vaddr, nextFreePFN++);
            mapping = pt.searchMappedPfn(vaddr);
        }

        // at this point there would be a valid mapping for the virtual address to a physical frame
        uint32_t paddr = (uint32_t(mapping->pfn) << pt.offsetBits) | pt.getOffset(vaddr); // constructs physical address by left shifting by offset bits and then adding offset bits
        log_va2pa(vaddr, paddr);
        count++;
    }
    fclose(tf);
    return 0;
    
}

// Logs the VPN pieces and PFN for each memory access
static int run_vpns_pfn(const string& traceFile, PageTable& pt, int numAccesses, int numFrames) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        std::cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace;
    int count = 0;
    int nextFreePFN = 0;

    // if numAccesses is <= 0, process all accesses, otherwise process only first numAccesses 
    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        unsigned int vaddr = mTrace.addr; // extract virtual address

        // extract VPN pieces for all levels of VPN address
        vector<unsigned> vpnPieces(pt.numLevels);
        for (int i = 0; i < pt.numLevels; i++) {
            vpnPieces[i] = pt.getVPNPiece(vaddr, i);
        }

        // lookup PFN mapping
        Map* mapping = pt.searchMappedPfn(vaddr);

        // if the mapping does not exist, it's a miss
        if (!mapping) {
            // a miss, need to insert new mapping
            if (nextFreePFN >= numFrames) {
                cerr << "Out of physical frames\n";
                fclose(tf);
                return 1;
            }
            pt.insertMapForVpn2Pfn(vaddr, nextFreePFN++);
            mapping = pt.searchMappedPfn(vaddr);
        }

        int pfn = mapping && mapping->valid ? mapping->pfn : -1;

        log_vpns_pfn(pt.numLevels, vpnPieces.data(), pfn);
        count++;
    }
    fclose(tf);
    return 0;

}

// Logs the offset for each memory access
static int run_offset(const string& traceFile, PageTable& pt, int numAccesses) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        std::cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace;
    int count = 0;

    // if numAccesses is <= 0, process all accesses, otherwise process only first numAccesses 
    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        unsigned int vaddr = mTrace.addr; // extract virtual address
        unsigned offset = pt.getOffset(vaddr);
        print_num_inHex(offset);
        count++;
    }
    fclose(tf);
    return 0;
}

// static int run_summary(const string& traceFile, PageTable& pt, int numAccesses) {
//     FILE* tf = fopen(traceFile.c_str(), "rb");
//     if (!tf) {
//         std::cerr << "Unable to open " << traceFile << '\n';
//         return 1;
//     }

//     p2AddrTr mTrace;
//     int count = 0;
//     int nextFreePFN = 0;
//     int pageSize =  pt.pageSizeBytes();
//     int addressesProcessed = 0;
//     int hits = 0;
//     int misses = 0;


//     // if numAccesses is <= 0, process all accesses, otherwise process only first numAccesses 
//     while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {

//     }
//     fclose(tf);
//     return 0;
// }

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

    PageTable pt;
    pt.initFromLevelBits(levelBits);

    // logging based on logMode
    if(logMode == "bitmasks") {
        return run_bitmasks(pt);
    } else if(logMode == "va2pa") {
        return run_va2pa(traceFile, pt, numAccesses, availFrames);
    } else if(logMode == "vpns_pfn") {
        return run_vpns_pfn(traceFile, pt, numAccesses, availFrames);
    } else if(logMode == "offset") {
        return run_offset(traceFile, pt, numAccesses);
    }

    return 0;
}