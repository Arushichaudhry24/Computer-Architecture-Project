#ifndef RRIP_REPL_H_
#define RRIP_REPL_H_

#include "repl_policies.h"
#include "mtrand.h"

// -----------------------------------------------------------------------------
// Replacement policies based on Re-Reference Interval Prediction (RRIP)
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Static RRIP (SRRIP)
// Maintains a fixed re-reference prediction value (RRPV) per cache line.
// -----------------------------------------------------------------------------
class SRRIPReplPolicy : public ReplPolicy {
protected:
    uint64_t* rrpvArray;   // Array storing RRPV for each cache line
    uint32_t  numLines;    // Total number of lines in the set
    uint32_t  maxRRPV;     // Maximum RRPV value indicating distant re-reference
    bool      justInserted;// Flag to skip update immediately after insertion

public:
    // Initialize SRRIP with specified number of lines and max RRPV
    explicit SRRIPReplPolicy(uint32_t _numLines, uint32_t _maxRRPV)
        : numLines(_numLines), maxRRPV(_maxRRPV), justInserted(false) {
        rrpvArray = gm_calloc<uint64_t>(numLines);
        // Set all lines to maximum RRPV (predict distant reuse)
        for (uint32_t i = 0; i < numLines; ++i)
            rrpvArray[i] = maxRRPV;
    }

    ~SRRIPReplPolicy() override {
        gm_free(rrpvArray);
    }

    // On cache hit: set RRPV to 0 (highest priority) unless this is the insertion step
    void update(uint32_t id, const MemReq* /*req*/) override {
        if (!justInserted)
            rrpvArray[id] = 0;
        justInserted = false;
    }

    // On insertion (replacement): assign RRPV = maxRRPV - 1 to new block
    void replaced(uint32_t id) override {
        rrpvArray[id]      = maxRRPV - 1;
        justInserted       = true;
    }

    // Select victim: find a line with RRPV == maxRRPV.
    // If none found, increment RRPV of all candidates and retry.
    template <typename C>
    uint32_t rank(const MemReq* /*req*/, C cands) {
        while (true) {
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpvArray[way] == maxRRPV)
                    return way;
            }
            // Age all candidate lines
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpvArray[way] < maxRRPV)
                    rrpvArray[way]++;
            }
        }
    }
    DECL_RANK_BINDINGS;
};

// -----------------------------------------------------------------------------
// Dynamic RRIP (DRRIP)
// Adapts insertion RRPV based on sampling to choose between SRRIP and BRRIP.
// -----------------------------------------------------------------------------
class DRRIPReplPolicy : public ReplPolicy {
protected:
    uint8_t* rrpv;         // Per-line RRPV storage
    uint32_t numLines;     // Number of lines in the set
    uint32_t maxRRPV;      // Maximum RRPV value
    uint32_t sampleExp;    // Exponent for sampling rate (1 => 1/2)
    MTRand   randGen;      // Random number generator

public:
    // Initialize DRRIP with max RRPV, sampling exponent, and line count
    DRRIPReplPolicy(uint32_t _maxRRPV,
                    uint32_t _sampleExp,
                    uint32_t _numLines,
                    uint32_t /*assoc*/)
        : numLines(_numLines),
          maxRRPV(_maxRRPV),
          sampleExp(_sampleExp),
          randGen(0xDEADBEEF) {
        rrpv = gm_calloc<uint8_t>(numLines);
        // On start, default all RRPVs to max (distant reuse)
        for (uint32_t i = 0; i < numLines; ++i)
            rrpv[i] = maxRRPV;
    }

    ~DRRIPReplPolicy() override {
        gm_free(rrpv);
    }

    // On cache hit: with probability 1/2^sampleExp, treat as long reuse (HP)
    void update(uint32_t id, const MemReq* /*req*/) override {
        if (randGen.randInt((1u << sampleExp) - 1) == 0)
            rrpv[id] = 0;
    }

    // On replacement: set new line to maxRRPV - 1 (same as SRRIP)
    void replaced(uint32_t id) override {
        rrpv[id] = maxRRPV - 1;
    }

    // Victim selection: same aging and search as in SRRIP
    template <typename C>
    uint32_t rank(const MemReq* /*req*/, C cands) {
        while (true) {
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] == maxRRPV)
                    return way;
            }
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] < maxRRPV)
                    rrpv[way]++;
            }
        }
    }
    DECL_RANK_BINDINGS;
};

// -----------------------------------------------------------------------------
// Write-Aware DRRIP (DRRIPW8)
// Prioritizes write hits (dirty blocks) with highest priority; ages clean blocks.
// -----------------------------------------------------------------------------
class DRRIPW8ReplPolicy : public ReplPolicy {
protected:
    uint8_t* rrpv;         // Per-line RRPV
    bool*    dirty;        // Dirty status for each line
    uint32_t numLines;     // Number of lines in the set
    uint32_t maxRRPV;      // Maximum RRPV

public:
    // Initialize DRRIPW8 with max RRPV and line count
    DRRIPW8ReplPolicy(uint32_t _maxRRPV,
                      uint32_t /*sampleProb*/,
                      uint32_t /*dirtyPrio*/,
                      uint32_t _numLines,
                      uint32_t /*assoc*/)
        : numLines(_numLines),
          maxRRPV(_maxRRPV) {
        rrpv  = gm_calloc<uint8_t>(numLines);
        dirty = gm_calloc<bool>(numLines);
        // Initialize all lines as distant reuse and clean
        for (uint32_t i = 0; i < numLines; i++) {
            rrpv[i]  = maxRRPV;
            dirty[i] = false;
        }
    }

    ~DRRIPW8ReplPolicy() override {
        gm_free(rrpv);
        gm_free(dirty);
    }

    // On cache hit:
    //  - Writes: mark dirty, set RRPV=0 (highest priority).
    //  - Reads: decrement RRPV if not zero (frequency priority).
    void update(uint32_t id, const MemReq* req) override {
        bool isWrite = (req->type == PUTX || req->type == PUTS);
        if (isWrite) {
            rrpv[id]  = 0;
            dirty[id] = true;
        } else if (rrpv[id] > 0) {
            rrpv[id]--;
        }
    }

    // On replacement: new line gets intermediate RRPV and is clean
    void replaced(uint32_t id) override {
        rrpv[id]   = maxRRPV - 1;
        dirty[id]  = false;
    }

    // Victim selection: same aging logic as SRRIP/DRRIP.
    // Prefers lines with RRPV == maxRRPV.
    template <typename C>
    uint32_t rank(const MemReq* /*req*/, C cands) {
        while (true) {
            // Search for victim
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] == maxRRPV)
                    return way;
            }
            // Age remaining lines
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] < maxRRPV)
                    rrpv[way]++;
            }
        }
    }
    DECL_RANK_BINDINGS;
};

#endif  // RRIP_REPL_H_
