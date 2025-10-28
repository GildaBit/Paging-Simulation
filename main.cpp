/**
 * Author: Gilad Bitton
 * RedID: 130621085
 *
 * Program overview:
 * - Builds a multi-level page table from level bit widths.
 * - Reads a binary virtual-address trace and simulates translation + NFU replacement.
 * - Supports multiple logging modes (bitmasks, va2pa, vpns_pfn, offset, summary, vpn2pfn_pr).
 *
 * Key collaborators (headers you provide):
 *   log_helpers.h     : logging/printing helpers (e.g., log_va2pa, log_summary, etc.)
 *   pageTable.h       : PageTable class (init, indexing, mapping insert/search, etc.)
 *   vaddr_tracereader.h : NextAddress() that yields p2AddrTr { uint32_t addr; ... }
 *   nfu.h             : NFU state + APIs (initNFUState, onHitNFU, onMissNFU, isFullNFU, selectVictimNFU, reuseSlotNFU, beforeAccessNFU, nfuState)
 */

#include <cassert>
#include <fstream>
#include <iostream>
#include <pthread.h>   // (appears unused here; possibly needed elsewhere in your project)
#include <sstream>
#include <unistd.h>
#include <vector>

#include "log_helpers.h"
#include "nfu.h"
#include "pageTable.h"
#include "vaddr_tracereader.h"

using namespace std;

/*──────────────────────────────────────────────────────────────────────────────┐
│ Helpers per log mode                                                         │
└──────────────────────────────────────────────────────────────────────────────*/

/**
 * bitmasks mode:
 * Print bitmasks for each page table level.
 */
static int run_bitmasks(PageTable& pt) {
    log_bitmasks(pt.numLevels, pt.bitmasks.data());
    return 0;
}

/**
 * va2pa mode:
 * For each address, produce virtual→physical translation using the page table +
 * NFU replacement policy. Logs the final physical address for each access.
 *
 * @param numAccesses If <= 0: process entire trace; else: only first numAccesses.
 */
static int run_va2pa(const string& traceFile, PageTable& pt, int numAccesses, int /*numFrames*/) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace{};
    int count = 0;
    int nextFreePFN = 0;

    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        const uint32_t vaddr = mTrace.addr;

        beforeAccessNFU();
        const uint32_t vpn = vaddr >> pt.offsetBits;

        Map* mapping = pt.searchMappedPfn(vaddr);

        if (mapping && mapping->valid) {
            // Page table hit
            onHitNFU(vpn);
        } else {
            // Page table miss
            if (!isFullNFU()) {
                // Free frame available: install mapping
                pt.insertMapForVpn2Pfn(vaddr, nextFreePFN);
                onMissNFU(vpn, nextFreePFN);
                mapping = pt.searchMappedPfn(vaddr);
                nextFreePFN++;
            } else {
                // Must evict victim selected by NFU
                const int victimIndex = selectVictimNFU();
                const int victimPFN   = nfuState.pages[victimIndex].pfn;

                // reuseSlotNFU returns (oldVPN, oldBitstring), and advances to hold 'vpn'
                const auto oldInfo  = reuseSlotNFU(victimIndex, vpn);
                const uint32_t oldVaddr = oldInfo.first << pt.offsetBits;

                // Invalidate old mapping in the page table
                if (Map* oldMapping = pt.searchMappedPfn(oldVaddr)) {
                    oldMapping->valid = false;
                }

                // Insert the new mapping
                pt.insertMapForVpn2Pfn(vaddr, victimPFN);
                mapping = pt.searchMappedPfn(vaddr);
            }
        }

        // Construct physical address = (PFN << offsetBits) | offset
        const uint32_t paddr = (uint32_t(mapping->pfn) << pt.offsetBits) | pt.getOffset(vaddr);
        log_va2pa(vaddr, paddr);

        count++;
    }

    fclose(tf);
    return 0;
}

/**
 * vpns_pfn mode:
 * For each access, log the VPN pieces at each level and the PFN (if mapped).
 */
static int run_vpns_pfn(const string& traceFile, PageTable& pt, int numAccesses, int /*numFrames*/) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace{};
    int count = 0;
    int nextFreePFN = 0;

    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        const uint32_t vaddr = mTrace.addr;

        // Extract multi-level VPN pieces
        vector<unsigned> vpnPieces(pt.numLevels);
        for (int i = 0; i < pt.numLevels; i++) {
            vpnPieces[i] = pt.getVPNPiece(vaddr, i);
        }

        beforeAccessNFU();
        const uint32_t vpn = vaddr >> pt.offsetBits;

        Map* mapping = pt.searchMappedPfn(vaddr);

        if (mapping && mapping->valid) {
            onHitNFU(vpn);
        } else {
            if (!isFullNFU()) {
                pt.insertMapForVpn2Pfn(vaddr, nextFreePFN);
                onMissNFU(vpn, nextFreePFN);
                mapping = pt.searchMappedPfn(vaddr);
                nextFreePFN++;
            } else {
                const int victimIndex = selectVictimNFU();
                const int victimPFN   = nfuState.pages[victimIndex].pfn;

                const auto oldInfo    = reuseSlotNFU(victimIndex, vpn);
                const uint32_t oldVaddr = oldInfo.first << pt.offsetBits;

                if (Map* oldMapping = pt.searchMappedPfn(oldVaddr)) {
                    oldMapping->valid = false;
                }

                pt.insertMapForVpn2Pfn(vaddr, victimPFN);
                mapping = pt.searchMappedPfn(vaddr);
            }
        }

        const int pfn = (mapping && mapping->valid) ? mapping->pfn : -1;
        log_vpns_pfn(pt.numLevels, vpnPieces.data(), pfn);

        count++;
    }

    fclose(tf);
    return 0;
}

/**
 * offset mode:
 * For each access, log only the page offset.
 */
static int run_offset(const string& traceFile, PageTable& pt, int numAccesses) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace{};
    int count = 0;

    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        const uint32_t vaddr = mTrace.addr;
        const unsigned offset = pt.getOffset(vaddr);
        print_num_inHex(offset);
        count++;
    }

    fclose(tf);
    return 0;
}

/**
 * summary mode:
 * Produce a compact summary of the simulation:
 *  - page size, page replacements, hits, total addresses processed,
 *    frames allocated (first-time allocations), and number of PTEs.
 */
static int run_summary(const string& traceFile, PageTable& pt, int numAccesses) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace{};
    int count = 0;
    int nextFreePFN = 0;

    const unsigned pageSize          = pt.pageSizeBytes();
    unsigned addressesProcessed      = 0;
    unsigned hits                    = 0;
    unsigned pageReplacements        = 0;
    unsigned framesAllocated         = 0;
    unsigned numEntries              = 0;

    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        const uint32_t vaddr = mTrace.addr;

        beforeAccessNFU();
        const uint32_t vpn   = vaddr >> pt.offsetBits;

        Map* mapping = pt.searchMappedPfn(vaddr);

        if (mapping && mapping->valid) {
            hits++;
            onHitNFU(vpn);
        } else {
            if (!isFullNFU()) {
                framesAllocated++;
                pt.insertMapForVpn2Pfn(vaddr, nextFreePFN);
                onMissNFU(vpn, nextFreePFN);
                mapping = pt.searchMappedPfn(vaddr);
                nextFreePFN++;
            } else {
                pageReplacements++;
                const int victimIndex = selectVictimNFU();
                const int victimPFN   = nfuState.pages[victimIndex].pfn;

                const auto oldInfo    = reuseSlotNFU(victimIndex, vpn);
                const uint32_t oldVaddr = oldInfo.first << pt.offsetBits;

                if (Map* oldMapping = pt.searchMappedPfn(oldVaddr)) {
                    oldMapping->valid = false;
                }

                pt.insertMapForVpn2Pfn(vaddr, victimPFN);
                mapping = pt.searchMappedPfn(vaddr);
            }
        }

        count++;
    }

    addressesProcessed = count;
    numEntries         = pt.countEntries(&pt);

    log_summary(pageSize, pageReplacements, hits, addressesProcessed, framesAllocated, numEntries);

    fclose(tf);
    return 0;
}

/**
 * vpn2pfn_pr mode:
 * For each access, log the (vpn, pfn) mapping, whether it was a page-table hit,
 * and when replacement occurs also log victim vpn and its NFU bitstring.
 */
static int run_vpn2pfn_pr(const string& traceFile, PageTable& pt, int numAccesses) {
    FILE* tf = fopen(traceFile.c_str(), "rb");
    if (!tf) {
        cerr << "Unable to open " << traceFile << '\n';
        return 1;
    }

    p2AddrTr mTrace{};
    int count = 0;
    int nextFreePFN = 0;

    while ((numAccesses <= 0 || count < numAccesses) && NextAddress(tf, &mTrace)) {
        const uint32_t vaddr = mTrace.addr;

        bool pthit           = false;
        int  vpnReplaced     = -1;
        int  victimBitstring = 0;

        beforeAccessNFU();
        const uint32_t vpn = vaddr >> pt.offsetBits;

        Map* mapping = pt.searchMappedPfn(vaddr);

        if (mapping && mapping->valid) {
            pthit = true;
            onHitNFU(vpn);
        } else {
            if (!isFullNFU()) {
                pt.insertMapForVpn2Pfn(vaddr, nextFreePFN);
                onMissNFU(vpn, nextFreePFN);
                mapping = pt.searchMappedPfn(vaddr);
                nextFreePFN++;
            } else {
                const int victimIndex = selectVictimNFU();
                const int victimPFN   = nfuState.pages[victimIndex].pfn;

                vpnReplaced     = nfuState.pages[victimIndex].vpn;
                victimBitstring = nfuState.pages[victimIndex].bitstring;

                const auto oldInfo    = reuseSlotNFU(victimIndex, vpn);
                const uint32_t oldVaddr = oldInfo.first << pt.offsetBits;

                if (Map* oldMapping = pt.searchMappedPfn(oldVaddr)) {
                    oldMapping->valid = false;
                }

                pt.insertMapForVpn2Pfn(vaddr, victimPFN);
                mapping = pt.searchMappedPfn(vaddr);
            }
        }

        const int pfn = (mapping && mapping->valid) ? mapping->pfn : -1;
        log_mapping(vpn, pfn, vpnReplaced, victimBitstring, pthit);

        count++;
    }

    fclose(tf);
    return 0;
}

/*──────────────────────────────────────────────────────────────────────────────┐
│ main                                                                          │
└──────────────────────────────────────────────────────────────────────────────*/

int main(int argc, char** argv) {
    int opt               = 0;
    int numAccesses       = -1;       // If not specified: process all accesses
    int availFrames       = 999999;   // Total NFU capacity (simulated free frames)
    int bitUpdateInterval = 10;       // NFU age/bitstring update period (in accesses)
    string logMode        = "summary";
    vector<int> levelBits;

    // Parse optional flags: -n (numAccesses), -f (frames), -b (bit interval), -l (log mode)
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

    // Required positional args: trace file, then list of level bit widths
    if (optind >= argc) {
        cerr << "Usage: " << argv[0]
             << " [-n numAccesses] [-f availFrames] [-b bitUpdateInterval] [-l logMode] trace.tr <levelBits...>" << endl;
        exit(0);
    }

    const string traceFile = argv[optind++];

    // Verify the trace file can be opened (for erroring out early)
    ifstream infile(traceFile);
    if (!infile.is_open() || infile.fail()) {
        cerr << "Unable to open " << traceFile << endl;
        exit(0);
    }

    // Read level bit widths
    int totalBits = 0;
    for (int i = optind, level = 0; i < argc; i++, level++) {
        const int bits = atoi(argv[i]);
        if (bits < 1) {
            cerr << "Level " << level << " page table must be at least 1 bit" << endl;
            exit(0);
        }
        totalBits += bits;
        levelBits.push_back(bits);
    }

    // Sanity: total VPN bits must not exceed 28 (leaves at least offset bits)
    if (totalBits > 28) {
        cerr << "Too many bits used in page tables" << endl;
        exit(0);
    }

    // Initialize page table and NFU system
    PageTable pt;
    pt.initFromLevelBits(levelBits);

    initNFUState(availFrames, bitUpdateInterval);

    // Dispatch selected log mode
    if (logMode == "bitmasks") {
        return run_bitmasks(pt);
    } else if (logMode == "va2pa") {
        return run_va2pa(traceFile, pt, numAccesses, availFrames);
    } else if (logMode == "vpns_pfn") {
        return run_vpns_pfn(traceFile, pt, numAccesses, availFrames);
    } else if (logMode == "offset") {
        return run_offset(traceFile, pt, numAccesses);
    } else if (logMode == "summary") {
        return run_summary(traceFile, pt, numAccesses);
    } else if (logMode == "vpn2pfn_pr") {
        return run_vpn2pfn_pr(traceFile, pt, numAccesses);
    }

    // Unknown mode: treat as no-op success
    return 0;
}
