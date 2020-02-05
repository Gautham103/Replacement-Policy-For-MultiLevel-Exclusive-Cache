#ifndef REPL_STATE_H
#define REPL_STATE_H

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cassert>
#include "utils.h"
#include "crc_cache_defs.h"
#include <iostream>

#define tablesize 1<<16    // Last 16 bits(PC) are used to hash the table for getting the signature
#define max_threshold 36   // The maximum value of threshold beyond which the counter saturates
#define threshold 6
using namespace std;

// Replacement Policies Supported
typedef enum
{
    CRC_REPL_LRU        = 0,
    CRC_REPL_RANDOM     = 1,
    CRC_REPL_CONTESTANT = 2
} ReplacemntPolicy;

#define TABLE_SIZE 1<<10

// Replacement State Per Cache Line
typedef struct
{
    UINT32  LRUstackposition;
    /* signature SHiP */
    UINT64 sign;
    /* outcome for SHiP */
    bool outcome;
    /* RRPV counter for RRIP */
    INT32 RRPV_counter;

    // CONTESTANTS: Add extra state per cache line here

} LINE_REPLACEMENT_STATE;

struct sampler; // Jimenez's structures

// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{
public:
    LINE_REPLACEMENT_STATE   **repl;
  private:

    UINT32 numsets;
    UINT32 assoc;
    UINT32 replPolicy;

    COUNTER mytimer;  // tracks # of references to the cache

    // CONTESTANTS:  Add extra state for cache here

    /* Table to store the signature */
    UINT64 *signature_table;


  public:
    /* Policy counter which increments on LRU and decrements on MRU */
    INT32 policy_counter;
    /* Selects MRU or LRU based on policy counter */
    INT32 policy_selector;
    /* Pointer containing the counter value per set */
    INT32 *sd_counter;

  public:
    ostream & PrintStats(ostream &out);

    // The constructor CAN NOT be changed
    CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol );

    INT32 GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType, UINT32 accessSource);

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID);

    void   SetReplacementPolicy( UINT32 _pol ) { replPolicy = _pol; }
    void   IncrementTimer() { mytimer++; }

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
                                   UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit, UINT32 accessSource);

    ~CACHE_REPLACEMENT_STATE(void);

  private:

    void   InitReplacementState();
    INT32  Get_Random_Victim( UINT32 setIndex );

    INT32  Get_LRU_Victim( UINT32 setIndex );
    INT32  Get_My_Victim( UINT32 setIndex );
    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID );
    void   UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, Addr_t PC, UINT32 accessType, bool cacheHit );
};

#endif
