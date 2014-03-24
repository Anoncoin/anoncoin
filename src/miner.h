#ifndef ANONCOIN_MINER_H
#define ANONCOIN_MINER_H

#include <stdint.h>

typedef long long  int64;
typedef unsigned long long  uint64;


class CBlock;
class CBlockHeader;
class CBlockIndex;
struct CBlockTemplate;
class CReserveKey;
class CScript;

#ifdef ENABLE_WALLET
class CWallet;
void AnoncoinMiner(CWallet *pwallet);
#endif

/** Run the miner threads */
void GenerateAnoncoins(bool fGenerate, CWallet* pwallet);
/** Generate a new block, without valid proof-of-work */
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn);
CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey);
/** Modify the extranonce in a block */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
/** Do mining precalculation */
void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1);
/** Check mined block */
bool CheckWork(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey);
/** Base sha256 mining transform */
void SHA256Transform(void* pstate, void* pinput, const void* pinit);


#endif
