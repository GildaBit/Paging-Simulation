Author: Gilad Bitton

Multilevel Paging Simulator (C++)

A C++ simulator for virtual memory translation using multi-level page tables and a Not Frequently Used (NFU) page replacement policy.
Reads a binary address trace and reports detailed address translations, replacements, and statistics.

Features:

    Configurable N-level page table (custom bit widths per level)

    NFU replacement with 16-bit aging counters

    Multiple log modes:

        bitmasks → show level masks
        va2pa → virtual → physical translations
        vpns_pfn → per-level VPN and PFN
        summary → hits, replacements, entries, etc.
        vpn2pfn_pr → full mapping + victim bitstrings

    Modular design with PageTable, Level, and NFUState classes

Build

mkdir build && cd build
cmake ..
make -j

Run

./pagingwithpr [options] trace.tr <levelBits...>

Example: 

./pagingwithpr -n 50 -f 20 -b 10 -l vpn2pfn_pr input_files/trace.tr 6 6 8

Options

Flag	Description
-n	Number of accesses to simulate
-f	Available physical frames
-b	NFU bit aging interval
-l	Log mode (summary, va2pa, etc.)
