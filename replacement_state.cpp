/*****************************************************************************/
/*                      STEPS TO RUN                                         */
/* 1. To run a policy, you can use the ENABLE macro to enable a policy       */
/*    and disable the other two policy in lines 37-39                        */
/* 2. By default, all three algorithms (SHiP2.0/ RRIP/ Set-Dueling) run on   */
/*    L2 and use LRU for L1 and L3.                                          */
/*    To run the algorithms on different levels, change the associvity       */
/*    For example, if you want to implement SHiP2.0 on L3 and LRU on L1      */
/*    and L2 change the code as below:                                       */
/*    if (assoc == 4 || assoc == 8)                                          */
/*       LRU                                                                 */
/*   else                                                                    */
/*       SHiP                                                                */
/* 3. To run normal SHiP on L2, change the condition in line 376             */
/*    if (flag == 0)                                                         */
/*                                                                           */
/*****************************************************************************/


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <map>
#include <iostream>

using namespace std;

#include "replacement_state.h"

#define ENABLE 1
#define DISABLE 0

#define SHIP_2_0_POLICY             ENABLE
#define RRIP_POLICY                 DISABLE
#define SET_DUELING_POLICY          DISABLE


/* map to store PC values based on tag+setIndex as key */
map <unsigned long long int, unsigned long long int> pc_map;
/* block to be evicted for RRIP algorithm */
INT32 replace_block = 0;

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

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/


////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol )
{

    numsets    = _sets;
    assoc      = _assoc;
    replPolicy = _pol;

    mytimer    = 0;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream & CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Statistics ================"<<endl;
    out<<"=========================================================="<<endl;

    // CONTESTANTS:  Insert your statistics printing here

    return out;

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways

    repl  = new LINE_REPLACEMENT_STATE* [ numsets ];

    // ensure that we were able to create replacement state

    assert(repl);

    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++)
    {
        repl[ setIndex ]  = new LINE_REPLACEMENT_STATE[ assoc ];

        for(UINT32 way=0; way<assoc; way++)
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ way ].LRUstackposition = way;
#if SHIP_2_0_POLICY
            /* Initialize variables for SHiP */
            repl[ setIndex ][ way ].sign=0;
            repl[ setIndex ][ way ].outcome=0;
#elif RRIP_POLICY
            /* Initialize variables for RRIP */
            repl[ setIndex ][ way ].RRPV_counter=3;
#endif
        }
    }

    if (replPolicy != CRC_REPL_CONTESTANT) return;

#if SHIP_2_0_POLICY
    /* Creating a table to maintain the signature counter */
    signature_table = new UINT64[tablesize];
    for(int i = 0;i < tablesize; i++)
        signature_table[i] = 0;

#elif SET_DUELING_POLICY
    /* Initialize variables for set-dueling */
    policy_counter = 0;
    policy_selector = 0;
    /* Creating a table to maintain the counter */
    sd_counter = new int[numsets];
    for(UINT32 i = 0; i < numsets; i++)
        sd_counter[i] = 6;
#endif

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// argument is the set index. The return value is the physical way            //
// index for the line being replaced.                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType, UINT32 accessSource ) {
    // If no invalid lines, then replace based on replacement policy
    if( replPolicy == CRC_REPL_LRU )
    {
        return Get_LRU_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        return Get_Random_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR VICTIM SELECTION FUNCTION HERE
	return Get_My_Victim (setIndex);
    }

    // We should never here here

    assert(0);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState(
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit, UINT32 accessSource )
{
    // What replacement policy?
    if( replPolicy == CRC_REPL_LRU )
    {
        UpdateLRU( setIndex, updateWayID );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        // Random replacement requires no replacement state update
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR UPDATE REPLACEMENT STATE FUNCTION HERE
        // Feel free to use any of the input parameters to make
        // updates to your replacement policy
        UpdateMyPolicy (setIndex, updateWayID, currLine, PC, accessType, cacheHit);
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim( UINT32 setIndex )
{
	// Get pointer to replacement state of current set

	LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
	INT32   lruWay   = 0;

	// Search for victim whose stack position is assoc-1

	for(UINT32 way=0; way<assoc; way++) {
		if (replSet[way].LRUstackposition == (assoc-1)) {
			lruWay = way;
			break;
		}
	}

	// return lru way

	return lruWay;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim( UINT32 setIndex )
{
    INT32 way = (rand() % assoc);

    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID )
{
	// Determine current LRU stack position
	UINT32 currLRUstackposition = repl[ setIndex ][ updateWayID ].LRUstackposition;

	// Update the stack position of all lines before the current line
	// Update implies incremeting their stack positions by one

	for(UINT32 way=0; way<assoc; way++) {
		if( repl[setIndex][way].LRUstackposition < currLRUstackposition ) {
			repl[setIndex][way].LRUstackposition++;
		}
	}

	// Set the LRU stack position of new line to be zero
	repl[ setIndex ][ updateWayID ].LRUstackposition = 0;
}

INT32 CACHE_REPLACEMENT_STATE::Get_My_Victim( UINT32 setIndex ) {

#if SHIP_2_0_POLICY
    /* Using LRU to evict the victim in the set since SHiP can be used in conjunction with LRU */
    return Get_LRU_Victim( setIndex );


#elif RRIP_POLICY
    if (assoc == 4 || assoc == 16)
    {
        /* Using LRU to evict the victim for L1 and L2 */
        return Get_LRU_Victim (setIndex);
    }
    else
    {
        /* victim block which has the largest RRPV (re-reference predicted value)*/
        return replace_block;
    }

#elif SET_DUELING_POLICY
    /* Using LRU for all levels of cache */
    return Get_LRU_Victim (setIndex);
#endif
}

void CACHE_REPLACEMENT_STATE::UpdateMyPolicy(UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, Addr_t PC, UINT32 accessType, bool cacheHit)
{

#if SHIP_2_0_POLICY
    /* Hashing the PC which is used as a signature */
    UINT64 pc_initial=(PC)%(tablesize);

    if (assoc == 4)
    {
        /* Store the PC value with tag+setIndex as key when L1 is accessed */
        pc_map[currLine->tag + setIndex] = pc_initial;
    }
    if (assoc == 4 || assoc == 16)
    {
        /* Use LRU for L1 and L3 */
        UpdateLRU (setIndex, updateWayID);
    }
    else
    {
        /* SHiP for L2 */
        /* Retrive the PC for the current block */
        map <unsigned long long int, unsigned long long int> :: iterator it = pc_map.find (currLine->tag + setIndex);
        pc_initial = it->second;
        UINT64 pc_counter = repl[ setIndex ][ updateWayID ].sign;
        int flag = 0;

        if(cacheHit == 1)
        {
            /* Cache hit, increment the signature counter */
            signature_table[pc_counter]++;
            repl[ setIndex ][ updateWayID ].outcome = 1;
        }
        else
        {
            /* Cache miss */
            if(repl[ setIndex ][ updateWayID ].outcome == 0)
            {
                /* Decrement the signature counter */
                if(signature_table[pc_counter] > 0)
                {
                    signature_table[pc_counter]--;
                }
            }
            /* Reset outcome */
            repl[ setIndex ][ updateWayID ].outcome = 0;
            /* Assign the new signature */
            repl[ setIndex ][ updateWayID ].sign = pc_initial;
            /* If the signature counter is 0, insert the block in LRU */
            if(signature_table[pc_initial] == 0)
            {
                flag = 1;
            }
        }

        if(flag == 1)
        {
            /* Do LRU */
            UpdateLRU (setIndex, updateWayID);
        }
    }

#elif RRIP_POLICY
    if (assoc == 4 || assoc == 16)
    {
        /* Use LRU for L1 and L3 */
        UpdateLRU (setIndex, updateWayID);
    }
    else
    {
        if (cacheHit == true)
        {
            /* Reset RRPV counter on cache hit */
            repl[setIndex][updateWayID].RRPV_counter = 0;
        }
        else
        {
            /* Find the block to be evicted */
            while(1)
            {
                for(UINT32 way=0; way<assoc; way++)
                {
                    /* If the block to be evicted is found, remember it so that it can be evicted and set the counter to 2 */
                    if (repl[ setIndex ][way].RRPV_counter == 3)
                    {
                        replace_block = way;
                        repl[ setIndex ][way].RRPV_counter = 2;
                        return;
                    }
                }

                /* If the block is not found, then increment the counter for all the block till you get the evicted block */
                for(UINT32 way=0; way<assoc; way++)
                {
                    repl[ setIndex ][way].RRPV_counter++;
                }
            }
        }
    }
#elif SET_DUELING_POLICY
    if (assoc == 4 || assoc == 16)
    {
        /* Use LRU for L1 and L3 */
        UpdateLRU (setIndex, updateWayID);
    }
    else
    {
        /* Identify LRU sets, MRU set and follower sets */
        UINT32 set_identifier = ((setIndex) & 511);
        UINT32 lru = 0, mru = 0;

        if(set_identifier == 511)
        {
            /* MRU sets since last 7 bits are 1 */
            mru = 1;
        }
        else if(set_identifier == 0)
        {
            /* LRU sets since last 7 bits are 0 */
            lru = 1;
        }
        else
        {
            /* Follower sets. Select the policy*/
            if(policy_selector == 1)
                mru = 1;
            else
                lru = 1;
        }

        if(cacheHit == 0)
        {
            /* Cache miss */
            if(set_identifier == 511)
            {
                /* Decrement the counter for MRU */
                policy_counter = policy_counter - 1;
            }
            if(set_identifier == 0)
            {
                /* Increment the counter for LRU */
                policy_counter = policy_counter + 1;
            }

            /* If the set follows MRU, then decrement the counter */
            if(mru == 1)
            {
                sd_counter[setIndex] = sd_counter[setIndex] - 1;
            }
        }

        /* Reset the counter if it saturates */
        if(policy_counter >= 1024)
        {
            policy_counter = 1023;
        }
        else if(policy_counter < 0)
        {
            policy_counter = 0;
        }


        /* Select the policy based on the counter */
        if(policy_counter > 128)
        {
            /* Selecing MRU policy */
            policy_selector = 1;
        }
        else
        {
            /* Selecting LRU policy */
            policy_selector = 0;
        }

        /* Based on previous policy_selector, do LRU or MRU */

        /* LRU */
        if(lru == 1 || sd_counter[setIndex] == 0 || cacheHit == 1)
        {
            UpdateLRU (setIndex, updateWayID);
        }

        /* MRU */
        if(mru == 1 && cacheHit == 0 && sd_counter[setIndex] != 0)
            repl[setIndex][updateWayID].LRUstackposition = assoc-1;

        /* Reset the counter */
        if(sd_counter[setIndex] == 0)
            sd_counter[setIndex] = 2;
    }
#endif
}

CACHE_REPLACEMENT_STATE::~CACHE_REPLACEMENT_STATE (void) {
}
