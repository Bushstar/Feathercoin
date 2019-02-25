// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "primitives/block.h"
#include "uint256.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    int nHeight = pindexLast->nHeight + 1;
    int nTargetTimespan = params.nPowTargetTimespan;
    int nTargetSpacing = params.nPowTargetSpacing;
    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    int64_t nActualTimespan = 0;
    int nActualTimespanMax = nTargetTimespan*4;
    int nActualTimespanMin = nTargetTimespan/4;

    int pindexFirstShortTime = 0;
    int pindexFirstMediumTime = 0;
    int nActualTimespanShort = nTargetTimespan;
    int nActualTimespanMedium = nTargetTimespan;
    int nActualTimespanLong = nTargetTimespan;
    int nActualTimespanAvg = 0;

    /*
    / New eHRC from start for testnet
    */
    
    if (Params().NetworkIDString() == CBaseChainParams::TESTNET ) {
        nActualTimespan = pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime();

        // 99 / 66 = 1.5 AKA 50% difficulty change limiter
        // 100 / 75 = 1.25 AKA 25% difficulty change limiter
      //  nActualTimespanMax = nTargetTimespan * 100 / 25;
      //  nActualTimespanMin = nTargetTimespan * 50 / 100;
        

        int shortInterval = 10;
        int mediumInterval = 240;
        int longInterval = 2880;  // = 60*48
        int shortWeight = 64;
        int mediumWeight = 4;
        int longWeight = 1;
        
        // damping to 30%
        int dampingFactor = 4;
        int dampingDivisor = 5;
    
      
           
          
        const CBlockIndex* pindexFirstLong = pindexLast;
        for(int i = 0; pindexFirstLong && i < (longInterval - 1)&& i < nHeight - 1; i++) {
            pindexFirstLong = pindexFirstLong->pprev;
            if (i == shortInterval - 1)
                pindexFirstShortTime = pindexFirstLong->GetBlockTime();
            if (i == mediumInterval - 1)
                pindexFirstMediumTime = pindexFirstLong->GetBlockTime();
        }

        if (pindexLast->GetBlockTime() - pindexFirstShortTime != 0)
            nActualTimespanShort = (pindexLast->GetBlockTime() - pindexFirstShortTime) / shortInterval;

        if (pindexLast->GetBlockTime() - pindexFirstMediumTime != 0)
            nActualTimespanMedium = (pindexLast->GetBlockTime() - pindexFirstMediumTime) / mediumInterval;

        if (pindexLast->GetBlockTime() - pindexFirstLong->GetBlockTime() != 0)
            nActualTimespanLong = (pindexLast->GetBlockTime() - pindexFirstLong->GetBlockTime()) / longInterval;

        nActualTimespanAvg = (nActualTimespanShort * shortWeight) + (nActualTimespanMedium * mediumWeight) + (nActualTimespanLong * longWeight);
        nActualTimespanAvg /= (shortWeight + mediumWeight + longWeight);
        
        // damping 
        nActualTimespan = nActualTimespanAvg + (dampingFactor * nTargetTimespan);
        nActualTimespan /= dampingDivisor;
        
        // if (nHeight >=tForkThree) {
            // 2018_12_10
        //diff downward change limiter  to 50% of long average
	   if (nActualTimespanLong > 0 ) {
            	nActualTimespanMax = nActualTimespanLong*2;
	   }

     //   }
     
     
     
        if(nActualTimespan < nActualTimespanMin)
            nActualTimespan = nActualTimespanMin;
        if(nActualTimespan > nActualTimespanMax)
            nActualTimespan = nActualTimespanMax;

        // Retarget
        arith_uint256 bnNew;
        bnNew.SetCompact(pindexLast->nBits);
        bool fShift = bnNew.bits() > 235;
        if (fShift)
            bnNew >>= 1;
        bnNew *= nActualTimespan;
        bnNew /= nTargetTimespan;
        if (fShift)
            bnNew <<= 1;

        const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
        if (bnNew > bnPowLimit)
            bnNew = bnPowLimit;

        return bnNew.GetCompact();
    } // if testnet

    /*
     * 
     * 
     * 
     * Main net 
     * 
     * 
     * 
     */
    
    // 4th Hard fork, reset difficulty
    if (nHeight == params.nForkFour)
        return UintToArith256(params.powNeoScryptLimit).GetCompact();

    if (nHeight >= params.nForkOne)
        nTargetTimespan = (7 * 24 * 60 * 60) / 8; // 7/8 days

    if (nHeight >= params.nForkTwo)
        nTargetTimespan = (7 * 24 * 60 * 60) / 32; // 7/32 days

    if (nHeight >= params.nForkThree) {
        nTargetTimespan = 60; // 1 minute timespan
        nTargetSpacing = 60; // 1 minute block
    }

    bool fHardFork = nHeight == params.nForkOne || nHeight == params.nForkTwo;

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % nInterval != 0 && !fHardFork && nHeight < params.nForkThree)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % nInterval != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // The 1st retarget after genesis
    if (nInterval >= nHeight)
        nInterval = nHeight - 1;

    // Go back by nInterval
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < nInterval; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();

    if (nHeight >= params.nForkTwo && nHeight < params.nForkThree) {
        nInterval *= 4;
        const CBlockIndex* pindexFirst = pindexLast;
        for(int i = 0; pindexFirst && i < nInterval; i++)
            pindexFirst = pindexFirst->pprev;

        int nActualTimespanLong = (pindexLast->GetBlockTime() - pindexFirst->GetBlockTime())/4;

        // Average between short and long windows
        nActualTimespanAvg = (nActualTimespan + nActualTimespanLong) / 2;
    }

    if (nHeight >= params.nForkThree) {
        nInterval = 480;
return nInterval;
        const CBlockIndex* pindexFirstLong = pindexLast;
        for(int i = 0; pindexFirstLong && i < nInterval && i < nHeight - 1; i++) {
            pindexFirstLong = pindexFirstLong->pprev;
            if (i == 14)
                pindexFirstShortTime = pindexFirstLong->GetBlockTime();
            if (i == 119)
                pindexFirstMediumTime = pindexFirstLong->GetBlockTime();
        }
        nActualTimespanShort = (pindexLast->GetBlockTime() - pindexFirstShortTime) / 15;
        nActualTimespanMedium = (pindexLast->GetBlockTime() - pindexFirstMediumTime)/120;
        nActualTimespanLong = (pindexLast->GetBlockTime() - pindexFirstLong->GetBlockTime())/480;

        nActualTimespanAvg = (nActualTimespanShort + nActualTimespanMedium + nActualTimespanLong) / 3;
    }

    if (nHeight >= params.nForkTwo) {
        // Apply .25 damping
        nActualTimespan = nActualTimespanAvg + 3 * nTargetTimespan;
        nActualTimespan /= 4;
    }

    // The 1st hard fork (1.4142857 aka 41% difficulty limiter)
    if (nHeight >= params.nForkOne && nHeight < params.nForkTwo) {
        nActualTimespanMax = nTargetTimespan*99/70;
        nActualTimespanMin = nTargetTimespan*70/99;
    }

    // The 2nd hard fork (1.0905077 aka 9% difficulty limiter)
    if (nHeight >= params.nForkTwo) {
        nActualTimespanMax = nTargetTimespan*494/453;
        nActualTimespanMin = nTargetTimespan*453/494;
    }

    // Limit adjustment step
    if(nActualTimespan < nActualTimespanMin)
        nActualTimespan = nActualTimespanMin;
    if(nActualTimespan > nActualTimespanMax)
        nActualTimespan = nActualTimespanMax;

    // Retarget
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bool fShift = bnNew.bits() > 235;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;
    if (fShift)
        bnNew <<= 1;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
