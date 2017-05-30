// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_MINER_H
#define ANONCOIN_MINER_H

#include <stdint.h>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CReserveKey;
class CScript;
class CWallet;

struct CBlockTemplate;

extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;

struct HashMeterStats
{
    uint8_t nIDsReporting;
    uint16_t nFastCount;
    uint16_t nSlowCount;
    int64_t nEarliestStartTime;
    int64_t nMiningStoppedTime;
    int64_t nRunTime;
    int64_t nCumulativeTime;
    int64_t nCumulativeHashes;
    double dFastKHPS;
    double dSlowKHPS;
    double dCumulativeMHPH;
};

//! Extensive result summarization and state reporting
extern bool GetHashMeterStats( HashMeterStats& HashMeterState );
//! Helper and final result funtions
extern bool ClearHashMeterSlowMRU();
extern bool IsMinersRunning();
extern double GetFastMiningKHPS();
extern double GetSlowMiningKHPS();

/** Run the miner threads */
extern void GenerateAnoncoins(bool fGenerate, CWallet* pwallet, int nThreads);
/** Generate a new block, without valid proof-of-work */
extern CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn);
//!
extern CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey);
/** Modify the extranonce in a block */
extern void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);

/** Do mining precalculation, also used in rpcmining.cpp for the old getwork() */
extern void FormatHashBuffers(const CBlockHeader* pblock, char* pmidstate, char* pdata, char* phash1);

extern void UpdateTime(CBlockHeader* block, const CBlockIndex* pindexPrev);

#endif // ANONCOIN_MINER_H
