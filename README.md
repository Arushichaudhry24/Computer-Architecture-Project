# DRRIPW8 and SLRU Cache Replacement Policy – ZSim Implementation

## Overview

This project explores and evaluates two advanced cache replacement policies—**DRRIPW8 (Dynamic Re-Reference Interval Prediction with 8-bit counters)** and **SLRU (Segmented Least Recently Used)**—within the ZSim simulator. These policies aim to improve cache performance, particularly for shared L3 caches in multicore systems, by reducing cache misses and minimizing costly writebacks to memory.

Our motivation stems from the limitations of traditional policies like LRU and LFU, which tend to perform poorly under workloads with irregular or bursty access patterns. DRRIPW8 offers dynamic adaptability using set-dueling and differentiated handling of clean and dirty cache blocks. SLRU enhances LRU by managing blocks in protected and probationary segments to avoid cache pollution.

## Environment

* **Simulator**: [ZSim](https://zsim.csail.mit.edu/) (fast and cycle-accurate)
* **Benchmarks**: SPEC CPU2006 (single-core focus)
* **Platform**: Linux system (multi-core simulation enabled)
* **Language**: C++ (for integration into ZSim)
* **Tools used for plotting**: Python (matplotlib/pandas), or Excel for final visuals

## DRRIPW8 Implementation Summary

* Uses 2-bit RRPVs (Re-reference Prediction Values)
* Employs **8-bit saturating counters** for adaptive policy selection
* Implements **set-dueling** to alternate between short and long re-reference intervals
* Differentiates promotion based on block state:
  * Clean blocks use frequency-based promotion (FP)
  * Dirty blocks use hit-priority promotion (HP)
 
## SLRU Implementation Summary

* Divides each cache set into two segments: **probationary** and **protected**
* Inserts all new blocks into the **probationary segment**
* Promotes blocks to the **protected segment** upon a second access
* Maintains separate **LRU chains** within each segment
* Prioritizes evictions from the **probationary segment** to preserve frequently reused data
* Falls back to evicting the **least recently used block in the protected segment** if needed
* Prevents cache pollution by isolating one-time-use blocks
* Lightweight and adaptable, offering improved performance in workloads with dynamic access patterns


## Modified Files and Run Instructions

### Files Modified for Cache Replacement Policies

The following files in the ZSim repository were modified to implement and support the **DRRIPW8** and **SLRU** cache replacement policies:

1. **`init.cpp`**

   * Updated to register new cache replacement policies (`drripw8` and `slru`) so they can be selected via configuration files.

2. **`rrip_repl.h`**

   * Modified to include logic for **DRRIPW8**, including 8-bit saturating counters, set-dueling mechanism, and differentiated promotion for clean and dirty blocks.
   * Also includes SLRU implementation: two-segment structure with promotion, demotion, and LRU management within each segment.

3. **`hw4runscript`**

   * Edited to automate the execution of all benchmarks with the newly implemented replacement policies using their respective configuration files.

4. **Benchmark Configuration Files (in `/configs/`)**



### Run Instructions

Each benchmark can be run directly using its corresponding configuration file without requiring any further changes to the script or setup. The simulator reads the `"replPolicy"` field from the config file to initialize the appropriate cache replacement logic.



## Simulation & Metrics

Each SPEC benchmark was run for **100 million instructions** using representative simpoints. For DRRIPW8 and SLRU, simulations were integrated into ZSim’s cache replacement module. Key performance metrics extracted include:

* **Total Cycles** = sum of `cycles` and `cCycles` for all cores
* **IPC (Instructions Per Cycle)** = total instructions / total cycles
* **MPKI (Misses Per Kilo Instructions)** = (L3 misses / total instructions) \* 1000
  (L3 misses = `mGETS` + `mGETXIM` + `mGETXSM`)
* **Writebacks** = clean (`PUTS`) + dirty (`PUTX`) evictions from L3

These values were parsed from the `zsim.out` files.


## Graphing Methodology

* Extracted data such as **total cycles**, **instructions**, **IPC**, **MPKI**, and **L3 writebacks (PUTS and PUTX)** from `zsim.out` files
* Used **regular expressions** and manual parsing to organize benchmark outputs into structured tables
* Focused specifically on **L3 cache behavior**, as it reflects the impact of the replacement policy
* Imported processed data into **MATLAB** for visualization
* Plotted comparative graphs for each policy (**LRU**, **SRRIP-HP**, **SLRU**, **DRRIPW8**) across all SPEC CPU2006 benchmarks
* Visualizations were used to highlight performance differences in cache efficiency, miss rates, and memory writebacks

## Structure

```
/src         # Modified ZSim source code with DRRIPW8 and SLRU
/data        # zsim.out files for all benchmarks
/scripts     # Python scripts used for log parsing and graphing
/report      # Final PDF report
README.md    # This file
```

## Contributors

* Arushi Chaudhry
* Samarth Pahwa

Electrical & Computer Engineering
Texas A\&M University
