// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Many builder specific things set in the config file, ENABLE_WALLET is a good example.  Don't forget to include it this way in your source files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "miner.h"

#include "amount.h"
#include "block.h"
#include "chainparams.h"
#include "hash.h"
#include "main.h"
#include "net.h"
#include "pow.h"
#include "scrypt.h"
#include "sync.h"
#include "timedata.h"
#include "transaction.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/unordered_map.hpp>

//! Used to initialize the scrypt mining hash buffers
#include <openssl/sha.h>

using namespace std;

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

//////////////////////////////////////////////////////////////////////////////
//
// AnoncoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }

    void print() const
    {
        LogPrintf("COrphan(hash=%s, dPriority=%.1f, FeePerKb=%d)\n",
               ptx->GetHash().ToString(), dPriority, feeRate.GetFeePerK());
        BOOST_FOREACH(uint256 hash, setDependsOn)
            LogPrintf("   setDependsOn %s\n", hash.ToString());
    }
};

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlockHeader* pblock, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
}

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn)
{
    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience

    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    //if (Params().MineBlocksOnDemand())
    if( RegTest() )
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;

    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        CCoinsViewCache view(pcoinsTip);                    // Create an empty coin cache view, based on the main pcoinsTip cache

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi)
        {
            const CTransaction& tx = mi->second.GetTx();
            if (tx.IsCoinBase() || !IsFinalTx(tx, nHeight))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                assert(coins);

                CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = nHeight - coins->nHeight;

                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn-tx.GetValueOut(), nTxSize);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            CAmount nTxFees = view.GetValueIn(tx)-tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            if (!CheckInputs(tx, state, view, true, SCRIPT_VERIFY_P2SH, true))
                continue;

            UpdateCoins(tx, state, view, nHeight);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fPrintPriority)
            {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);

        // Compute final coinbase transaction.
        txNew.vout[0].nValue = GetBlockValue(nHeight, nFees);
        txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;
        pblock->vtx[0] = txNew;
        pblocktemplate->vTxFees[0] = -nFees;

        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockSha256dHash();             // Make sure it places the sha256d hash of the previous block into this new one.
        UpdateTime(pblock, pindexPrev);
        pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock);
        pblock->nNonce         = 0;
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        //! Force both hash calculations to be updated before validity testing the block
        assert( pblock->GetHash(true) != uint256(0) );
        assert( pblock->CalcSha256dHash(true) != uintFakeHash(0) );
        assert( pblock->GetGost3411Hash() != uint256(0) );
        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false))
            throw std::runtime_error("CreateNewBlock() : TestBlockValidity failed");
    }

    return pblocktemplate.release();
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uintFakeHash hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}


#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey);
}

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockSha256dHash())
            return error("AnoncoinMiner : generated block is stale");
    }

    // Remove key from key pool
    reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->CalcSha256dHash()] = 0;
    }

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, NULL, pblock))
        return error("AnoncoinMiner : ProcessNewBlock, block not accepted");

    return true;
}

class CHashMeter
{
private:
    const uint8_t nMinerID;     //! The ID is likely the same value as the thread number which started this hash meter
    const int64_t nSlowRunTime; //! This constant defines how long (in ms) before a logging event needs to be produced.
    int64_t nMinerStartTime;    //! in Seconds, the time for which this miner thread 1st started.
    int64_t nFastStartTime;     //! in milliSeconds, the time we started this last integration period
    int64_t nSlowStartTime;     //! in milliSeconds, the time we last logged our results
    uint64_t nFastCounter;      //! Hash count over one integration period (10sec)
    uint64_t nSlowCounter;      //! Cumulative hash count since this miner last reported a logging request
    uint64_t nCumulative;       //! Cumulative hash count since this miner was running
    double dFastKHPS;           //! The last computed Hash rate sample, in Kilo Hashes Per Second.
    double dSlowKHPS;           //! The computed Hash rate for one logging period.

public:
    //! Multithread miners will start out with a log result time one second longer than the previous thread.
    //! We want to avoid them hitting the log at exactly the same moment, and preferably in order, this is
    //! not likely, but an experiment.  Calculating the nSlowRunTime constant now and storing it saves doing
    //! that each time the UpdateAccumulations() is run, easy to change here and for speed.
    CHashMeter( const uint8_t nID ) : nMinerID(nID), nSlowRunTime( ( 10 * 60 + (int64_t)nID ) * 1000 )
    {
        int64_t nTimeNow = GetTimeMillis();

        nMinerStartTime = (nTimeNow + 500) / 1000;  //! Nearest second, with rounding
        nFastStartTime = nSlowStartTime = nTimeNow;
        nFastCounter = nSlowCounter = nCumulative = 0;
        dFastKHPS = dSlowKHPS = 0.0;
    }
    ~CHashMeter() {}

    //! Adds newly hashed counts to the primary accumulator only and starts returning true if it wants the short term result updated and logged.
    bool UpdateFastCounter( const uint16_t nHashesDone )
    {
        nFastCounter += (uint64_t)nHashesDone;
        return GetTimeMillis() - nFastStartTime > 10000;
    }

    //! Updates the longer term counters, short term measurement calculation & resets its counter and start time.
    //! Returns true if the longer mid term log entry should be updated.
    bool UpdateAccumulations()
    {
        int64_t nTimeNow = GetTimeMillis();

        nSlowCounter += nFastCounter;
        nCumulative += nFastCounter;
        dFastKHPS = (double)nFastCounter / (double)(nTimeNow - nFastStartTime);
        nFastCounter = 0;
        nFastStartTime = nTimeNow;

        //! After 10 minutes plus 1 sec x the nID, we start asking for a logging event
        return nTimeNow - nSlowStartTime > nSlowRunTime;
    }

    //! Updates the mid term (logged) measurement calculation, resets its counter and start time, then returns with the calculation in Hashes per second.
    double RestartSlowCounter()
    {
        int64_t nTimeNow = GetTimeMillis();
        int64_t nMilliSecs = nTimeNow - nSlowStartTime;

        dSlowKHPS = (double)nSlowCounter / (double)nMilliSecs;
        nSlowCounter = 0;
        nSlowStartTime = nTimeNow;

        return dSlowKHPS;
    }
    uint8_t GetMinerID() const { return nMinerID; }
    int64_t GetStartTime() const { return nMinerStartTime; }
    double GetFastKHPS() const { return dFastKHPS; }
    double GetSlowKHPS() const { return dSlowKHPS; }
    //! The total work performed so far on this miner thread can be gotten several ways, this method simply returns the total number of hashes the thread has calculated.
    uint64_t GetCumulativeHashes() const { return nCumulative; }
    //! This next one is a little more complicated, likely this is called by some routine that cares we return the correct value for time.
    //! As it marches on, this sample could have been made in the distant past, aka placed in one of the mru storage sets long ago.  So we
    //! make sure the accumulated hashes do not get added to, unless the HPSCounter was just zeroed, and most importantly to this next
    //! method,  the nFastStartTime was set to the ending time for the accumulation!
    int64_t GetCumulativeSecs() const { return ( (nFastStartTime + 500) / 1000 ) - nMinerStartTime; }
    //! Returns the Hashes per Second for the total time this hash meter has been running (within 10sec)
    double GetCumulativeHPS() const { return (double)nCumulative / (double)GetCumulativeSecs(); }

    //! Needed for inclusion in an mru set...
    friend bool operator<(const CHashMeter& a, const CHashMeter& b)
    {
        return a.nMinerID < b.nMinerID || (a.nMinerID == b.nMinerID && a.nFastStartTime < b.nFastStartTime) ||
               (a.nMinerID == b.nMinerID && a.nFastStartTime == b.nFastStartTime && a.nSlowStartTime < b.nSlowStartTime);
    }
};

static uint8_t nLastRunThreadCount = 0;
static int64_t nMiningStoppedTime = 0;

//! Global values protected by the cs_hashmeter condition variable...
static CCriticalSection cs_hashmeter;
static uint8_t nMinerThreadsRunning = 0;
//! Initialization here only requires a few bytes, actual storage maximum size will be set when the miner(s) start running.
//! We only keep ~ the last 10mins worth of 10sec short term data and ~1 days worth of 10min, these sizes will be defined
//! depending on how many threads the user wants started.
static mruset<CHashMeter> mruFastReadings(60);
static mruset<CHashMeter> mruSlowReadings(144);

typedef pair<uint16_t,double> MeterSum;
typedef boost::unordered_map<uint8_t,MeterSum> MeterMap;

static void UpdateMeterMap( uint8_t nMinerID, double dMinerKHPS, MeterMap& mapMeterSums )
{
    MeterMap::iterator it = mapMeterSums.find(nMinerID);
    if( it == mapMeterSums.end() ) {
        MeterSum newMiner;
        newMiner.first = 1;
        newMiner.second = dMinerKHPS;
        mapMeterSums[nMinerID] = newMiner;
    } else {
        (*it).second.first++;
        (*it).second.second += dMinerKHPS;
    }
}

bool GetHashMeterStats( HashMeterStats& HashMeterState )
{
    bool fResultsProduced = false;
    uint8_t nAttempts = 0; //! If lock fails, its no big deal, we'll try again for at least a couple of seconds...
    int64_t nTimeNow = GetTime();

    HashMeterState.nIDsReporting = 0;
    HashMeterState.nFastCount = HashMeterState.nSlowCount = 0;
    HashMeterState.nEarliestStartTime = nTimeNow;
    HashMeterState.nMiningStoppedTime = nMiningStoppedTime;
    HashMeterState.nRunTime = HashMeterState.nCumulativeTime = HashMeterState.nCumulativeHashes = 0;
    HashMeterState.dFastKHPS = HashMeterState.dSlowKHPS = HashMeterState.dCumulativeMHPH = 0.0;
    do {
        TRY_LOCK(cs_hashmeter, lockedmeter);
        if( lockedmeter ) {
            //! If no miners are running, we still may have results from the past stored in the longer term dataset
            //! Determine if we have some meter readings to analyze...
            HashMeterState.nFastCount = mruFastReadings.size();
            HashMeterState.nSlowCount = mruSlowReadings.size();
            //! If there are no meter readings, we are also done...
            if( HashMeterState.nFastCount || HashMeterState.nSlowCount ) {
                //! For each of the slow and fast mruReadings, we must now total the counts and hashes found from each and every miner thread separately.
                //! It requires us to keep track of that, as we go through all the mru datapoints, and gets kinda complicated to explain.  See the
                //! UpdateMeterMap() helper function that figures out if the miner ID is already in the MeterMap, or needs to be added, takes care of both.
                if( HashMeterState.nFastCount ) {
                    MeterMap mapMiner;
                    for( mruset<CHashMeter>::const_iterator it = mruFastReadings.begin(); it != mruFastReadings.end(); ++it )
                        UpdateMeterMap( (*it).GetMinerID(), (*it).GetFastKHPS(), mapMiner );
                    //! Finally we can go through each of the miner IDs that reported results and
                    //! add the averaged KHPS from each to the hash meter states final result.
                    for( MeterMap::const_iterator it = mapMiner.begin(); it != mapMiner.end(); ++it )
                        HashMeterState.dFastKHPS += (*it).second.second / (double)(*it).second.first;
                }
                //! Finally we handle the processing of the longer term 10 minute samples and calculate the final
                //! KHPS from the average we've seen in the same fashion we did for the fast mru.
                if( HashMeterState.nSlowCount ) {
                    MeterMap mapMiner;
                    for( mruset<CHashMeter>::const_iterator it = mruSlowReadings.begin(); it != mruSlowReadings.end(); ++it )
                        UpdateMeterMap( (*it).GetMinerID(), (*it).GetSlowKHPS(), mapMiner );
                    //! Again we can go through each of the miner IDs that reported results and
                    //! add the averaged KHPS to the hash meter states final result.
                    for( MeterMap::const_iterator it = mapMiner.begin(); it != mapMiner.end(); ++it )
                        HashMeterState.dSlowKHPS += (*it).second.second / (double)(*it).second.first;
                }

                //! While we know there is at least some meter readings, we can find and total the most recent accumulative
                //! hash count and total execution times, from each of the miner threads.  This time we'll do that with a
                //! simple MinerID set, we start out with no miner IDs and an iterator that points to the end of the selected
                //! mru set, that depends on if this is history or while operating live running mining threads.  In either
                //! case all we need is the most recent sample from each of the MinerID's, that also depends on if we are
                //! running or stopped at the moment.
                set<uint8_t> setMinerID;
                uint8_t nMaxId = nMinerThreadsRunning ? nMinerThreadsRunning : nLastRunThreadCount;
                mruset<CHashMeter>::const_iterator it = HashMeterState.nFastCount ? mruFastReadings.end() : mruSlowReadings.end();
                mruset<CHashMeter>::const_iterator itend = HashMeterState.nFastCount ? mruFastReadings.begin() : mruSlowReadings.begin();
                do {
                    //! ToDo: Upgrade this to use the newly invented (typedefs added to mruset.h) reverse_iterator method.
                    it--; //! 1st decrement it, as we move backwards in time from the most recent sample produced, starts out = end() of the selected mru set.
                    //! Insertion fails, and returns false if the miner ID is already in the set so we skip it, because that means we already have
                    //! that miners ID information. When we get all of them we're done, and have also totaled how many reported, which is a very
                    //! good way of knowing if they are all still running or one or more have terminated.
                    if( setMinerID.insert( (*it).GetMinerID() ).second ) {
                        //! So it must have been inserted, include this threads results in the accumulation...
                        HashMeterState.nCumulativeTime += (*it).GetCumulativeSecs();
                        HashMeterState.nCumulativeHashes += (*it).GetCumulativeHashes();
                        //! ...and keep track of the earliest start time we find, that will be when mining was initiated.
                        int64_t nMetersStartTime = (*it).GetStartTime();
                        if( HashMeterState.nEarliestStartTime > nMetersStartTime ) HashMeterState.nEarliestStartTime = nMetersStartTime;
                        //! Finally bump the number of reporting threads we have found.
                        HashMeterState.nIDsReporting++;
                    }
                //! Do this until we have all the mining thread reports, or we've run out of samples to explore.
                } while( HashMeterState.nIDsReporting < nMaxId && it != itend );

                //! We now have only one datapoint from each of the miners, and it is the most recent Cumulative run time from each of them.
                //! so we calculate the HPS and convert it into mega hashes per hour at the same time.  nCumulativeTime here is not used though
                //! or we could multiply the result by the number of nIDsReporting.  Instead we use actual time span, this effectively multiplies
                //! the result by the same factor and sets up the final result needed by the 'gethashmeter' query.  It also works if mining has
                //! stopped or is currently running.
                if( HashMeterState.nIDsReporting ) {
                    HashMeterState.nRunTime = HashMeterState.nFastCount ? nTimeNow : nMiningStoppedTime;
                    HashMeterState.nRunTime -= HashMeterState.nEarliestStartTime;
                }
                //! Should never happen, but stops divide by zero, good for debugging and not reporting garbage to the user...
                if( HashMeterState.nRunTime > 0 )
                    HashMeterState.dCumulativeMHPH = (double)(HashMeterState.nCumulativeHashes * 3600) / (double)(HashMeterState.nRunTime * 1000000);
                else
                    HashMeterState.nRunTime = -1;
            }
            fResultsProduced = true;
        } else
            MilliSleep(100);            //! Sleep for abit..
    } while( !fResultsProduced && ++nAttempts < 20 );

    return fResultsProduced;
}

bool ClearHashMeterSlowMRU()
{
    bool fCleared = false;
    uint8_t nAttempts = 0; //! If lock fails, its no big deal, we'll try again for at least a couple of seconds...

    do {
        TRY_LOCK(cs_hashmeter, lockedmeter);
        if( lockedmeter ) {
            mruSlowReadings.clear();
            fCleared = true;
        } else
            MilliSleep(100);            //! Sleep for abit..
    } while( !fCleared && ++nAttempts < 20 );

    return fCleared;
}

bool IsMinersRunning()
{
    LOCK(cs_hashmeter);
    return nMinerThreadsRunning != 0;
}

//! This procedure averages the miner threads 10 second samples, as found from over the last 10 minutes.
//! Once the totals from each thread have been created, it finds the average each of them are producing
//! and sums each of those, into the total returned.
double GetFastMiningKHPS()
{
    double dFastKHPS = 0.0;

    LOCK(cs_hashmeter);

    if( nMinerThreadsRunning ) {
        MeterMap mapMiner;
        for( mruset<CHashMeter>::const_iterator it = mruFastReadings.begin(); it != mruFastReadings.end(); ++it )
            UpdateMeterMap( (*it).GetMinerID(), (*it).GetFastKHPS(), mapMiner );
        for( MeterMap::const_iterator it = mapMiner.begin(); it != mapMiner.end(); ++it )
            dFastKHPS += (*it).second.second / (double)(*it).second.first;
    }

    return dFastKHPS;
}

//! Slow mining results can be returned even if generation has been turned off, if there are still results stored, this routine will
//! find, and return the most recent 10 minute sample from each of the threads that were(are) running, and return that value.
//! Accumulated averaging is not done, although it could be in the future.  As of this commentary it is not being used for anything.
double GetSlowMiningKHPS()
{
    double dResult = 0.0;

    LOCK(cs_hashmeter);

    uint8_t nIDsFound = 0;
    set<uint8_t> setMinerID;
    for( mruset<CHashMeter>::const_reverse_iterator it = mruSlowReadings.rbegin(); nIDsFound < nMinerThreadsRunning && it != mruSlowReadings.rend(); ++it ) {
        if( setMinerID.insert( (*it).GetMinerID() ).second ) {
            dResult += (*it).GetFastKHPS();
            nIDsFound++;
        }
    }
    return dResult;
}

void static AnoncoinMiner(CWallet *pwallet)
{
    LogPrintf("%s : v3.0 for GOST3411 started with (DDA) Dynamic Difficulty Awareness and (MTHM) Multi-Threaded HashMeter technologies.\n", __func__ );
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("anoncoin-miner");

    //! Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    uint32_t nExtraNonce = 0;

    //! Create a new Hash meter for this thread, destruction handled automatically
    uint8_t nMyID;
    {
        LOCK(cs_hashmeter);
        nMyID = ++nMinerThreadsRunning;
    }
    //! Allocating with scoped pointers means no more concern over its destruction when we're finished...

    //! Each thread gets its own Hash Meter, with a unique ID
    boost::scoped_ptr<CHashMeter> spMyMeter(new CHashMeter( nMyID ));
    //! Each thread gets its own Scrypt mining ScratchPad buffer, they are large.
    boost::scoped_ptr<char> spScratchPad( new char[ SCRYPT_SCRATCHPAD_SIZE ] );
    // Each thread gets its own scratchpad buffer, allocated in normal data storage and off the stack...
    // char* pScratchPadBuffer = (char*) ::operator new (SCRYPT_SCRATCHPAD_SIZE, nothrow);
    // if( !pScratchPadBuffer ) {
        // LogPrintf( "%s %2d: ERROR - Unable to allocate scratchpad buffer for scrypt miner...\n", __func__, nMyID );
        // return;
    //}

    try {
        while (true) {
            if (!RegTest()) {
                //! Busy-wait for the network to come online so we don't waste time mining
                //! on an obsolete chain. In regtest mode we expect to fly solo.
                while (vNodes.empty())
                    MilliSleep(10000);
            }

            /**
             * Create new block
             */
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey));
            if (!pblocktemplate.get())
            {
                LogPrintf("%s %2d: ERROR - Keypool ran out, please refill before restarting.\n", __func__, nMyID );
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("%s %2d: Running with %u transactions in block (%u bytes)\n", __func__, nMyID, pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            /**
             * Search
             */
            int64_t nStart = GetTime();
            uint256 hashTarget;
            hashTarget.SetCompact(pblock->nBits);
            while( true ) {
                bool fFound = false;
                bool fAccepted = false;
                uint16_t nHashesDone = 0;
                uint256 thash;
                //! Scan nonces looking for a solution
                while(true) {
                    // scrypt_1024_1_1_256_sp(BEGIN(pblock->nVersion), BEGIN(thash), pScratchPadBuffer);
                    //char *pPad = spScratchPad.get();
                    if (chainActive.UseGost3411Hash())
                    {
                        thash = SerializeGost3411Hash(*pblock, SER_NETWORK, PROTOCOL_VERSION);
                    } else {
                        scrypt_1024_1_1_256_sp(BEGIN(pblock->nVersion), BEGIN(thash), spScratchPad.get());
                    }
                    nHashesDone++;
                    if( thash <= hashTarget ) {
                        fFound = true;
                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
                        //! Found a solution
                        //! Force new proof-of-work block scrypt hash and the sha256d hash values to be calculated.
                        //! Calling GetHash() & CalcSha256dHash() with true invalidates any previous (and obsolete) ones.
                        assert( thash == pblock->GetHash(true) );
                        //! Basically this next line does the Scrypt calculation again once, then all the normal
                        //! validation code kicks in from the call to ProcessBlockFound(), insuring that is the case...
                        assert( pblock->CalcSha256dHash(true) != uintFakeHash(0) );
                        LogPrintf("%s %2d:\n", __func__, nMyID );
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", thash.GetHex(), hashTarget.GetHex());
                        fAccepted = ProcessBlockFound(pblock, *pwallet, reservekey);
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);

                        //! In regression test mode, stop mining after a block is found.
                        if( RegTest() )
                            throw boost::thread_interrupted();
                        break;
                    }
                    pblock->nNonce++;
                    //! In this inner loop, we calculate 256 hashes, if none are found, we'll try updating some other factors
                    if( (pblock->nNonce & 0xFF) == 0 )
                        break;
                }

                //!
                //if( fFound ) {
                    //if( !fAccepted )
                    //break;
                //}

                //! Meter hashes/sec production, UpdateFastCounter starts returning true if its time to log results (10 sec.)
                if( spMyMeter->UpdateFastCounter(nHashesDone) ) {
                    //! If lock fails, its no big deal, we'll try again next time, the meter update is fast & keeps accumulating...
                    TRY_LOCK(cs_hashmeter, lockedmeter);
                    if( lockedmeter ) {
                        //! Once we have the meter locked we first update our meter results, 'then' the 10 sec summary mru.
                        bool fUpdateLog = spMyMeter->UpdateAccumulations();
                        //! Now we store the whole hash meter as another (most recent) data point in the short term sample buffer,
                        //! its new start time, becomes our final timestamp and can be used outside this thread anytime in the future.
                        mruFastReadings.insert( *spMyMeter );
                        //! If it turns out that the 10 minute log update needs to be done, we update that and report it and other stuff as well, otherwise we're done.
                        if( fUpdateLog ) {
                            //! Again before saving the sample, we make sure the timestamp is updated and the meters log fields reset for another interval,
                            //! this call returns the logged HashesPermilliSec, so the value is correct for KiloHashes per second already...cool
                            double dKiloHashesPerSec = spMyMeter->RestartSlowCounter();
                            //! Store the whole hash meter as another data point in the long term sample buffer, and report that to the log.
                            mruSlowReadings.insert( *spMyMeter );
                            LogPrintf("%s %2d: reporting new 10 min sample update of %6.3f KHashes/Sec.\n", __func__, nMyID, dKiloHashesPerSec );
#if defined( DONT_COMPILE )
                            //! If our ID matches the number of threads running, we are the last one, and report some more...
                            //! ToDo: Note this only works if the last thread always keep running, for an unknown reason it or other threads may have shut down.
                            //! The HashMeter stats reporting function looks at the data in the 10sec mru, and knows if all threads are reporting results, can
                            //! even tell which ones are not if we need that information.  Here though, it could fail if the last thread shutdown unexpectedly.
                            if( nMyID == nMinerThreadsRunning ) {
                                int nReadings = mruFastReadings.size();
                                double dCumulativeKHPS = 0.0;
                                for( mruset<CHashMeter>::const_iterator it = mruFastReadings.begin(); it != mruFastReadings.end(); ++it )
                                    dCumulativeKHPS += (*it).GetFastKHPS();
                                dCumulativeKHPS /= (double)nReadings;
                                LogPrintf("%s : Hash Power of %6.3f Khash/sec seen across the last %d (10sec) meter readings from %d thread(s).\n", __func__, dCumulativeKHPS, nReadings, nMinerThreadsRunning );
                                nReadings = mruSlowReadings.size();
                                dCumulativeKHPS = 0.0;
                                for( mruset<CHashMeter>::const_iterator it = mruSlowReadings.begin(); it != mruSlowReadings.end(); ++it )
                                    dCumulativeKHPS += (*it).GetSlowKHPS();
                                dCumulativeKHPS /= (double)nReadings;
                                LogPrintf("%s : Hash Power of %6.3f Khash/sec seen across the last %d (10min) meter readings from %d thread(s).\n", __func__, dCumulativeKHPS, nReadings, nMinerThreadsRunning );
                            }
#endif
                        }
                    }
                }
                //! Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                //! If there are no peers we terminate the miner... except for RegTests
                if( vNodes.empty() && !RegTest() )
                    break;
                //! Having processed more than 65K hashes means updating would be a good idea...
                if( pblock->nNonce >= 0xffff0000 )
                    break;
                //! Checks for transaction changes happen at least 5 times over the coarse of one nTargetSpacing interval
                if( mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 36 )
                    break;
                //! If the previous block has changed, we must as well...
                if (pindexPrev != chainActive.Tip())
                    break;

                //! Update nTime and difficulty required (if needed) every so often
                //! Changing pblock->nTime effects the work required for Anoncoin if the blockheader time is used for PID calculations,
                //! otherwise it does not.  No older generation of the NextWorkRequired will be effected by time change.
                UpdateTime(pblock, pindexPrev);
                pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
                hashTarget.SetCompact(pblock->nBits);
            } // Looping forever while true and no nonce overflow, transactions updated or new blocks arrived
        } // Looping forever while true in this thread
    } // try, errors caught and logged before terminating
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("%s %2d: terminated.\n", __func__, nMyID );
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("%s %2d: runtime error: %s\n", __func__, nMyID, e.what());
        return;
    }
    // if( pScratchPadBuffer ) delete pScratchPadBuffer;
}

void GenerateAnoncoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0) {
        if( RegTest() )
            nThreads = 1;
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    {
        LOCK(cs_hashmeter);
        nMinerThreadsRunning = 0;
        mruFastReadings.clear();
    }

    if (nThreads == 0 || !fGenerate) {
        nMiningStoppedTime = GetTime();
        return;
    }

    {
        LOCK(cs_hashmeter);
        mruFastReadings.max_size( nThreads * 60 );
        mruSlowReadings.max_size( nThreads * 144 );
    }

    nLastRunThreadCount = nThreads;
    nMiningStoppedTime = 0;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&AnoncoinMiner, pwallet));
}

#endif // ENABLE_WALLET

/**
 * Scrypt mining related code for block creation
 */
static const uint32_t pSHA256InitState[8] =
{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

int static FormatHashBlocks(void* pbuffer, unsigned int len)
{
    unsigned char* pdata = (unsigned char*)pbuffer;
    unsigned int blocks = 1 + ((len + 8) / 64);
    unsigned char* pend = pdata + 64 * blocks;
    memset(pdata + len, 0, 64 * blocks - len);
    pdata[len] = 0x80;
    unsigned int bits = len * 8;
    pend[-1] = (bits >> 0) & 0xff;
    pend[-2] = (bits >> 8) & 0xff;
    pend[-3] = (bits >> 16) & 0xff;
    pend[-4] = (bits >> 24) & 0xff;
    return blocks;
}

/** Base sha256 mining transform */
void static SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    SHA256_CTX ctx;
    unsigned char data[64];

    SHA256_Init(&ctx);

    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = ByteReverse(((uint32_t*)pinput)[i]);

    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];

    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}

/** Do mining precalculation, also used in rpcmining.cpp for the old getwork() */
void FormatHashBuffers(const CBlockHeader* pblock, char* pmidstate, char* pdata, char* phash1)
{
    //! Structure used for pre-building the hash buffers
    struct
    {
        struct unnamed2
        {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
        }
        block;
        unsigned char pchPadding0[64];
        uint256 hash1;
        unsigned char pchPadding1[64];
    }
    tmp;
    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion       = pblock->nVersion;
    tmp.block.hashPrevBlock  = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime          = pblock->nTime;
    tmp.block.nBits          = pblock->nBits;
    tmp.block.nNonce         = pblock->nNonce;

    FormatHashBlocks(&tmp.block, sizeof(tmp.block));
    FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

    //! Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    //! Precalc the first half of the first hash, which stays constant
    SHA256Transform(pmidstate, &tmp.block, pSHA256InitState);

    memcpy(pdata, &tmp.block, 128);
    memcpy(phash1, &tmp.hash1, 64);
}
