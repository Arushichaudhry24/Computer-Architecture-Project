#ifndef RRIP_REPL_H_
#define RRIP_REPL_H_

#include "repl_policies.h"
#include "mtrand.h"

// -----------------------------------------------------------------------------
// Static Re-Reference Interval Prediction (SRRIP)
// -----------------------------------------------------------------------------
class SRRIPReplPolicy : public ReplPolicy {
protected:
    uint64_t* array;   // per-line RRPV
    uint32_t  numLines;
    uint32_t  rpvMax;  // max RRPV value
    bool      inInserted;

public:
    explicit SRRIPReplPolicy(uint32_t _numLines, uint32_t _rpvMax)
        : numLines(_numLines), rpvMax(_rpvMax), inInserted(false) {
        array = gm_calloc<uint64_t>(numLines);
        for (uint32_t i = 0; i < numLines; ++i)
            array[i] = rpvMax;
    }

    ~SRRIPReplPolicy() override {
        gm_free(array);
    }

    void update(uint32_t id, const MemReq* /*req*/) override {
        if (!inInserted)  // skip on the miss insertion step
            array[id] = 0;
        inInserted = false;
    }

    void replaced(uint32_t id) override {
        array[id]      = rpvMax - 1;
        inInserted     = true;
    }

    template <typename C>
    uint32_t rank(const MemReq* /*req*/, C cands) {
        while (true) {
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                if (array[*it] == rpvMax)
                    return *it;
            }
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                array[*it]++;
            }
        }
    }
    DECL_RANK_BINDINGS;
};

// -----------------------------------------------------------------------------
// Dynamic Re-Reference Interval Prediction (DRRIP)
// -----------------------------------------------------------------------------
class DRRIPReplPolicy : public ReplPolicy {
protected:
    uint8_t* rrpv;
    uint32_t numLines;
    uint32_t rrpvMax;
    uint32_t sampleProb;  // exponent: 1 => 1/2 chance
    MTRand   rnd;

public:
    DRRIPReplPolicy(uint32_t _rrpvMax,
                    uint32_t _sampleProb,
                    uint32_t _numLines,
                    uint32_t /*assoc*/)
        : numLines(_numLines)
        , rrpvMax(_rrpvMax)
        , sampleProb(_sampleProb)
        , rnd(0xDEADBEEF) {
        rrpv = gm_calloc<uint8_t>(numLines);
    }

    ~DRRIPReplPolicy() override {
        gm_free(rrpv);
    }

    void update(uint32_t id, const MemReq* /*req*/) override {
        // HP with probability 1/2^sampleProb
        if (rnd.randInt((1u << sampleProb) - 1) == 0)
            rrpv[id] = 0;
    }

    void replaced(uint32_t id) override {
        rrpv[id] = rrpvMax - 1;  // Fixed: changed rpvMax to rrpvMax
    }

    template <typename C>
    uint32_t rank(const MemReq* /*req*/, C cands) {
        while (true) {
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] == rrpvMax)  // Fixed: correctly uses rrpvMax
                    return way;
            }
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] < rrpvMax)  // Fixed: correctly uses rrpvMax
                    rrpv[way]++;
            }
        }
    }
    DECL_RANK_BINDINGS;
};

// -----------------------------------------------------------------------------
// DRRIPW8: Write-Aware DRRIP
// -----------------------------------------------------------------------------
class DRRIPW8ReplPolicy : public ReplPolicy {
protected:
    uint8_t* rrpv;     // per-line RRPV
    bool*    dirty;    // per-line dirty bit tracking
    uint32_t numLines;
    uint32_t rrpvMax;  // max RRPV value

public:
    DRRIPW8ReplPolicy(uint32_t _rrpvMax,
                      uint32_t /*sampleProb*/,
                      uint32_t /*dirtyPrio*/,
                      uint32_t _numLines,
                      uint32_t /*assoc*/)
        : numLines(_numLines)
        , rrpvMax(_rrpvMax) {
        rrpv = gm_calloc<uint8_t>(numLines);
        dirty = gm_calloc<bool>(numLines);
        
        // Initialize all lines to distant re-reference
        for (uint32_t i = 0; i < numLines; i++) {
            rrpv[i] = rrpvMax;
            dirty[i] = false;
        }
    }

    ~DRRIPW8ReplPolicy() override {
        gm_free(rrpv);
        gm_free(dirty);
    }

    void update(uint32_t id, const MemReq* req) override {
        bool isWrite = (req->type == PUTX || req->type == PUTS);
        
        if (isWrite) {
            // Mark as dirty and apply Hit Priority (HP) - set to 0
            rrpv[id] = 0;
            dirty[id] = true;
        } else {
            // For clean blocks, apply Frequency Priority (FP) - decrement if not 0
            if (rrpv[id] > 0)
                rrpv[id]--;
        }
    }

    void replaced(uint32_t id) override {
        // New blocks get intermediate RRPV (rrpvMax - 1)
        rrpv[id] = rrpvMax - 1;
        dirty[id] = false;
    }

    template <typename C>
    uint32_t rank(const MemReq* /*req*/, C cands) {
        while (true) {
            // First try to find any victim with RRPV = max
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] == rrpvMax)
                    return way;
            }
            
            // If not found, age all blocks and retry
            for (auto it = cands.begin(); it != cands.end(); it.inc()) {
                uint32_t way = *it;
                if (rrpv[way] < rrpvMax)
                    rrpv[way]++;
            }
        }
    }
    DECL_RANK_BINDINGS;
};

#endif  // RRIP_REPL_H_