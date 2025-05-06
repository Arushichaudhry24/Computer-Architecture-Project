#ifndef RRIP_REPL_H_
#define RRIP_REPL_H_

#include "repl_policies.h"


// Static RRIP
class SRRIPReplPolicy : public ReplPolicy {
    protected:
        // add class member variables here
        uint32_t* array;
        uint32_t numLines;
        uint32_t rpvMax;
    
    public:
        // add member methods here, refer to repl_policies.h
        explicit SRRIPReplPolicy(uint32_t _numLines, uint32_t _rpvMax) : numLines(_numLines), rpvMax(_rpvMax) { //Initializing SRRIP policy with numLines, rpv max value
            array = gm_calloc<uint32_t>(numLines);
            for (uint32_t i= 0; i <numLines; ++i){ //initializing the array with rpvMax as it's the first one
                array[i]= rpvMax;//looping througgh
            }
        }
        ~SRRIPReplPolicy() override { //Destructor
            gm_free(array); //freeing memory space
        }
        
        void update(uint32_t id, const MemReq*) override{ //update
            array[id] = 0; //set rrpv to be equal to 0 as it was recently hit, low eviction
        }
        
        void replaced(uint32_t id) override{ //replace
            array[id] = rpvMax -1; //let the new block's rrpv equal to rpvMax-1
        }
        
        template <typename C> inline uint32_t rank(const MemReq*, C cands) { //rank
            while(true){
                for(auto ci= cands.begin(); ci != cands.end(); ci.inc() ){ //loop through the cache
                    if(array[*ci] == rpvMax){ //if we find an element with the maximum rrpv Value, evict
                        return *ci; //evict
                    }
                }
                
                for(auto ci= cands.begin(); ci !=cands.end(); ci.inc()){ //if we do not find any rrpv value equal to the maximum value
                    if(array[*ci]< rpvMax){ //loop through the cache again
                        array[*ci]+=1; //increement each block's rrpv until we finally find one equal to rpvMax
                    }
                    
                    
                }
                
                
            }
        }

        DECL_RANK_BINDINGS;
};

//SLRU policy
class SLRUReplPolicy : public ReplPolicy {
private:
    uint8_t* segment;     // segment to see if it is probationary or not
    uint64_t* array;      //  timestamps
    uint32_t numLines;
    uint32_t protectedLimit; //limit for protected segment
    uint32_t protectedCount;
    uint64_t timestamp;

public:
    explicit SLRUReplPolicy(uint32_t _numLines, uint32_t _protectedLimit) : numLines(_numLines), protectedLimit(_protectedLimit),protectedCount(0),timestamp(1) {
        segment = gm_calloc<uint8_t>(numLines);  // 0=probationary
        array = gm_calloc<uint64_t>(numLines);   // timestamp datastuff
    }

    ~SLRUReplPolicy() override { //destructor
        gm_free(segment);
        gm_free(array);
    }

    void update(uint32_t id, const MemReq* req) override { //update
        array[id] = timestamp++;
        if (segment[id] ==0 && protectedCount<protectedLimit) { //ensuring under limit
            segment[id] =1; //making it protected when it is hit
            protectedCount++; //increasing count forprotected
        }
    }

    void replaced(uint32_t id) override { //replace
        if (segment[id] == 1){ //decrementing if it was protected
            protectedCount--;
        }
        segment[id]= 0; //making it probationary again..
        array[id] =timestamp++; //timestamp increments
    }

    template <typename C> inline uint32_t rank(const MemReq* req, C cands) {
        uint32_t bestProbationary = -1; //finding best candidates
        uint32_t bestProtected = -1;
        uint64_t bestProbTs = (uint64_t)-1;
        uint64_t bestProtTs = (uint64_t)-1;

        for (auto ci= cands.begin(); ci !=cands.end(); ci.inc()){
            uint32_t id= *ci;
            if (segment[id] ==0) {
                if (array[id] < bestProbTs) {//looping through probationary
                    bestProbationary = id;
                    bestProbTs = array[id];
                }
            } else {
                if (array[id] <bestProtTs) { //if probationary empty, looping through protected
                    bestProtected = id;
                    bestProtTs = array[id];
                }
            }
        }

        return (bestProbationary!= (uint32_t)-1) ? bestProbationary : bestProtected; //evicting
    }

    DECL_RANK_BINDINGS;
};




#endif // RRIP_REPL_POLICY_H_
