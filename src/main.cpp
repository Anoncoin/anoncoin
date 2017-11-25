// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"

#include "addrman.h"
#include "alert.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "checkqueue.h"
#include "init.h"
#include "merkleblock.h"
#include "net.h"
#include "pow.h"
#include "random.h"
#include "sigcache.h"
#include "timedata.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"

#include <sstream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread.hpp>

using namespace std;

#if defined(NDEBUG)
# error "Anoncoin cannot be compiled without assertions."
#endif

//! Constants found in this source codes header(.h)

/** "reject" message codes */
const uint8_t REJECT_MALFORMED = 0x01;
const uint8_t REJECT_INVALID = 0x10;
const uint8_t REJECT_OBSOLETE = 0x11;
const uint8_t REJECT_DUPLICATE = 0x12;
const uint8_t REJECT_NONSTANDARD = 0x40;
const uint8_t REJECT_DUST = 0x41;
const uint8_t REJECT_INSUFFICIENTFEE = 0x42;
const uint8_t REJECT_CHECKPOINT = 0x43;

//! disconnect from peers older than this proto version, anything older than a build
//! from client version v0.8.5.5 has not been seen or supported on the network
static const int32_t MIN_PEER_PROTO_VERSION = 70006;
static const int32_t MIN_PEER_PROTO_VERSION_AFTER_HF = 70010; //! After the Hardfork Block is reached, this version will be obligatory
static const int32_t MIN_PEER_PROTO_VERSION_AFTER_HF2 = 70012; //! After the second Hardfork Block changing PID parameters is reached, this version will be obligatory

/** Default for -blockmaxsize and -blockminsize, which control the range of sizes the mining code will create **/
const uint32_t DEFAULT_BLOCK_MAX_SIZE = 750000;
const uint32_t DEFAULT_BLOCK_MIN_SIZE = 0;
/** Default for -blockprioritysize, maximum space for zero/low-fee transactions **/
const uint32_t DEFAULT_BLOCK_PRIORITY_SIZE = 27000;              //! This matches Anoncoin v0.8.5.6 clients value
/** The maximum size for transactions we're willing to relay/mine */
const uint32_t MAX_STANDARD_TX_SIZE = 100000;
/** The maximum allowed number of signature check operations in a block (network rule) */
const uint32_t MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
/** Maximum number of signature check operations in an IsStandard() P2SH script */
const uint32_t MAX_P2SH_SIGOPS = 15;
/** The maximum number of sigops we're willing to relay/mine in a single tx */
const uint32_t MAX_TX_SIGOPS = MAX_BLOCK_SIGOPS/5;
/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
const uint32_t DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** The maximum size of a blk?????.dat file (since 0.8) */
const uint32_t MAX_BLOCKFILE_SIZE = 0x8000000; // 128 MiB
/** The pre-allocation chunk size for blk?????.dat files (since 0.8) */
const uint32_t BLOCKFILE_CHUNK_SIZE = 0x1000000; // 16 MiB
/** The pre-allocation chunk size for rev?????.dat files (since 0.8) */
const uint32_t UNDOFILE_CHUNK_SIZE = 0x100000; // 1 MiB
/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
const int32_t COINBASE_MATURITY = 100;
/** Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX timestamp. */
const uint32_t LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC
/** Maximum number of script-checking threads allowed */
const int32_t MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
const int32_t DEFAULT_SCRIPTCHECK_THREADS = 0;
/** Number of blocks that can be requested at any given time from a single peer. */
const int32_t MAX_BLOCKS_IN_TRANSIT_PER_PEER = 32;
/** Timeout in seconds during which a peer must stall block download progress before being disconnected. */
const uint32_t BLOCK_STALLING_TIMEOUT = 15;      //! We wait at least 15sec over i2p before stalling out a peer
/** Number of headers sent in one getheaders result. We rely on the assumption that if a peer sends
 *  less than this number, we reached their tip. Changing this value is a protocol upgrade. */
const uint32_t MAX_HEADERS_RESULTS = 2000;
/** Size of the "block download window": how far ahead of our current height do we fetch?
 *  Larger windows tolerate larger download speed differences between peer, but increase the potential
 *  degree of disordering of blocks on disk (which make reindexing and in the future perhaps pruning
 *  harder). We'll probably want to make this a per-peer adaptive value at some point. */
const uint32_t BLOCK_DOWNLOAD_WINDOW = 4096;
/** Time to wait (in seconds) between writing blockchain state to disk. */
const uint32_t DATABASE_WRITE_INTERVAL = 3600;
/** Maximum length of reject messages. */
const uint32_t MAX_REJECT_MESSAGE_LENGTH = 111;

// This value came from Anoncoin v0.8.5.6, and allow us to increase Tx fee prices (ToDo: check if that code got removed)
// as the block grows in size, the param is still a number of places in main.cpp, but not found in v10
/** The maximum size for mined blocks */
const uint32_t MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/4;         // 250KB  block soft limit

// Old params no longer used or changed ToDo: Review
// For I2p, this could be considerably longer, up'n the value by double
// static const unsigned int BLOCK_DOWNLOAD_TIMEOUT = 60;
/** Timeout in seconds before considering a block download peer unresponsive. */
const uint32_t BLOCK_DOWNLOAD_TIMEOUT = 120;

/** Dust Soft Limit, allowed with additional fee per output */
// This value matches the v0.8.5.6 client, however does not exist in v10
const int64_t DUST_SOFT_LIMIT = 1000000; // 0.01 ANC

/** Minimum disk space required - used in CheckDiskSpace() */
const uint64_t nMinDiskSpace = 52428800;


/**
 * Global state
 */

CCriticalSection cs_main;

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex *pindexBestHeader = NULL;
int64_t nTimeBestReceived = 0;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;

bool fImporting = false;
bool fReindex = false;
bool fTxIndex = false;
bool fIsBareMultisigStd = true;
bool fCheckBlockIndex = false;
uint32_t nCoinCacheSize = 5000;

//! If we've just initialized Testnet with a genesis block we need to create some initial blocks, this flag starts that process
bool fGenerateInitialTestNetState = false;

/** Fees smaller than this (in satoshi) are considered zero fee (for relaying and mining) */
CFeeRate minRelayTxFee = CFeeRate(1000);
CTxMemPool mempool(::minRelayTxFee);

struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
};
map<uint256, COrphanTx> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;

void EraseOrphansFor(NodeId peer);
static void CheckBlockIndex();

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

const string strMessageMagic = "Anoncoin Signed Message:\n";

// Internal stuff
namespace {

    struct CBlockIndexWorkComparator
    {
        bool operator()(CBlockIndex *pa, CBlockIndex *pb) const {
            // First sort by most total work, ...
            if (pa->nChainWork > pb->nChainWork) return false;
            if (pa->nChainWork < pb->nChainWork) return true;

            // ... then by earliest time received, ...
            if (pa->nSequenceId < pb->nSequenceId) return false;
            if (pa->nSequenceId > pb->nSequenceId) return true;

            // Use pointer address as tie breaker (should only happen with blocks
            // loaded from disk, as those all have id 0).
            if (pa < pb) return false;
            if (pa > pb) return true;

            // Identical blocks.
            return false;
        }
    };

    CBlockIndex *pindexBestInvalid;

    /**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though.
     */
    set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;
    /** Number of nodes with fSyncStarted. */
    int nSyncStarted = 0;
    /** All pairs A->B, where A (or one if its ancestors) misses transactions, but B has transactions. */
    multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

    CCriticalSection cs_LastBlockFile;
    std::vector<CBlockFileInfo> vinfoBlockFile;
    int nLastBlockFile = 0;

    /**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
    CCriticalSection cs_nBlockSequenceId;
    /** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
    uint32_t nBlockSequenceId = 1;

    /**
     * Special Note from Anoncoin Developer: GroundRod
     *      Its going to be difficult for anyone looking at this code to understand (including myself
     *      months or years from now) what part uses/works with/is setup as sha256d hash values
     *      for blocks, and which parts are working internally with the real block mined hashes.
     *      So with that in mind, I've decided to try and structure things around this idea.  If its
     *      not directly headed out to node(s), or received from them, we stick to using the real
     *      Scrypt hash, as that is what is being referenced everywhere else as much as possible
     *      throughout the rest of this code, so it can work as was intended.  RPC users inbound we
     *      try to decode both ways, so they can use either/or.  Outbound is more complicated, and
     *      requires tuning based on the command/query itself, and another topic.
     *      Back to source code data structures, like mapBlockSource next up here in the main.cpp
     *      That hash value in each map object will only be converted to a sha256d hash, if we need
     *      to send out a reject message to a node, other than that one place, you can figure it is
     *      a map of real work uint256 hash values.
     *      On the other hand, QueuedBlock & CBlockReject structures are all only useful as heading
     *      to the outside world.  And will not be setup with REAL block hashes, instead what is used
     *      is often referenced as a FakeHash, the double sha256 hash of the block, which have long
     *      since been used throughout Anoncoin's history, totally useless for chain proof-of-work
     *      calculations.
     *      The past is still used in the exchange with peers, and embedded in transactions.  As much
     *      as possible where referenced that will be classed as uintFakeHash, instead of uint256 values.
     *      to help indicate this distinction .  As we can then easily use the cross reference map to
     *      find the real hash quick if we need to for some reason.  Note CBlockLocator objects
     *      are also made up of sha256d hashes, and can be sent directly to peers, or referenced as real
     *      hashes when we receive a locator from another peer inventory, thanks again to the new map.
     *      A new GetRealHashLocator() has been setup, although only used to pass the skiplist tests atm...
     *
     * Sources of received blocks, to be able to send them reject messages or ban
     * them, if processing happens afterwards. Protected by cs_main.
     */
    map<uint256, NodeId> mapBlockSource;

    /** Blocks that are in flight, and that are in the queue to be downloaded. Protected by cs_main. */
    struct QueuedBlock {
        uintFakeHash hash;
        CBlockIndex *pindex;  //! Optional.
        int64_t nTime;  //! Time of "getdata" request in microseconds.
        int nValidatedQueuedBefore;  //! Number of blocks queued with validated headers (globally) at the time this one is requested.
        bool fValidatedHeaders;  //! Whether this block has validated headers at the time of request.
    };
    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> > mapBlocksInFlight;

    /** Number of blocks in flight with validated headers. */
    int nQueuedValidatedHeaders = 0;

    /** Number of preferable block download peers. */
    int nPreferredDownload = 0;

    /** Dirty block index entries. */
    set<CBlockIndex*> setDirtyBlockIndex;

    /** Dirty block file entries. */
    set<int> setDirtyFileInfo;
} // anon namespace

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions, and allocation for the primary signals structure group
//
static CMainSignals g_signals;
CMainSignals& GetMainSignals() { return g_signals; }

// These functions dispatch to one or all registered wallets

void RegisterValidationInterface(CValidationInterface* pInterfaceIn) {
    g_signals.SyncTransaction.connect(boost::bind(&CValidationInterface::SyncTransaction, pInterfaceIn, _1, _2));
    g_signals.UpdatedTransaction.connect(boost::bind(&CValidationInterface::UpdatedTransaction, pInterfaceIn, _1));
    g_signals.SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pInterfaceIn, _1));
    g_signals.Inventory.connect(boost::bind(&CValidationInterface::Inventory, pInterfaceIn, _1));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pInterfaceIn));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pInterfaceIn, _1, _2));
    g_signals.GetScriptForMining.connect(boost::bind(&CValidationInterface::GetScriptForMining, pInterfaceIn, _1));
    g_signals.BlockFound.connect(boost::bind(&CValidationInterface::ResetRequestCount, pInterfaceIn, _1));
}

void UnregisterValidationInterface(CValidationInterface* pInterfaceIn) {
    g_signals.BlockFound.disconnect(boost::bind(&CValidationInterface::ResetRequestCount, pInterfaceIn, _1));
    g_signals.GetScriptForMining.disconnect(boost::bind(&CValidationInterface::GetScriptForMining, pInterfaceIn, _1));
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pInterfaceIn, _1, _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pInterfaceIn));
    g_signals.Inventory.disconnect(boost::bind(&CValidationInterface::Inventory, pInterfaceIn, _1));
    g_signals.SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pInterfaceIn, _1));
    g_signals.UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pInterfaceIn, _1));
    g_signals.SyncTransaction.disconnect(boost::bind(&CValidationInterface::SyncTransaction, pInterfaceIn, _1, _2));
}

void UnregisterAllValidationInterfaces() {
    g_signals.BlockFound.disconnect_all_slots();
    g_signals.GetScriptForMining.disconnect_all_slots();
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
    g_signals.SetBestChain.disconnect_all_slots();
    g_signals.UpdatedTransaction.disconnect_all_slots();
    g_signals.SyncTransaction.disconnect_all_slots();
}

void SyncWithWallets(const CTransaction &tx, const CBlock *pblock) {
    g_signals.SyncTransaction(tx, pblock);
}

//////////////////////////////////////////////////////////////////////////////
//
// Registration of network node signals.
//

namespace {

struct CBlockReject {
    uint8_t chRejectCode;
    string strRejectReason;
    uintFakeHash hashBlock;
};

/**
 * Maintain validation-specific state about nodes, protected by cs_main, instead
 * by CNode's own locks. This simplifies asynchronous operation, where
 * processing of incoming data is done after the ProcessMessage call returns,
 * and we're no longer holding the node's locks.
 */
struct CNodeState {
    //! The peer's address
    CService address;
    //! Whether we have a fully established connection.
    bool fCurrentlyConnected;
    //! Accumulated misbehaviour score for this peer.
    int nMisbehavior;
    //! Whether this peer should be disconnected and banned (unless whitelisted).
    bool fShouldBan;
    //! String name of this peer (debugging/logging purposes).
    std::string name;
    //! List of asynchronously-determined block rejections to notify this peer about.
    std::vector<CBlockReject> rejects;
    //! The best known block we know this peer has announced.
    CBlockIndex *pindexBestKnownBlock;
    //! The sha256d hash of the last unknown block this peer has announced.
    //! We can not use the real hash here, as the block maybe unknown to us.
    uintFakeHash hashLastUnknownBlock;
    //! The last full block we both have.
    CBlockIndex *pindexLastCommonBlock;
    //! Whether we've started headers synchronization with this peer.
    bool fSyncStarted;
    //! Since when we're stalling block download progress (in microseconds), or 0.
    int64_t nStallingSince;
    list<QueuedBlock> vBlocksInFlight;
    int nBlocksInFlight;
    //! Whether we consider this a preferred download peer.
    bool fPreferredDownload;

    CNodeState() {
        fCurrentlyConnected = false;
        nMisbehavior = 0;
        fShouldBan = false;
        pindexBestKnownBlock = NULL;
        hashLastUnknownBlock = uint256(0);
        pindexLastCommonBlock = NULL;
        fSyncStarted = false;
        nStallingSince = 0;
        nBlocksInFlight = 0;
        fPreferredDownload = false;
    }
};

/** Map maintaining per-node state. Requires cs_main. */
map<NodeId, CNodeState> mapNodeState;

// Requires cs_main.
CNodeState *State(NodeId pnode) {
    map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
    if (it == mapNodeState.end())
        return NULL;
    return &it->second;
}

int GetHeight()
{
    LOCK(cs_main);
    return chainActive.Height();
}

void UpdatePreferredDownload(CNode* node, CNodeState* state)
{
    nPreferredDownload -= state->fPreferredDownload;

    // Whether this node should be marked as a preferred download node.
    state->fPreferredDownload = (!node->fInbound || node->fWhitelisted || IsI2POnly()) && !node->fOneShot && !node->fClient;

    nPreferredDownload += state->fPreferredDownload;
}

void InitializeNode(NodeId nodeid, const CNode *pnode) {
    LOCK(cs_main);
    CNodeState &state = mapNodeState.insert(std::make_pair(nodeid, CNodeState())).first->second;
    state.name = pnode->addrName;
    state.address = pnode->addr;
}

void FinalizeNode(NodeId nodeid) {
    LOCK(cs_main);
    CNodeState *state = State(nodeid);

    if (state->fSyncStarted)
        nSyncStarted--;

    if (state->nMisbehavior == 0 && state->fCurrentlyConnected) {
        AddressCurrentlyConnected(state->address);
    }

    BOOST_FOREACH(const QueuedBlock& entry, state->vBlocksInFlight)
        mapBlocksInFlight.erase(entry.hash);
    EraseOrphansFor(nodeid);
    nPreferredDownload -= state->fPreferredDownload;

    mapNodeState.erase(nodeid);
}

// Requires cs_main.
void MarkBlockAsReceived(const uintFakeHash& hash) {
    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end()) {
        CNodeState *state = State(itInFlight->second.first);
        nQueuedValidatedHeaders -= itInFlight->second.second->fValidatedHeaders;
        state->vBlocksInFlight.erase(itInFlight->second.second);
        state->nBlocksInFlight--;
        state->nStallingSince = 0;
        mapBlocksInFlight.erase(itInFlight);
    }
}

// Requires cs_main.
void MarkBlockAsInFlight(NodeId nodeid, const uintFakeHash& hash, CBlockIndex *pindex = NULL) {
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    // Make sure it's not listed somewhere already.
    MarkBlockAsReceived(hash);

    QueuedBlock newentry = {hash, pindex, GetTimeMicros(), nQueuedValidatedHeaders, pindex != NULL};
    nQueuedValidatedHeaders += newentry.fValidatedHeaders;
    list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(), newentry);
    state->nBlocksInFlight++;
    mapBlocksInFlight[hash] = std::make_pair(nodeid, it);
}

/** Check whether the last unknown block a peer advertized is not yet known. */
void ProcessBlockAvailability(NodeId nodeid) {
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    if (!state->hashLastUnknownBlock.IsNull()) {
        uint256 aRealHash = state->hashLastUnknownBlock.GetRealHash();
        BlockMap::iterator itOld = (aRealHash != 0) ? mapBlockIndex.find(aRealHash) : mapBlockIndex.end();
        if (itOld != mapBlockIndex.end() && itOld->second->nChainWork > 0) {
            if (state->pindexBestKnownBlock == NULL || itOld->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
                state->pindexBestKnownBlock = itOld->second;
            state->hashLastUnknownBlock.SetNull();
        }
    }
}

/** Update tracking information about which blocks a peer is assumed to have. */
void UpdateBlockAvailability(NodeId nodeid, const uintFakeHash &hash) {
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    ProcessBlockAvailability(nodeid);
    uint256 aRealHash = hash.GetRealHash();
    BlockMap::iterator it = ( aRealHash != 0 ) ? mapBlockIndex.find( aRealHash ) : mapBlockIndex.end();
    if (it != mapBlockIndex.end() && it->second->nChainWork > 0) {
        // An actually better block was announced.
        if (state->pindexBestKnownBlock == NULL || it->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
            state->pindexBestKnownBlock = it->second;
    } else {
        // An unknown block was announced; just assume that the latest one is the best one.
        state->hashLastUnknownBlock = hash;
    }
}

/** Find the last common ancestor two blocks have.
 *  Both pa and pb must be non-NULL. */
CBlockIndex* LastCommonAncestor(CBlockIndex* pa, CBlockIndex* pb) {
    if (pa->nHeight > pb->nHeight) {
        pa = pa->GetAncestor(pb->nHeight);
    } else if (pb->nHeight > pa->nHeight) {
        pb = pb->GetAncestor(pa->nHeight);
    }

    while (pa != pb && pa && pb) {
        pa = pa->pprev;
        pb = pb->pprev;
    }

    // Eventually all chain branches meet at the genesis block.
    assert(pa == pb);
    return pa;
}

/** Update pindexLastCommonBlock and add not-in-flight missing successors to vBlocks, until it has
 *  at most count entries. */
void FindNextBlocksToDownload(NodeId nodeid, unsigned int count, std::vector<CBlockIndex*>& vBlocks, NodeId& nodeStaller) {
    if (count == 0)
        return;

    vBlocks.reserve(vBlocks.size() + count);
    CNodeState *state = State(nodeid);
    assert(state != NULL);

    // Make sure pindexBestKnownBlock is up to date, we'll need it.
    ProcessBlockAvailability(nodeid);

    if (state->pindexBestKnownBlock == NULL || state->pindexBestKnownBlock->nChainWork < chainActive.Tip()->nChainWork) {
        // This peer has nothing interesting.
        return;
    }

    if (state->pindexLastCommonBlock == NULL) {
        // Bootstrap quickly by guessing a parent of our best tip is the forking point.
        // Guessing wrong in either direction is not a problem.
        state->pindexLastCommonBlock = chainActive[std::min(state->pindexBestKnownBlock->nHeight, chainActive.Height())];
    }

    // If the peer reorganized, our previous pindexLastCommonBlock may not be an ancestor
    // of their current tip anymore. Go back enough to fix that.
    state->pindexLastCommonBlock = LastCommonAncestor(state->pindexLastCommonBlock, state->pindexBestKnownBlock);
    if (state->pindexLastCommonBlock == state->pindexBestKnownBlock)
        return;

    std::vector<CBlockIndex*> vToFetch;
    CBlockIndex *pindexWalk = state->pindexLastCommonBlock;
    // Never fetch further than the best block we know the peer has, or more than BLOCK_DOWNLOAD_WINDOW + 1 beyond the last
    // linked block we have in common with this peer. The +1 is so we can detect stalling, namely if we would be able to
    // download that next block if the window were 1 larger.
    int nWindowEnd = state->pindexLastCommonBlock->nHeight + BLOCK_DOWNLOAD_WINDOW;
    int nMaxHeight = std::min<int>(state->pindexBestKnownBlock->nHeight, nWindowEnd + 1);
    NodeId waitingfor = -1;
    while (pindexWalk->nHeight < nMaxHeight) {
        // Read up to 128 (or more, if more blocks than that are needed) successors of pindexWalk (towards
        // pindexBestKnownBlock) into vToFetch. We fetch 128, because CBlockIndex::GetAncestor may be as expensive
        // as iterating over ~100 CBlockIndex* entries anyway.
        int nToFetch = std::min(nMaxHeight - pindexWalk->nHeight, std::max<int>(count - vBlocks.size(), 128));
        vToFetch.resize(nToFetch);
        pindexWalk = state->pindexBestKnownBlock->GetAncestor(pindexWalk->nHeight + nToFetch);
        vToFetch[nToFetch - 1] = pindexWalk;
        for (unsigned int i = nToFetch - 1; i > 0; i--) {
            vToFetch[i - 1] = vToFetch[i]->pprev;
        }

        // Iterate over those blocks in vToFetch (in forward direction), adding the ones that
        // are not yet downloaded and not in flight to vBlocks. In the mean time, update
        // pindexLastCommonBlock as long as all ancestors are already downloaded.
        BOOST_FOREACH(CBlockIndex* pindex, vToFetch) {
            if (!pindex->IsValid(BLOCK_VALID_TREE)) {
                // We consider the chain that this peer is on invalid.
                return;
            }
            if (pindex->nStatus & BLOCK_HAVE_DATA) {
                if (pindex->nChainTx)
                    state->pindexLastCommonBlock = pindex;
            } else if (mapBlocksInFlight.count(pindex->GetBlockSha256dHash()) == 0) {
                // The block is not already downloaded, and not yet in flight.
                if (pindex->nHeight > nWindowEnd) {
                    // We reached the end of the window.
                    if (vBlocks.size() == 0 && waitingfor != nodeid) {
                        // We aren't able to fetch anything, but we would be if the download window was one larger.
                        nodeStaller = waitingfor;
                    }
                    return;
                }
                vBlocks.push_back(pindex);
                if (vBlocks.size() == count) {
                    return;
                }
            } else if (waitingfor == -1) {
                // This is the first already-in-flight block.
                waitingfor = mapBlocksInFlight[pindex->GetBlockSha256dHash()].first;
            }
        }
    }
}

} // anon namespace

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats) {
    LOCK(cs_main);
    CNodeState *state = State(nodeid);
    if (state == NULL)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    stats.nSyncHeight = state->pindexBestKnownBlock ? state->pindexBestKnownBlock->nHeight : -1;
    stats.nCommonHeight = state->pindexLastCommonBlock ? state->pindexLastCommonBlock->nHeight : -1;
    BOOST_FOREACH(const QueuedBlock& queue, state->vBlocksInFlight) {
        if (queue.pindex)
            stats.vHeightInFlight.push_back(queue.pindex->nHeight);
    }
    return true;
}

void RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.connect(&GetHeight);
    nodeSignals.ProcessMessages.connect(&ProcessMessages);
    nodeSignals.SendMessages.connect(&SendMessages);
    nodeSignals.InitializeNode.connect(&InitializeNode);
    nodeSignals.FinalizeNode.connect(&FinalizeNode);
}

void UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.disconnect(&GetHeight);
    nodeSignals.ProcessMessages.disconnect(&ProcessMessages);
    nodeSignals.SendMessages.disconnect(&SendMessages);
    nodeSignals.InitializeNode.disconnect(&InitializeNode);
    nodeSignals.FinalizeNode.disconnect(&FinalizeNode);
}

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH(const uintFakeHash& aFakeHash, locator.vHave) {
        uint256 aRealHash = aFakeHash.GetRealHash();
        BlockMap::iterator mi = ( aRealHash != 0 ) ? mapBlockIndex.find( aRealHash ) : mapBlockIndex.end();
        if (mi != mapBlockIndex.end())
        {
            CBlockIndex* pindex = (*mi).second;
            if (chain.Contains(pindex))
                return pindex;
        }
    }
    return chain.Genesis();
}

CCoinsViewCache *pcoinsTip = NULL;
CBlockTreeDB *pblocktree = NULL;

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx, NodeId peer)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz > 5000)
    {
        LogPrint("mempool", "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
        return false;
    }

    mapOrphanTransactions[hash].tx = tx;
    mapOrphanTransactions[hash].fromPeer = peer;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    LogPrint("mempool", "stored orphan tx %s (mapsz %u prevsz %u)\n", hash.ToString(),
             mapOrphanTransactions.size(), mapOrphanTransactionsByPrev.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.find(hash);
    if (it == mapOrphanTransactions.end())
        return;
    BOOST_FOREACH(const CTxIn& txin, it->second.tx.vin)
    {
        map<uint256, set<uint256> >::iterator itPrev = mapOrphanTransactionsByPrev.find(txin.prevout.hash);
        if (itPrev == mapOrphanTransactionsByPrev.end())
            continue;
        itPrev->second.erase(hash);
        if (itPrev->second.empty())
            mapOrphanTransactionsByPrev.erase(itPrev);
    }
    mapOrphanTransactions.erase(it);
}

void EraseOrphansFor(NodeId peer)
{
    int nErased = 0;
    map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end())
    {
        map<uint256, COrphanTx>::iterator maybeErase = iter++; // increment to avoid iterator becoming invalid
        if (maybeErase->second.fromPeer == peer)
        {
            EraseOrphanTx(maybeErase->second.tx.GetHash());
            ++nErased;
        }
    }
    if (nErased > 0) LogPrint("mempool", "Erased %d orphan tx from peer %d\n", nErased, peer);
}


unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans)
    {
        // Evict a random orphan:
        uint256 randomhash = GetRandHash();
        map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}







bool IsStandardTx(const CTransaction& tx, string& reason)
{
    AssertLockHeld(cs_main);
    if (tx.nVersion > CTransaction::CURRENT_VERSION || tx.nVersion < 1) {
        reason = "version";
        return false;
    }

    // Treat non-final transactions as non-standard to prevent a specific type
    // of double-spend attack, as well as DoS attacks. (if the transaction
    // can't be mined, the attacker isn't expending resources broadcasting it)
    // Basically we don't want to propagate transactions that can't be included in
    // the next block.
    //
    // However, IsFinalTx() is confusing... Without arguments, it uses
    // chainActive.Height() to evaluate nLockTime; when a block is accepted, chainActive.Height()
    // is set to the value of nHeight in the block. However, when IsFinalTx()
    // is called within CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a transaction can
    // be part of the *next* block, we need to call IsFinalTx() with one more
    // than chainActive.Height().
    //
    // Timestamps on the other hand don't get any special treatment, because we
    // can't know what timestamp the next block will have, and there aren't
    // timestamp applications where it matters.
    if (!IsFinalTx(tx, chainActive.Height() + 1)) {
        reason = "non-final";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE) {
        reason = "tx-size";
        return false;
    }

    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)=1624
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)
        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
        if (!txin.scriptSig.HasCanonicalPushes()) {
            reason = "scriptsig-non-canonical-push";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    unsigned int nTxnOut = 0;
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut& txout, tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else {
            if (txout.IsDust(minRelayTxFee)) {
                reason = "dust";
                return false;
            }
            nTxnOut++;
        }
    }
#ifdef ENABLE_STEALTH
    if (nDataOut > nTxnOut) {
        LogPrintf("Found %d OP_RETURNs versus %d TxOut. Not allowed, rejecting!", nDataOut, nTxnOut);
#else
    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
#endif
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Height();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& prev = mapInputs.GetOutputFor(tx.vin[i]);

        vector<vector<uint8_t> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig
        // IsStandard() will have already returned false
        // and this method isn't called.
        vector<vector<uint8_t> > stack;
        if (!EvalScript(stack, tx.vin[i].scriptSig, false, BaseSignatureChecker()))
            return false;

        if (whichType == TX_SCRIPTHASH)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<uint8_t> > vSolutions2;
            txnouttype whichType2;
            if (!Solver(subscript, whichType2, vSolutions2))
                return false;
            if (whichType2 == TX_SCRIPTHASH)
                return false;

            int tmpExpected;
            tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
            if (tmpExpected < 0)
                return false;
            nArgsExpected += tmpExpected;
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut &prevout = inputs.GetOutputFor(tx.vin[i]);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}








bool CheckTransaction(const CTransaction& tx, CValidationState &state)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, error("CheckTransaction() : vin empty"),
                         REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, error("CheckTransaction() : vout empty"),
                         REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CheckTransaction() : size limits failed"),
                         REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    int64_t nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, error("CheckTransaction() : txout.nValue negative"),
                             REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, error("CheckTransaction() : txout.nValue too high"),
                             REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CheckTransaction() : txout total out of range"),
                             REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, error("CheckTransaction() : duplicate inputs"),
                             REJECT_INVALID, "bad-txns-inputs-duplicate");
        vInOutPoints.insert(txin.prevout);
    }

    /** GR Notes 12/2014: The following chunck of code was commented out in Anc 8.6,
     * The problem was the Genesis Block has a string 95 bytes long, so this was failing
     * with script size error, this important set of tests on the transaction never got
     * 'uncommented' and fixed so the following CheckTransaction code could run properly.
     * We set the size limit slightly higher, so our Genesis does not error, and we get
     * V9 wallets to build the blockchain database properly with the test intact.
     * Bitcoin's value here was size() > 100....changed to 120
     */
    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 120)
            return state.DoS(100, error("CheckTransaction() : coinbase script size"),
                             REJECT_INVALID, "bad-cb-length");
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
            if (txin.prevout.IsNull())
                return state.DoS(10, error("CheckTransaction() : prevout is null"),
                                 REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}


int64_t GetMinRelayFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree)
{
    {
        LOCK(mempool.cs);
        uint256 hash = tx.GetHash();
        double dPriorityDelta = 0;
        CAmount nFeeDelta = 0;
        mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
        if (dPriorityDelta > 0 || nFeeDelta > 0)
            return 0;
    }

    CAmount nMinFee = ::minRelayTxFee.GetFee(nBytes);

    if (fAllowFree)
    {
        // There is a free transaction area in blocks created by most miners,
        // * If we are relaying we allow transactions up to DEFAULT_BLOCK_PRIORITY_SIZE - 1000
        //   to be considered to fall into this category. We don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        if (nBytes < (DEFAULT_BLOCK_PRIORITY_SIZE - 1000))
            nMinFee = 0;
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}


bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState &state, const CTransaction &tx, bool fLimitFree,
                        bool* pfMissingInputs, bool fRejectInsaneFee)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!CheckTransaction(tx, state))
        return error("AcceptToMemoryPool: : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, error("AcceptToMemoryPool: : coinbase as individual tx"),
                         REJECT_INVALID, "coinbase");

    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    if (isMainNetwork() && !IsStandardTx(tx, reason))
        return state.DoS(0,
                         error("AcceptToMemoryPool : nonstandard transaction: %s", reason),
                         REJECT_NONSTANDARD, reason);

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // Check for conflicts with in-memory transactions
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint)) {
                // Disable replacement feature for now
                return false;
            }
        }
    }

    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        int64_t nValueIn = 0;
        {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            // do we already have it?
            if (view.HaveCoins(hash))
                return false;

            // do all inputs exist?
            // Note that this does not check for the presence of actual outputs (see the next check for that),
            // only helps filling in pfMissingInputs (to determine missing vs spent).
            BOOST_FOREACH(const CTxIn txin, tx.vin) {
                uint256 prevHash = txin.prevout.hash;
                if (!view.HaveCoins(prevHash)) {
                    LogPrintf( "%s : While processing Tx hash %s missing inputs found, no TxIn.prevout %s\n", __func__, hash.ToString(), prevHash.ToString() );
                    if (pfMissingInputs)
                        *pfMissingInputs = true;
                    return false;
                }
            }

            // are the actual inputs available?
            if (!view.HaveInputs(tx))
                return state.Invalid(error("AcceptToMemoryPool : inputs already spent"),
                                     REJECT_DUPLICATE, "bad-txns-inputs-spent");

            // Bring the best block into scope
            view.GetBestBlock();

            // Be SURE to grab the transaction ValueIn BEFORE the view is returned to a dummy state!
            nValueIn = view.GetValueIn(tx);

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (isMainNetwork() && !AreInputsStandard(tx, view))
            return error("AcceptToMemoryPool: : nonstandard transaction input");

#ifdef HARDFORK_BLOCK
        // ToDo: Ask the dev team about including this...
        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);
        if (nSigOps > MAX_TX_SIGOPS)
            return state.DoS(0,
                             error("AcceptToMemoryPool : too many sigops %s, %d > %d",
                                   hash.ToString(), nSigOps, MAX_TX_SIGOPS),
                             REJECT_NONSTANDARD, "bad-txns-too-many-sigops");
#endif
        int64_t nValueOut = tx.GetValueOut();
        int64_t nFees = nValueIn - nValueOut;
        double dPriority = view.GetPriority(tx, chainActive.Height());

        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height());
        unsigned int nSize = entry.GetTxSize();

        // Don't accept it if it can't get into a block
        int64_t txMinFee = GetMinRelayFee(tx, nSize, true);
        if (fLimitFree && nFees < txMinFee)
            return state.DoS(0, error("AcceptToMemoryPool : not enough fees %s, %d < %d",
                                      hash.ToString(), nFees, txMinFee),
                             REJECT_INSUFFICIENTFEE, "insufficient fee");

        // Require that free transactions have sufficient priority to be mined in the next block.
        if (GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(view.GetPriority(tx, chainActive.Height() + 1))) {
            return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
        }
        // Continuously rate-limit free transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (fLimitFree && nFees < ::minRelayTxFee.GetFee(nSize))
        {
            static CCriticalSection csFreeLimiter;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow = GetTime();

            LOCK(csFreeLimiter);

            // Use an exponentially decaying ~10-minute window:
            dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
            nLastTime = nNow;
            // -limitfreerelay unit is thousand-bytes-per-minute
            // At default rate it would take over a month to fill 1GB
            if (dFreeCount >= GetArg("-limitfreerelay", 15)*10*1000)
                return state.DoS(0, error("AcceptToMemoryPool : free transaction rejected by rate limiter"),
                                 REJECT_INSUFFICIENTFEE, "rate limited free transaction");
            LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
            dFreeCount += nSize;
        }

        if (fRejectInsaneFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000)
            return error("AcceptToMemoryPool: : insane fees %s, %d > %d",
                         hash.ToString(),
                         nFees, ::minRelayTxFee.GetFee(nSize) * 10000);

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!CheckInputs(tx, state, view, true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC, true))
        {
            return error("AcceptToMemoryPool: : ConnectInputs failed %s", hash.ToString());
        }

        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        if (!CheckInputs(tx, state, view, true, SCRIPT_VERIFY_P2SH, true))
        {
            return error("AcceptToMemoryPool: : BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString());
        }

        // Store transaction in memory
        pool.addUnchecked(hash, entry);
    }

    SyncWithWallets(tx, NULL);

    return true;
}

/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256 &hash, CTransaction &txOut, uintFakeHash &hashBlock, bool fAllowSlow)
{
    CBlockIndex *pindexSlow = NULL;
    {
        LOCK(cs_main);
        {
            if (mempool.lookup(hash, txOut))
            {
                return true;
            }
        }

        if (fTxIndex) {
            CDiskTxPos postx;
            if (pblocktree->ReadTxIndex(hash, postx)) {
                CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
                if (file.IsNull())
                    return error("%s : OpenBlockFile failed", __func__);
                CBlockHeader header;
                try {
                    file >> header;
                    fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                    file >> txOut;
                } catch (const std::exception& e) {
                    return error("%s : Deserialize or I/O error - %s", __func__, e.what());
                }
                //! Returning a block hash to the outside world means the sha256d hash is needed
                hashBlock = header.CalcSha256dHash();
                if (txOut.GetHash() != hash)
                    return error("%s : txid mismatch", __func__);
                return true;
            }
        }

        if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
            int nHeight = -1;
            {
                CCoinsViewCache &view = *pcoinsTip;
                const CCoins* coins = view.AccessCoins(hash);
                if (coins)
                    nHeight = coins->nHeight;
            }
            if (nHeight > 0)
                pindexSlow = chainActive[nHeight];
        }
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow)) {
            BOOST_FOREACH(const CTransaction &tx, block.vtx) {
                if (tx.GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockSha256dHash();
                    return true;
                }
            }
        }
    }

    return false;
}






//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool WriteBlockToDisk(CBlock& block, CDiskBlockPos& pos)
{
    // Open history file to append
    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk : OpenBlockFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(block);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos)
{
    block.SetNull();

    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk : OpenBlockFile failed");

    // Read block
    try {
        filein >> block;
    }
    catch (const std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Check the header POW
    if( !CheckProofOfWork( block.GetHash(), block.nBits) )
            return error("ReadBlockFromDisk : Errors in block header");

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex)
{
    if (!ReadBlockFromDisk(block, pindex->GetBlockPos()))
        return false;
    if (block.GetHash() != pindex->GetBlockHash())
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*) : GetHash() doesn't match index");
    return true;
}

/**
 * The primary routine, which determines Anoncoins mined value over time...
 */
int64_t GetBlockValue(int nHeight, int64_t nFees)
{
    int64_t nSubsidy = 5 * COIN;
    // Some adjustments to the start of the lifetime to Anoncoin
    if (nHeight < 42000) {
        nSubsidy = 4.2 * COIN;
    } else if (nHeight < 77777) { // All luck is seven ;)
        nSubsidy = 7 * COIN;
    } else if (nHeight == 77778) {
        nSubsidy = 10 * COIN;
    } else {
        nSubsidy >>= (nHeight / 306600); // Anoncoin: 306600 blocks in ~2 years
    }
    return nSubsidy + nFees;
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    // LogPrintf( "IsInitialBlockDownload() fImporting=%d fReindex=%d Checkpoints=%d\n", fImporting, fReindex, chainActive.Height() < Checkpoints::GetTotalBlocksEstimate() );
    if (fImporting || fReindex || chainActive.Height() < Checkpoints::GetTotalBlocksEstimate())
        return true;
    // ToDo:
    static bool lockIBDState = false;
    // LogPrintf( "IsInitialBlockDownload() lockIBDState=%d BestHeader Height=%d BlockTime=%d\n", lockIBDState, pindexBestHeader->nHeight, pindexBestHeader->GetBlockTime() );
    if (lockIBDState)
        return false;
    // With 3min blocks, we have ~20/hour set now for 8hrs, btc was 24
    // Else, allow up to a one week old non-synchronized blockchain, in case of mining problem
    bool state = (chainActive.Height() < pindexBestHeader->nHeight - 8 * 20 ||
            pindexBestHeader->GetBlockTime() < GetTime() - 7 * 24 * 60 * 60);
    if (!state)
        lockIBDState = true;
    return state;
}

bool fLargeWorkForkFound = false;
bool fLargeWorkInvalidChainFound = false;
CBlockIndex *pindexBestForkTip = NULL, *pindexBestForkBase = NULL;

void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);
    // Before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before the last checkpoint)
    if (IsInitialBlockDownload())
        return;
    // If our best fork is no longer within ~12 hours of our head, drop it
    // Adjusted for Anoncoin's 3 minute block time target
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 240)
        pindexBestForkTip = NULL;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6)))
    {
        if (!fLargeWorkForkFound && pindexBestForkBase)
        {
            std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") +
                pindexBestForkBase->phashBlock->ToString() + std::string("'");
            CAlert::Notify(warning, true);
        }
        if (pindexBestForkTip && pindexBestForkBase)
        {
            LogPrintf("CheckForkWarningConditions: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n",
                   pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                   pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
            fLargeWorkForkFound = true;
        }
        else
        {
            LogPrintf("CheckForkWarningConditions: Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.\n");
            fLargeWorkInvalidChainFound = true;
        }
    }
    else
    {
        fLargeWorkForkFound = false;
        fLargeWorkInvalidChainFound = false;
    }
}

void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);
    // If we are on a fork that is sufficiently large, set a warning flag
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger)
    {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // We define a condition which we should warn the user about as a fork of at least 20 blocks
    // who's tip is within 240 blocks (+/- 12 hours if no one mines it) of ours
    // We use 20 blocks rather arbitrarily as it represents just under ??% of sustained network
    // hash rate operating on the fork.
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 20-block condition and from this always have the most-likely-to-cause-warning fork
    // ToDo: Note to team--> GR Adjusted values based on Anoncoin 3min target time, although setting 20 blocks was arbitrary
    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->nHeight > pindexBestForkTip->nHeight)) &&
            pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 20) &&
            chainActive.Height() - pindexNewForkTip->nHeight < 240)
    {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

// Requires cs_main.
void Misbehaving(NodeId pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    CNodeState *state = State(pnode);
    if (state == NULL)
        return;

    state->nMisbehavior += howmuch;
    int banscore = GetArg("-banscore", 100);
    if (state->nMisbehavior >= banscore && state->nMisbehavior - howmuch < banscore)
    {
        LogPrintf("Misbehaving: %s (%d -> %d) BAN THRESHOLD EXCEEDED\n", state->name, state->nMisbehavior-howmuch, state->nMisbehavior);
        state->fShouldBan = true;
    } else
        LogPrintf("Misbehaving: %s (%d -> %d)\n", state->name, state->nMisbehavior-howmuch, state->nMisbehavior);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;

    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n",
      pindexNew->GetBlockHash().ToString(), pindexNew->nHeight,
      GetLog2Work(pindexNew->nChainWork), DateTimeStrFormat("%Y-%m-%d %H:%M:%S",
      pindexNew->GetBlockTime()));
    LogPrintf("InvalidChainFound:  current best=%s  height=%d  log2_work=%.8g  date=%s\n",
      chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), GetLog2Work(chainActive.Tip()->nChainWork),
      DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()));
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex *pindex, const CValidationState &state) {
    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
        std::map<uint256, NodeId>::iterator it = mapBlockSource.find(pindex->GetBlockHash());
        if (it != mapBlockSource.end() && State(it->second)) {
            CBlockReject reject = {state.GetRejectCode(), state.GetRejectReason(), pindex->GetBlockSha256dHash()};
            State(it->second)->rejects.push_back(reject);
            if (nDoS > 0)
                Misbehaving(it->second, nDoS);
        }
    }
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CValidationState &state, CCoinsViewCache &inputs, CTxUndo &txundo, int nHeight)
{
    // mark inputs spent
    if (!tx.IsCoinBase()) {
        txundo.vprevout.reserve(tx.vin.size());
        BOOST_FOREACH(const CTxIn &txin, tx.vin) {
            CCoinsModifier coins = inputs.ModifyCoins(txin.prevout.hash);
            unsigned nPos = txin.prevout.n;

            if (nPos >= coins->vout.size() || coins->vout[nPos].IsNull())
                assert(false);
            // mark an outpoint spent, and construct undo information
            txundo.vprevout.push_back(CTxInUndo());
            bool ret = coins->Spend(txin.prevout, txundo.vprevout.back());
            assert(ret);
        }
    }

    // add outputs
    inputs.ModifyCoins(tx.GetHash())->FromTx(tx, nHeight);
}

void UpdateCoins(const CTransaction& tx, CValidationState &state, CCoinsViewCache &inputs, int nHeight)
{
    CTxUndo txundo;
    UpdateCoins(tx, state, inputs, txundo, nHeight);
}

bool CScriptCheck::operator()() {
    const CScript &scriptSig = ptxTo->vin[nIn].scriptSig;
/**
 * Valid signature cache, to avoid doing expensive ECDSA signature checking
 * twice for every transaction (once when accepted into memory pool, and
 * again when accepted into the block chain)
 */

    if (!VerifyScript(scriptSig, scriptPubKey, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, cacheStore), &error)) {
        return ::error("CScriptCheck(): %s:%d VerifySignature failed: %s", ptxTo->GetHash().ToString(), nIn, ScriptErrorString(error));
    }
    return true;
}

bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, std::vector<CScriptCheck> *pvChecks)
{
    if (!tx.IsCoinBase())
    {
        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

        // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
        // for an attacker to attempt to split the network.
        if (!inputs.HaveInputs(tx))
            return state.Invalid(error("CheckInputs() : %s inputs unavailable", tx.GetHash().ToString()));

        // While checking, GetBestBlock() refers to the parent block.
        // This is also true for mempool checks.
        // If it was not already setup properly, this call also brings the best block into scope for the view,
        // after consulting the base view, which could be programmed wrong or have returned 0
        uint256 viewBestBlock = inputs.GetBestBlock();
        BlockMap::iterator itBM = (viewBestBlock != 0) ? mapBlockIndex.find( viewBestBlock ) : mapBlockIndex.end();
        if( itBM == mapBlockIndex.end() )
            return state.Invalid(error("%s : for Tx %s view GetBestBlock() returned %s not setup correctly.", __func__, tx.GetHash().ToString(), viewBestBlock.ToString()));

        CBlockIndex *pindexPrev = itBM->second;
        int nSpendHeight = pindexPrev->nHeight + 1;
        CAmount nValueIn = 0;
        CAmount nFees = 0;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            const COutPoint &prevout = tx.vin[i].prevout;
            const CCoins *coins = inputs.AccessCoins(prevout.hash);
            assert(coins);

            // If prev is coinbase, check that it's matured
            if (coins->IsCoinBase()) {
                if (nSpendHeight - coins->nHeight < COINBASE_MATURITY)
                    return state.Invalid(
                        error("CheckInputs() : tried to spend coinbase at depth %d", nSpendHeight - coins->nHeight),
                        REJECT_INVALID, "bad-txns-premature-spend-of-coinbase");
            }

            // Check for negative or overflow input values
            nValueIn += coins->vout[prevout.n].nValue;
            if (!MoneyRange(coins->vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return state.DoS(100, error("CheckInputs() : txin values out of range"),
                                 REJECT_INVALID, "bad-txns-inputvalues-outofrange");

        }

        if (nValueIn < tx.GetValueOut())
            return state.DoS(100, error("CheckInputs() : %s value in (%s) < value out (%s)",
                                        tx.GetHash().ToString(), FormatMoney(nValueIn), FormatMoney(tx.GetValueOut())),
                             REJECT_INVALID, "bad-txns-in-belowout");

        // Tally transaction fees
        CAmount nTxFee = nValueIn - tx.GetValueOut();
        if (nTxFee < 0)
            return state.DoS(100, error("CheckInputs() : %s nTxFee < 0", tx.GetHash().ToString()),
                             REJECT_INVALID, "bad-txns-fee-negative");
        nFees += nTxFee;
        if (!MoneyRange(nFees))
            return state.DoS(100, error("CheckInputs() : nFees out of range"),
                             REJECT_INVALID, "bad-txns-fee-outofrange");

        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.

        // Skip ECDSA signature verification when connecting blocks
        // before the last block chain checkpoint. This is safe because block merkle hashes are
        // still computed and checked, and any change will be caught at the next checkpoint.
        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint &prevout = tx.vin[i].prevout;
                const CCoins* coins = inputs.AccessCoins(prevout.hash);
                assert(coins);

                // Verify signature
                CScriptCheck check(*coins, tx, i, flags, cacheStore);
                if (pvChecks) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & SCRIPT_VERIFY_STRICTENC) {
                        // Check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard DER encodings or non-null dummy
                        // arguments; if so, don't trigger DoS protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        CScriptCheck check(*coins, tx, i,
                                flags & (~SCRIPT_VERIFY_STRICTENC), cacheStore);
                        if (check())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
                    // Failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid P2SH. We DoS ban
                    // such nodes as they are not following the protocol. That
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after a soft-fork
                    // super-majority vote has passed.
                    return state.DoS(100,false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }
        }
    }

    return true;
}



bool DisconnectBlock(CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool* pfClean)
{
    assert(pindex->GetBlockHash() == view.GetBestBlock());

    if (pfClean)
        *pfClean = false;

    bool fClean = true;

    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull())
        return error("DisconnectBlock() : no undo data available");
    if (!blockUndo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
        return error("DisconnectBlock() : failure reading undo data");

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size())
        return error("DisconnectBlock() : block and undo data inconsistent");

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction &tx = block.vtx[i];
        uint256 hash = tx.GetHash();

        // Check that all outputs are available and match the outputs in the block itself
        // exactly. Note that transactions with only provably unspendable outputs won't
        // have outputs available even in the block itself, so we handle that case
        // specially with outsEmpty.
        {
            CCoins outsEmpty;
            CCoinsModifier outs = view.ModifyCoins(hash);
            outs->ClearUnspendable();

            CCoins outsBlock(tx, pindex->nHeight);
            // The CCoins serialization does not serialize negative numbers.
            // No network rules currently depend on the version here, so an inconsistency is harmless
            // but it must be corrected before txout nversion ever influences a network rule.
            if (outsBlock.nVersion < 0)
                outs->nVersion = outsBlock.nVersion;
            if (*outs != outsBlock)
                fClean = fClean && error("DisconnectBlock() : added transaction mismatch? database corrupted");

            // remove outputs
            outs->Clear();
        }

        // restore inputs
        if (i > 0) { // not coinbases
            const CTxUndo &txundo = blockUndo.vtxundo[i-1];
            if (txundo.vprevout.size() != tx.vin.size())
                return error("DisconnectBlock() : transaction and undo data inconsistent");
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const COutPoint &out = tx.vin[j].prevout;
                const CTxInUndo &undo = txundo.vprevout[j];
                CCoinsModifier coins = view.ModifyCoins(out.hash);
                if (undo.nHeight != 0) {
                    // undo data contains height: this is the last output of the prevout tx being spent
                    if (!coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data overwriting existing transaction");
                    coins->Clear();
                    coins->fCoinBase = undo.fCoinBase;
                    coins->nHeight = undo.nHeight;
                    coins->nVersion = undo.nVersion;
                } else {
                    if (coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data adding output to missing transaction");
                }
                if (coins->IsAvailable(out.n))
                    fClean = fClean && error("DisconnectBlock() : undo data overwriting existing output");
                if (coins->vout.size() < out.n+1)
                    coins->vout.resize(out.n+1);
                coins->vout[out.n] = undo.txout;
            }
        }
    }

    // move best block pointer to prevout block
    view.SetBestBlock(pindex->pprev->GetBlockHash());

    if (pfClean) {
        *pfClean = fClean;
        return true;
    } else {
        return fClean;
    }
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE *fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }
}

bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck() {
    RenameThread("anoncoin-scriptch");
    scriptcheckqueue.Thread();
}

static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool fJustCheck)
{
    AssertLockHeld(cs_main);
    // Check it again in case a previous version let a bad block in
    if (!CheckBlock(block, state, !fJustCheck, !fJustCheck))
        return false;

    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256(0) : pindex->pprev->GetBlockHash();
    // This assertion fails when trying to run Anoncoin v9.5 with an old blockindex database
    // Trying to handle it better, log the problem, tell them the solution and shutdown.
    // assert(hashPrevBlock == view.GetBestBlock());
    if( hashPrevBlock != view.GetBestBlock()) {
        LogPrintf( "ERROR - You maybe trying to run Anoncoin, without having 1st reindexed the BlockChain, executing shutdown.\n" );
        LogPrintf( "ConnectBlock variables: indexheight=%d hashPrevBlock=%s viewBestBloc=%s\n", pindex->nHeight, hashPrevBlock.ToString(), view.GetBestBlock().ToString() );
        AbortNode( _("Internal ERROR - Invalid BlockIndex") );
        return false;
    }

    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    if (block.GetHash() == Params().HashGenesisBlock()) {
        if (!fJustCheck)
            view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

    bool fScriptChecks = pindex->nHeight >= Checkpoints::GetTotalBlocksEstimate();

    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
    // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. This prevents exploiting the issue against nodes in their
    // initial block download.  Anoncoin always enforces it....
    bool fEnforceBIP30 = true;


    if (fEnforceBIP30) {
        BOOST_FOREACH(const CTransaction& tx, block.vtx) {
            const CCoins* coins = view.AccessCoins(tx.GetHash());
            if (coins && !coins->IsPruned())
                return state.DoS(100, error("ConnectBlock() : tried to overwrite transaction"),
                                 REJECT_INVALID, "bad-txns-BIP30");
        }
    }

    // BIP16 didn't become active until Apr 1 2012, again Anoncoin always enforces it.
    int64_t nBIP16SwitchTime = 1333238400;
    bool fStrictPayToScriptHash = (pindex->GetBlockTime() >= nBIP16SwitchTime);

    // unsigned int flags = SCRIPT_VERIFY_LOW_S;        ToDo: Can not support this flag yet
    unsigned int flags = 0;
    flags |= fStrictPayToScriptHash ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE;

    // Start enforcing the DERSIG (BIP66) rules, for block.nVersion=3 blocks, when 75% of the network has upgraded:
    // Anoncoin does not yet have such, however the coding work is gettind done and we intend a v3 block w/multi-algo
    // if (block.nVersion >= 3 && CBlockIndex::IsSuperMajority(3, pindex->pprev, Params().EnforceBlockUpgradeMajority())) {
    // if (block.nVersion >= 3 ) {
    //    flags |= SCRIPT_VERIFY_DERSIG;
    //}

    CBlockUndo blockundo;

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    int64_t nTimeStart = GetTimeMicros();
    int64_t nFees = 0;
    int nInputs = 0;
    unsigned int nSigOps = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const CTransaction &tx = block.vtx[i];

        nInputs += tx.vin.size();
        nSigOps += GetLegacySigOpCount(tx);
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return state.DoS(100, error("ConnectBlock() : too many sigops"),
                             REJECT_INVALID, "bad-blk-sigops");

        if (!tx.IsCoinBase())
        {
            if (!view.HaveInputs(tx))
                return state.DoS(100, error("ConnectBlock() : inputs missing/spent"),
                                 REJECT_INVALID, "bad-txns-inputs-missingorspent");

            if (fStrictPayToScriptHash)
            {
                // Add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nSigOps += GetP2SHSigOpCount(tx, view);
                if (nSigOps > MAX_BLOCK_SIGOPS)
                    return state.DoS(100, error("ConnectBlock() : too many sigops"),
                                     REJECT_INVALID, "bad-blk-sigops");
            }

            nFees += view.GetValueIn(tx)-tx.GetValueOut();

            std::vector<CScriptCheck> vChecks;
            if (!CheckInputs(tx, state, view, fScriptChecks, flags, false, nScriptCheckThreads ? &vChecks : NULL))
                return false;
            control.Add(vChecks);
        }

        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, state, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->nHeight);

        vPos.push_back(std::make_pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }
    int64_t nTime1 = GetTimeMicros(); nTimeConnect += nTime1 - nTimeStart;
    LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime1 - nTimeStart), 0.001 * (nTime1 - nTimeStart) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime1 - nTimeStart) / (nInputs-1), nTimeConnect * 0.000001);

    if (block.vtx[0].GetValueOut() > GetBlockValue(pindex->nHeight, nFees))
        return state.DoS(100,
                         error("ConnectBlock() : coinbase pays too much (actual=%d vs limit=%d)",
                               block.vtx[0].GetValueOut(), GetBlockValue(pindex->nHeight, nFees)),
                               REJECT_INVALID, "bad-cb-amount");

    if (!control.Wait())
        return state.DoS(100, false);
    int64_t nTime2 = GetTimeMicros(); nTimeVerify += nTime2 - nTimeStart;
    LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime2 - nTimeStart), nInputs <= 1 ? 0 : 0.001 * (nTime2 - nTimeStart) / (nInputs-1), nTimeVerify * 0.000001);

    if (fJustCheck)
        return true;

    // Write undo information to disk
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS))
    {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos pos;
            if (!FindUndoPos(state, pindex->nFile, pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock() : FindUndoPos failed");
            if (!blockundo.WriteToDisk(pos, pindex->pprev->GetBlockHash()))
                return state.Abort(_("Failed to write undo data"));

            // update nUndoPos in block index
            pindex->nUndoPos = pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return state.Abort(_("Failed to write transaction index"));

    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime3 = GetTimeMicros(); nTimeIndex += nTime3 - nTime2;
    LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2), nTimeIndex * 0.000001);

//! Removing this from here and placing it later in the validation process solved Windows Build crashing problems
//! which took many hours of debug time to figure out well enough that at least the immediate problem was solved.
//! Which QT builds crash with this here?  That remains a question mark so putting this on the ToDo: list as moving
//! this code to ConnectTip(), right after the UpdateTip() call is not yet fully understood as to why it caused only
//! a crash in Windows QT programs built against the QT 5.2.1 library.
#if defined( DONT_COMPILE )
    // Watch for changes to the previous coinbase transaction, first time here the static variable with be null.
    static uint256 hashPrevBestCoinBase;
    if( !hashPrevBestCoinBase.IsNull() ) {
        g_signals.UpdatedTransaction( hashPrevBestCoinBase );
    }
    hashPrevBestCoinBase = block.vtx[0].GetHash();
#endif // defined

    int64_t nTime4 = GetTimeMicros(); nTimeCallbacks += nTime4 - nTime3;
    LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3), nTimeCallbacks * 0.000001);

    return true;
}

enum FlushStateMode {
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed if either they're too large, forceWrite is set, or
 * fast is not set and it's been a while since the last write.
 */
bool static FlushStateToDisk(CValidationState &state, FlushStateMode mode) {
    LOCK2(cs_main, cs_LastBlockFile);
    static int64_t nLastWrite = 0;
    try {
    if ((mode == FLUSH_STATE_ALWAYS) ||
        ((mode == FLUSH_STATE_PERIODIC || mode == FLUSH_STATE_IF_NEEDED) && pcoinsTip->GetCacheSize() > nCoinCacheSize) ||
        (mode == FLUSH_STATE_PERIODIC && GetTimeMicros() > nLastWrite + DATABASE_WRITE_INTERVAL * 1000000)) {
        // Typical CCoins structures on disk are around 100 bytes in size.
        // Pushing a new one to the database can cause it to be written
        // twice (once in the log, and once in the tables). This is already
        // an overestimation, as most will delete an existing entry or
        // overwrite one. Still, use a conservative safety factor of 2.
        if (!CheckDiskSpace(100 * 2 * 2 * pcoinsTip->GetCacheSize()))
            return state.Error("out of disk space");
        // First make sure all block and undo data is flushed to disk.
        FlushBlockFile();
        // Then update all block file information (which may refer to block and undo files).
        bool fileschanged = false;
        for (set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end(); ) {
            if (!pblocktree->WriteBlockFileInfo(*it, vinfoBlockFile[*it])) {
                return state.Abort("Failed to write to block index");
            }
            fileschanged = true;
            setDirtyFileInfo.erase(it++);
        }
        if (fileschanged && !pblocktree->WriteLastBlockFile(nLastBlockFile)) {
            return state.Abort("Failed to write to block index");
        }
        for (set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end(); ) {
             if (!pblocktree->WriteBlockIndex(CDiskBlockIndex(*it))) {
                 return state.Abort("Failed to write to block index");
             }
             setDirtyBlockIndex.erase(it++);
        }
        pblocktree->Sync();
        // Finally flush the chainstate (which may refer to block index entries).
        if (!pcoinsTip->Flush())
            return state.Abort("Failed to write to coin database");
        // Update best block in wallet (so we can detect restored wallets).
        if (mode != FLUSH_STATE_IF_NEEDED) {
            g_signals.SetBestChain(chainActive.GetLocator());
        }
        nLastWrite = GetTimeMicros();
    }
    } catch (const std::runtime_error& e) {
        return state.Abort(std::string("System error while flushing: ") + e.what());
    }
    return true;
}

void FlushStateToDisk() {
    CValidationState state;
    FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
}

/** Update chainActive and related internal data structures. */
void static UpdateTip(CBlockIndex *pindexNew) {
    chainActive.SetTip(pindexNew);

    // New best block
    nTimeBestReceived = GetTime();
    mempool.AddTransactionsUpdated(1);

    uint16_t nReportInterval = 0;
    if( isMainNetwork() ) {
        if( chainActive.Height() < Checkpoints::GetTotalBlocksEstimate() )
            nReportInterval = 1000;
        else if( chainActive.Height() < 350000 )
            nReportInterval = 100;
        else if( chainActive.Height() < 360000 )
            nReportInterval = 10;
    }
    // Limit the log output allot with this if your initializing the blockchain
    if( !nReportInterval || chainActive.Height() % nReportInterval == 0 )
        LogPrintf("UpdateTip: new best=%s  height=%d  log2_work=%.8g  tx=%lu  date=%s progress=%f  cache=%u\n",
          chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), GetLog2Work(chainActive.Tip()->nChainWork), (unsigned long)chainActive.Tip()->nChainTx,
          DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
          Checkpoints::GuessVerificationProgress(chainActive.Tip()), (unsigned int)pcoinsTip->GetCacheSize());

    cvBlockChange.notify_all();

    SetRetargetToBlock(pindexNew);

    // Check the version of the last 100 blocks to see if we need to upgrade:
    static bool fWarned = false;
    if (!IsInitialBlockDownload() && !fWarned)
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            LogPrintf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded, (int)CBlock::CURRENT_VERSION);
        if (nUpgraded > 100/2)
        {
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
            CAlert::Notify(strMiscWarning, true);
            fWarned = true;
        }
    }
}

/** Disconnect chainActive's tip. */
bool static DisconnectTip(CValidationState &state) {
    CBlockIndex *pindexDelete = chainActive.Tip();
    assert(pindexDelete);
    mempool.check(pcoinsTip);
    // Read block from disk.
    CBlock block;
    if (!ReadBlockFromDisk(block, pindexDelete))
        return state.Abort(_("Failed to read block"));
    // Apply the block atomically to the chain state.
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);                    // Create an empty coin cache view, based on the main pcoinsTip cache
        if (!DisconnectBlock(block, state, pindexDelete, view))
            return error("DisconnectTip() : DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        assert(view.Flush());
    }
    LogPrint("bench", "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
        return false;
    // Resurrect mempool transactions from the disconnected block.
    BOOST_FOREACH(const CTransaction &tx, block.vtx) {
        // ignore validation errors in resurrected transactions
        list<CTransaction> removed;
        CValidationState stateDummy;
        if (tx.IsCoinBase() || !AcceptToMemoryPool(mempool, stateDummy, tx, false, NULL))
            mempool.remove(tx, removed, true);
    }
    mempool.removeCoinbaseSpends(pcoinsTip, pindexDelete->nHeight);
    mempool.check(pcoinsTip);
    // Update chainActive and related variables.
    UpdateTip(pindexDelete->pprev);
    // Let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    BOOST_FOREACH(const CTransaction &tx, block.vtx) {
        SyncWithWallets(tx, NULL);
    }
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

/**
 * Connect a new block to chainActive. pblock is either NULL or a pointer to a CBlock
 * corresponding to pindexNew, to bypass loading it again from disk.
 */
bool static ConnectTip(CValidationState &state, CBlockIndex *pindexNew, CBlock *pblock) {
    assert(pindexNew->pprev == chainActive.Tip());
    mempool.check(pcoinsTip);
    // Read block from disk.
    int64_t nTime1 = GetTimeMicros();
    CBlock block;
    if (!pblock) {
        if (!ReadBlockFromDisk(block, pindexNew))
            return state.Abort("Failed to read block");
        pblock = &block;
    }
    // Apply the block atomically to the chain state.
    int64_t nTime2 = GetTimeMicros(); nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint("bench", "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
    {
        CCoinsViewCache view(pcoinsTip);                    // Create an empty coin cache view, based on the main pcoinsTip cache
        bool rv = ConnectBlock(*pblock, state, pindexNew, view);
        g_signals.BlockChecked(*pblock, state);                             // New signal used for the rpc miner
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("ConnectTip() : ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        mapBlockSource.erase(pindexNew->GetBlockHash());
        nTime3 = GetTimeMicros(); nTimeConnectTotal += nTime3 - nTime2;
        LogPrint("bench", "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        assert(view.Flush());
    }
    int64_t nTime4 = GetTimeMicros(); nTimeFlush += nTime4 - nTime3;
    LogPrint("bench", "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_IF_NEEDED))
        return false;
    int64_t nTime5 = GetTimeMicros(); nTimeChainState += nTime5 - nTime4;
    LogPrint("bench", "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);
    // Remove conflicting transactions from the mempool.
    list<CTransaction> txConflicted;
    mempool.removeForBlock(pblock->vtx, pindexNew->nHeight, txConflicted);
    mempool.check(pcoinsTip);
    // Update chainActive & related variables.
    UpdateTip(pindexNew);

    // Watch for changes to the previous coinbase transaction, first time here the static variable with be null.
    static uint256 hashPrevBestCoinBase;
    if( !hashPrevBestCoinBase.IsNull() ) {
        g_signals.UpdatedTransaction( hashPrevBestCoinBase );
    }
    hashPrevBestCoinBase = pblock->vtx[0].GetHash();

    // Tell wallet about transactions that went from mempool
    // to conflicted:
    BOOST_FOREACH(const CTransaction &tx, txConflicted) {
        SyncWithWallets(tx, NULL);
    }
    // ... and about transactions that got confirmed:
    BOOST_FOREACH(const CTransaction &tx, pblock->vtx) {
        SyncWithWallets(tx, pblock);
    }

    int64_t nTime6 = GetTimeMicros(); nTimePostConnect += nTime6 - nTime5; nTimeTotal += nTime6 - nTime1;
    LogPrint("bench", "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    LogPrint("bench", "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);
    return true;
}

/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain() {
    do {
        CBlockIndex *pindexNew = NULL;

        // Find the best candidate header.
        {
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
        }

        // Check whether all blocks on the path between the currently active chain and the candidate are valid.
        // Just going until the active chain is an optimization, as we know all blocks in it are valid already.
        CBlockIndex *pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nStatus & BLOCK_HAVE_DATA);
            assert(pindexTest->nChainTx || pindexTest->nHeight == 0);
            if (pindexTest->nStatus & BLOCK_FAILED_MASK) {
                // Candidate has an invalid ancestor, remove entire chain from the set.
                if (pindexBestInvalid == NULL || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
                    pindexBestInvalid = pindexNew;
                CBlockIndex *pindexFailed = pindexNew;
                while (pindexTest != pindexFailed) {
                    pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while(true);
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates() {
    // Note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.Tip())) {
        setBlockIndexCandidates.erase(it++);
    }
    // Either the current tip or a successor of it we're working towards is left in setBlockIndexCandidates.
    assert(!setBlockIndexCandidates.empty());
}

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(CValidationState &state, CBlockIndex *pindexMostWork, CBlock *pblock) {
    AssertLockHeld(cs_main);
    bool fInvalidFound = false;
    const CBlockIndex *pindexOldTip = chainActive.Tip();
    const CBlockIndex *pindexFork = chainActive.FindFork(pindexMostWork);

    // Disconnect active blocks which are no longer in the best chain.
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state))
            return false;
    }

    // Build list of new blocks to connect.
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
    // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
    // a few blocks along the way.
    int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
    vpindexToConnect.clear();
    vpindexToConnect.reserve(nTargetHeight - nHeight);
    CBlockIndex *pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
    while (pindexIter && pindexIter->nHeight != nHeight) {
        vpindexToConnect.push_back(pindexIter);
        pindexIter = pindexIter->pprev;
    }
    nHeight = nTargetHeight;

    // Connect new blocks.
    BOOST_REVERSE_FOREACH(CBlockIndex *pindexConnect, vpindexToConnect) {
        if (!ConnectTip(state, pindexConnect, pindexConnect == pindexMostWork ? pblock : NULL)) {
            if (state.IsInvalid()) {
                // The block violates a consensus rule.
                if (!state.CorruptionPossible())
                    InvalidChainFound(vpindexToConnect.back());
                state = CValidationState();
                fInvalidFound = true;
                fContinue = false;
                break;
            } else {
                // A system error occurred (disk space, database error, ...).
                return false;
            }
        } else {
            PruneBlockIndexCandidates();
            if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {
                // We're in a better position than we were. Return temporarily to release the lock.
                fContinue = false;
                break;
            }
        }
    }
    }

    // Callbacks/notifications for a new best chain.
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(CValidationState &state, CBlock *pblock) {
    CBlockIndex *pindexNewTip = NULL;
    CBlockIndex *pindexMostWork = NULL;
    do {
        boost::this_thread::interruption_point();

        bool fInitialDownload;
        {
            LOCK(cs_main);
            pindexMostWork = FindMostWorkChain();

            // Whether we have anything to do at all.
            if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                return true;

            if (!ActivateBestChainStep(state, pindexMostWork, pblock && pblock->GetHash() == pindexMostWork->GetBlockHash() ? pblock : NULL))
                return false;

            pindexNewTip = chainActive.Tip();
            fInitialDownload = IsInitialBlockDownload();
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).

        // Notifications/callbacks that can run without cs_main
        if (!fInitialDownload) {
            uintFakeHash hashNewTip = pindexNewTip->GetBlockSha256dHash();
            // Relay inventory, but don't relay old inventory during initial block download.
            int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
            {
                LOCK(cs_vNodes);
                BOOST_FOREACH(CNode* pnode, vNodes)
                    if (chainActive.Height() > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                        pnode->PushInventory(CInv(MSG_BLOCK, hashNewTip));
            }
            // Notify external listeners about the new tip, and in this case use the real hash
            uiInterface.NotifyBlockTip(pindexNewTip->GetBlockHash());
        }
    } while(pindexMostWork != chainActive.Tip());
    CheckBlockIndex();

    // Write changes periodically to disk, after relay.
    if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}

bool InvalidateBlock(CValidationState& state, CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    while (chainActive.Contains(pindex)) {
        CBlockIndex *pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!DisconnectTip(state)) {
            return false;
        }
    }

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add them again.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);
    return true;
}

bool ReconsiderBlock(CValidationState& state, CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

static CBlockIndex* AddToBlockIndex(const CBlockHeader& aHeader)
{
    //! Check for duplicate
    uint256 uintRealHash = aHeader.GetHash();
    BlockMap::iterator it = mapBlockIndex.find( uintRealHash );
    if (it != mapBlockIndex.end())                  //! Bingo, got it already
        return it->second;

    //! Construct new block index object, filling in the initial basic header values
    CBlockIndex* pindexNew = new CBlockIndex(aHeader);
    assert(pindexNew);
    //! Time complexity is much higher now, it really shows up in reindexing...ToDo:
    pindexNew->fakeBIhash = aHeader.CalcSha256dHash();      //! Now store it in the new index object
    pindexNew->fakeBIhash.SetRealHash( uintRealHash );      //! Make sure we keep our cross reference lookup map full and fast

    //! We assign the sequence id to blocks only when the full data is available,
    //! to avoid miners withholding blocks but broadcasting headers, to get a
    //! competitive advantage.
    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.insert(make_pair(uintRealHash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);
    if( aHeader.hashPrevBlock != 0 ) {
        //! We can do this now because the call to new CBlockIndex calculated both hashes and updated
        //! the cross reference map for the sha256d hash.
        uint256 aRealHash = aHeader.hashPrevBlock.GetRealHash();
        BlockMap::iterator miPrev = ( aRealHash != 0 ) ? mapBlockIndex.find( aRealHash ) : mapBlockIndex.end();
        //! ToDo: This means something is wrong, add more armor...
        assert( miPrev != mapBlockIndex.end() );
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        pindexNew->BuildSkip();
    } else
        assert( uintRealHash == Params().HashGenesisBlock() );
    // ToDo: Above, instead of crashing due to assertion failure, if the previous block hash is zero..or the realhash isn't found,
    // log them or something better, perhaps log a reindex blockchain request & start shutdown?
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == NULL || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);               //! Signal this needs to get stored to disk later

    return pindexNew;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS). */
bool ReceivedBlockTransactions(const CBlock &block, CValidationState& state, CBlockIndex *pindexNew, const CDiskBlockPos& pos)
{
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {
        // If pindexNew is the genesis block or all parents are BLOCK_VALID_TRANSACTIONS.
        deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        // Recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            CBlockIndex *pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                 LOCK(cs_nBlockSequenceId);
                 pindex->nSequenceId = nBlockSequenceId++;
            }
            if (chainActive.Tip() == NULL || !setBlockIndexCandidates.value_comp()(pindex, chainActive.Tip())) {
                setBlockIndexCandidates.insert(pindex);
            }
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(std::make_pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

bool FindBlockPos(CValidationState &state, CDiskBlockPos &pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
{
    LOCK(cs_LastBlockFile);

    unsigned int nFile = fKnown ? pos.nFile : nLastBlockFile;
    if (vinfoBlockFile.size() <= nFile) {
        vinfoBlockFile.resize(nFile + 1);
    }

    if (!fKnown) {
        while (vinfoBlockFile[nFile].nSize + nAddSize >= MAX_BLOCKFILE_SIZE) {
            LogPrintf("Leaving block file %i: %s\n", nFile, vinfoBlockFile[nFile].ToString());
            FlushBlockFile(true);
            nFile++;
            if (vinfoBlockFile.size() <= nFile) {
                vinfoBlockFile.resize(nFile + 1);
            }
        }
        pos.nFile = nFile;
        pos.nPos = vinfoBlockFile[nFile].nSize;
    }

    nLastBlockFile = nFile;
    vinfoBlockFile[nFile].AddBlock(nHeight, nTime);
    if (fKnown)
        vinfoBlockFile[nFile].nSize = std::max(pos.nPos + nAddSize, vinfoBlockFile[nFile].nSize);
    else
        vinfoBlockFile[nFile].nSize += nAddSize;

    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = (vinfoBlockFile[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos)) {
                FILE *file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            }
            else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

bool FindUndoPos(CValidationState &state, int nFile, CDiskBlockPos &pos, unsigned int nAddSize)
{
    pos.nFile = nFile;

    LOCK(cs_LastBlockFile);

    unsigned int nNewSize;
    pos.nPos = vinfoBlockFile[nFile].nUndoSize;
    nNewSize = vinfoBlockFile[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos)) {
            FILE *file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        }
        else
            return state.Error("out of disk space");
    }

    return true;
}

bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, bool fCheckPOW)
{
    // Check proof of work matches claimed amount
    if (fCheckPOW && !CheckProofOfWork(block.GetHash(), block.nBits))
        return state.DoS(50, error("CheckBlockHeader() : proof of work failed"),
                         REJECT_INVALID, "high-hash");

    // Check timestamp, for Anoncoin we make that less than 2hrs after the hardfork,
    // no mined block time need have a future time so large.  In fact the header can
    // not be used unless this value is reduced to mere seconds.
    int64_t nTimeLimit = GetAdjustedTime();
#if defined( HARDFORK_BLOCK )
    CBlockIndex *tip = chainActive.Tip();
    if(tip) nTimeLimit += ( tip->nHeight < HARDFORK_BLOCK ) ? 2 * 60 * 60 : 15 * 60;
#else
    nTimeLimit += 2 * 60 * 60;
#endif
    if( block.GetBlockTime() > nTimeLimit )
        return state.Invalid(error("CheckBlockHeader(): block timestamp too far in the future"),
                             REJECT_INVALID, "time-too-new");
    return true;
}

bool CheckBlock(const CBlock& block, CValidationState& state, bool fCheckPOW, bool fCheckMerkleRoot)
{
    // These are checks that are independent of context.

    // Check that the header is valid (particularly PoW).  This is mostly
    // redundant with the call in AcceptBlockHeader.
    if (!CheckBlockHeader(block, state, fCheckPOW))
        return false;

    // Check the merkle root.
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = block.BuildMerkleTree(&mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"),
                             REJECT_INVALID, "bad-txnmrklroot", true);

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.DoS(100, error("CheckBlock() : duplicate transaction"),
                             REJECT_INVALID, "bad-txns-duplicate", true);
    }

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.

    // Size limits
    if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CheckBlock() : size limits failed"),
                         REJECT_INVALID, "bad-blk-length");

    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0].IsCoinBase())
        return state.DoS(100, error("CheckBlock() : first tx is not coinbase"),
                         REJECT_INVALID, "bad-cb-missing");
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].IsCoinBase())
            return state.DoS(100, error("CheckBlock() : more than one coinbase"),
                             REJECT_INVALID, "bad-cb-multiple");

    // Check transactions
    BOOST_FOREACH(const CTransaction& tx, block.vtx)
        if (!CheckTransaction(tx, state))
            return error("CheckBlock() : CheckTransaction failed");

    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTransaction& tx, block.vtx)
    {
        nSigOps += GetLegacySigOpCount(tx);
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return state.DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"),
                         REJECT_INVALID, "bad-blk-sigops", true);

    return true;
}

bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const CBlockIndex* pindexPrev)
{
    uint256 hash = block.GetHash();
    if (hash == Params().HashGenesisBlock())
        return true;

    assert(pindexPrev);

    int nHeight = pindexPrev->nHeight+1;

    // Check proof of work
    if (block.nBits != GetNextWorkRequired(pindexPrev, &block) ) {
        if (!TestNet() || pindexPrev->nHeight > pRetargetPid->GetTipFilterBlocks() )
            return state.Invalid(error("%s : incorrect proof of work", __func__), REJECT_INVALID, "bad-diffbits");
    }        

    // Check timestamp against prev
    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast())
        return state.Invalid(error("%s : block's timestamp is too early", __func__),
                             REJECT_INVALID, "time-too-old");

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckBlock(nHeight, block.CalcSha256dHash()))
        return state.DoS(100, error("%s : rejected by checkpoint lock-in at %d", __func__, nHeight),
                         REJECT_CHECKPOINT, "checkpoint mismatch");

    // Don't accept any forks from the main chain prior to last checkpoint
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint();
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(100, error("%s : forked chain older than last checkpoint (height %d)", __func__, nHeight));
#if defined( HARDFORK_BLOCK )
    // Reject block.nVersion=1 blocks
    // This code has been taken from the v0.8.5.5 source, and does not work, because the IsSuperMajority test was
    // set to always return false.  As of 3/15/2015 in the last 10000 blocks over 700 where version 1 blocks
    if( block.nVersion < 2 && nHeight > HARDFORK_BLOCK )                // We'll start enforcing the new rule
        return state.Invalid(error("%s : rejected nVersion=1 block", __func__), REJECT_OBSOLETE, "bad-version");
#endif
#if defined( DONT_COMPILE )
    // Reject block.nVersion=2 blocks when 95% (75% on testnet) of the network has upgraded:
    if (block.nVersion < 3 && CBlockIndex::IsSuperMajority(3, pindexPrev, Params().RejectBlockOutdatedMajority()))
    {
        return state.Invalid(error("%s : rejected nVersion=2 block", __func__),
                             REJECT_OBSOLETE, "bad-version");
    }
#endif

    return true;
}

bool ContextualCheckBlock(const CBlock& block, CValidationState& state, const CBlockIndex* pindexPrev)
{
    const int32_t nHeight = pindexPrev == NULL ? 0 : pindexPrev->nHeight + 1;

    // Check that all transactions are finalized
    BOOST_FOREACH(const CTransaction& tx, block.vtx)
        if (!IsFinalTx(tx, nHeight, block.GetBlockTime())) {
            return state.DoS(10, error("%s : contains a non-final transaction", __func__), REJECT_INVALID, "bad-txns-nonfinal");
        }

    // Enforce block.nVersion=2 rule that the coinbase starts with serialized block height, there was one block
    // way back at block index 222353, which doesn't pass this smell test, other than that the chain appears clean.
    if( block.nVersion >= 2 && nHeight != 222353 ) {                    // Enforce the rule if true now
        CScript expect = CScript() << nHeight;
        if (block.vtx[0].vin[0].scriptSig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin())) {
            return state.DoS(100, error("%s : block height mismatch in coinbase", __func__), REJECT_INVALID, "bad-cb-height");
        }
    }

    return true;
}

bool AcceptBlockHeader(const CBlockHeader& block, CValidationState& state, CBlockIndex** ppindex)
{
    AssertLockHeld(cs_main);
    // Check for duplicate
    uint256 realHash = block.GetHash();
    BlockMap::iterator miSelf = mapBlockIndex.find(realHash);
    CBlockIndex *pindex = NULL;
    if (miSelf != mapBlockIndex.end()) {
        // Block header is already known.
        pindex = miSelf->second;
        if (ppindex)
            *ppindex = pindex;
        if (pindex->nStatus & BLOCK_FAILED_MASK)
            return state.Invalid(error("%s : block is marked invalid", __func__), 0, "duplicate");
        return true;
    }

    if (!CheckBlockHeader(block, state))
        return false;

    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    if (realHash != Params().HashGenesisBlock()) {
        uint256 realPrevHash = block.hashPrevBlock.GetRealHash();
        BlockMap::iterator mi = ( realPrevHash != 0 ) ? mapBlockIndex.find( realPrevHash ) : mapBlockIndex.end();
        if (mi == mapBlockIndex.end())
            return state.Invalid(error("%s : prev block not found", __func__), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
            return state.DoS(100, error("%s : prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
    }

    if (!ContextualCheckBlockHeader(block, state, pindexPrev))
        return false;

    if (pindex == NULL)
        pindex = AddToBlockIndex(block);

    if (ppindex)
        *ppindex = pindex;

    return true;
}

bool AcceptBlock(CBlock& block, CValidationState& state, CBlockIndex** ppindex, CDiskBlockPos* dbp)
{
    AssertLockHeld(cs_main);

    CBlockIndex *&pindex = *ppindex;

    if (!AcceptBlockHeader(block, state, &pindex))
        return false;

    if (pindex->nStatus & BLOCK_HAVE_DATA) {
        // TODO: deal better with duplicate blocks.
        // return state.DoS(20, error("AcceptBlock() : already have block %d %s", pindex->nHeight, pindex->GetBlockHash().ToString()), REJECT_DUPLICATE, "duplicate");
        return true;
    }

    if ((!CheckBlock(block, state)) || !ContextualCheckBlock(block, state, pindex->pprev)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return false;
    }

    int nHeight = pindex->nHeight;

    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(state, blockPos, nBlockSize+8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("AcceptBlock() : FindBlockPos failed");
        if (dbp == NULL) {
            // LogPrintf( "Writing Block %d with hash: %s to position %d\n", nHeight, (block.GetHash()).ToString(), blockPos.nPos);
            if (!WriteBlockToDisk(block, blockPos))
                return state.Abort("Failed to write block");
        }
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("AcceptBlock() : ReceivedBlockTransactions failed");
    } catch(const std::runtime_error& e) {
        return state.Abort(std::string("System error: ") + e.what());
    }

    return true;
}

#if defined( DONT_COMPILE )
bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired, unsigned int nToCheck)
{
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}
#endif // defined

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockIndex::pskip pointer. */
int static inline GetSkipHeight(int height) {
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    if (height > nHeight || height < 0)
        return NULL;

    CBlockIndex* pindexWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (heightSkip == height ||
            (heightSkip > height && !(heightSkipPrev < heightSkip - 2 &&
                                      heightSkipPrev >= height))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    return const_cast<CBlockIndex*>(this)->GetAncestor(height);
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

bool ProcessNewBlock(CValidationState &state, CNode* pfrom, CBlock* pblock, CDiskBlockPos *dbp)
{
    // Preliminary checks
    bool checked = CheckBlock(*pblock, state);

    {
        LOCK(cs_main);
        MarkBlockAsReceived(pblock->CalcSha256dHash());
        if (!checked) {
            return error("%s : CheckBlock FAILED", __func__);
        }

        // Store to disk
        CBlockIndex *pindex = NULL;
        bool ret = AcceptBlock(*pblock, state, &pindex, dbp);
        if (pindex && pfrom) {
            mapBlockSource[pindex->GetBlockHash()] = pfrom->GetId();
        }
        CheckBlockIndex();
        if (!ret)
            return error("%s : AcceptBlock FAILED", __func__);
    }

    if (!ActivateBestChain(state, pblock))
        return error("%s : ActivateBestChain failed", __func__);

    return true;
}

bool TestBlockValidity(CValidationState &state, const CBlock& block, const CBlockIndex* pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev == chainActive.Tip());

    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex indexDummy(block);
    indexDummy.pprev = (CBlockIndex*)pindexPrev;    // The constantness of pindexPrev must be overridden here, the dummy will not modify what it points to
    indexDummy.nHeight = pindexPrev->nHeight + 1;

    // NOTE: CheckBlockHeader is called by CheckBlock
    if (!ContextualCheckBlockHeader(block, state, pindexPrev))
        return false;
    if (!CheckBlock(block, state, fCheckPOW, fCheckMerkleRoot))
        return false;
    if (!ContextualCheckBlock(block, state, pindexPrev))
        return false;
    if (!ConnectBlock(block, state, &indexDummy, viewNew, true))
        return false;
    assert(state.IsValid());

    return true;
}



bool AbortNode(const std::string &strMessage) {
    strMiscWarning = strMessage;
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(strMessage, "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = boost::filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode(_("Error: Disk space is low!"));

    return true;
}

FILE* OpenDiskFile(const CDiskBlockPos &pos, const char *prefix, bool fReadOnly)
{
    if (pos.IsNull())
        return NULL;
    boost::filesystem::path path = GetBlockPosFilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    FILE* file = fopen(path.string().c_str(), "rb+");
    if (!file && !fReadOnly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return NULL;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "blk", fReadOnly);
}

FILE* OpenUndoFile(const CDiskBlockPos &pos, bool fReadOnly) {
    return OpenDiskFile(pos, "rev", fReadOnly);
}

boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix)
{
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}
#if defined( DONT_COMPILE )
// Don't use this routine for Anoncoin
CBlockIndex * InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}
#endif
bool static LoadBlockIndexDB()
{
    //! Load the blockindex guts & build a vector of blockindex pointers sorted by height...
    vector<BlockTreeEntry> vSortedByHeight;
    if( !pblocktree->LoadBlockIndexGuts( vSortedByHeight ) )
        return false;

    //! If there are no blocks to sort, that is ok, the genesis block has not even been created
    //! and setup yet on disk...we're done
    if( vSortedByHeight.size() == 0 )
        return true;

    if(ShutdownRequested()) {                 //! Watch out for and respond to any shutdown signal
        vSortedByHeight.clear();
        return false;
    }
    boost::this_thread::interruption_point();               //! If there is other stuff to do, now would be a good time
    SetThreadPriority(THREAD_PRIORITY_ABOVE_NORMAL);        //! Move to warp speed if the OS will let us, try to up the thread priority

    sort(vSortedByHeight.begin(), vSortedByHeight.end());   //! U gotta love the old sort routine, still works great!

    uint32_t nBIsize = vSortedByHeight.size();
    LogPrintf( "%s : Sorted %d blockindex entries by height.\n", __func__, nBIsize );

    //! Now with minimal time complexity we can finally build the cross reference index,
    //! initialize the mapBlockIndex and CBlockIndex structure pointers and values.
    //! We already have the previous block sha256d hashes stored in childern object(s) temporarily
    //! First we must calculate the sha256d hash and scrypt hash of every block to build our
    //! crossreference map, then we can lookup the fake sha256d hashes for every block, to set
    //! its previous block pointer to correctly, in the 2nd pass
    uint32_t nHeight = 0;
    CBlockHeader aHeader;                                           //! Setup a temp header here to work in the loop with
    //! Best to dynamically allocate some temporary arrays (vectors) to finish things up quick on the 2nd pass...
    vector<uintFakeHash> vFakeHashes;
    vFakeHashes.reserve( nBIsize );
    mapBlockHashCrossReference.reserve( nBIsize );                  //! Pre-allocate the number of entries
    bool fDoubleCheckingHash = true;
    //! Better tell the user, this takes awhile
    uint64_t nStartTime = GetTime() - 16;
    uint8_t msgcnt = 0;
    BOOST_FOREACH(const BlockTreeEntry& entry, vSortedByHeight) {
        CBlockIndex* pindex = entry.pBlockIndex;
        //! ONLY the Genesis block should not have a previous hash
        assert( pindex->fakeBIhash != 0 || pindex->nHeight == 0 );
        aHeader.nVersion        = pindex->nVersion;
        aHeader.hashPrevBlock   = pindex->fakeBIhash; //! Temporarily stored the prev block sha256d hash here
        aHeader.hashMerkleRoot  = pindex->hashMerkleRoot;
        aHeader.nTime           = pindex->nTime;
        aHeader.nBits           = pindex->nBits;
        aHeader.nNonce          = pindex->nNonce;
        //! Calling GetHash & CalcSha256dHash with true, invalidates any previously calculated hashes for this block, as they have changed
        uintFakeHash aFakeHash  = aHeader.CalcSha256dHash(true);    //! Calculate the sha256d hash, even for the genesis block
        uint256 aRealHash;
        if( fDoubleCheckingHash ) {
            aRealHash = aHeader.GetHash(true);              //! Calc the real scrypt hash of this block.
            if( nHeight > 100 )                             //! Stop checking if things have been ok after the 1st 100 blocks
                fDoubleCheckingHash = false;
        } else if( nHeight > nBIsize - 1000 ) {             //! Turn it back on for the last 1000 blocks
            aRealHash = aHeader.GetHash(true);              //! Calc the real scrypt hash of this block.
        } else
            aRealHash = entry.uintRealHash;

        if( aRealHash != entry.uintRealHash ) {
            LogPrintf( "%s : ERROR - at Block %d, the Real Hash is not the same as being reported by the BlockTreeDB key, recommend a reindex.\n", __func__, nHeight );
            StartShutdown();
        }

        //! Could do a quick check of the nBits to confirm pow here...its fast.
        if( !CheckProofOfWork( aRealHash, aHeader.nBits ) )
            return error("%s : CheckProofOfWork failed: %s", __func__, pindex->ToString());
        // LogPrintf( "fakeBIhash: %s aRealHash: %s Height=%d\n", aFakeHash.ToString(), aRealHash.ToString(), nHeight );
        vFakeHashes[nHeight++] = aFakeHash; //! Save it for later on the 2nd pass
        aFakeHash.SetRealHash( aRealHash ); //! Update our cross reference unordered fast hash lookup map
//      if( GetTime() - nStartTime  > 15 ) {
        if( GetTime() - nStartTime > 1 ) {
            switch( msgcnt++ ) {
                case 0 : uiInterface.InitMessage(_("Building cross reference...")); break;
                case 1 : uiInterface.InitMessage(_("Checking proof-of-work too...")); msgcnt = 0; break;
/*                case 1 : uiInterface.InitMessage(_("Please be patient...")); break;
                case 2 : uiInterface.InitMessage(_("Checking proof-of-work too...")); break;
                case 3 : uiInterface.InitMessage(_("Anoncoin for the 1st time...")); break;
                case 4 : uiInterface.InitMessage(_("Will use real block hashes...")); break;
                case 5 : uiInterface.InitMessage(_("Each block has 2 identities...")); break;
                case 6 : uiInterface.InitMessage(_("A mined hash & sha256d hash...")); msgcnt = 0; break; */
            }
            nStartTime = GetTime();
            boost::this_thread::interruption_point();//! If there is other stuff to do, now would be a good time
       }
        if(ShutdownRequested()) {                   //! Watch out for and respond to any shutdown signal
            vSortedByHeight.clear();                //! Although not really needed, cleaning up the vectors in use
                                                    //! is good programming practice & helps to understand what
            vFakeHashes.clear();                    //! variables the code has been working on.
            //! Don't really need to worry about the thread priority, as this is all about to be over anyway
            return false;
        }
    }
    LogPrintf( "%s : Cross referenced %s block sha256d hashes, using real proof-of-work for the index.\n", __func__, mapBlockHashCrossReference.size() );
    uiInterface.InitMessage(_("Finishing block index setup..."));

    //! Now that is finally done, we can build the main softwares mapBlockIndex and fix the BlockIndex
    //! Structures variables and pointers.
    nHeight = 0;
    assert( mapBlockIndex.size() == 0 );
    mapBlockIndex.reserve( nBIsize );                               //! Pre-allocate the number of entries
    BOOST_FOREACH(const BlockTreeEntry& entry, vSortedByHeight) {
        CBlockIndex* pindex = entry.pBlockIndex;
        BlockMap::iterator mi = mapBlockIndex.insert(make_pair(entry.uintRealHash, pindex)).first;
        pindex->phashBlock = &((*mi).first);                        //! Keeps a pointer to the hash in the BI
        //! Now find the real hash of this blocks previous block, and set the pointer up correctly.
        //! It should already be in the mapBlockIndex
        if( pindex->fakeBIhash != 0 ) {      //! Can't do that for the genesis though
            uint256 aRealHash = pindex->fakeBIhash.GetRealHash();
            BlockMap::iterator mi2 = ( aRealHash != 0 ) ? mapBlockIndex.find( aRealHash ) : mapBlockIndex.end();
            // LogPrintf( "fakeBIhash: %s aRealHash: %s  mi2 at end? %s Height=%d\n", pindex->fakeBIhash.ToString(), aRealHash.ToString(), (mi2 == mapBlockIndex.end()) ? "yes" : "no", pindex->nHeight );
            assert(mi2 != mapBlockIndex.end());
            pindex->pprev = (*mi2).second;
        }
        //! Finally we can wipe out the previous blocks fake hash that was stored here temporarily
        //! and now set it correct for what this blocks sha256d hash is, which we remembered from
        //! the 1st pass...and was the original design intention of the field.
        pindex->fakeBIhash = vFakeHashes[nHeight++];
    }
    vFakeHashes.clear();
    SetThreadPriority(THREAD_PRIORITY_NORMAL);      //! Return to normal processing priority, the hard work has been finished
    LogPrintf( "%s : Completed building the BlockIndex map with %d real proof-of-work hashes.\n", __func__, mapBlockIndex.size() );

    //! Another day, another pass...returning to the standard coding...
    //! Calculate nChainWork
    nHeight = 0;
    BOOST_FOREACH(const BlockTreeEntry& entry, vSortedByHeight)
    {
        CBlockIndex* pindex = entry.pBlockIndex;
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);
        if( (nHeight < 25000 && nHeight % 5000 == 0 ) || nHeight % 25000 == 0 )
            LogPrintf( "%s : Block @ Height=%6d, ChainWork=%s\n", __func__, entry.nHeight, pindex->nChainWork.ToString() );
        nHeight++;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }
    vSortedByHeight.clear();
    LogPrintf( "%s : setBlockIndexCandidates size=%d\n", __func__, setBlockIndexCandidates.size() );
    LogPrintf( "%s : mapBlocksUnlinked       size=%d\n", __func__, mapBlocksUnlinked.size() );
    LogPrintf( "%s : pindexBestHeader points to block=%d\n", __func__, pindexBestHeader ? pindexBestHeader->nHeight : 0 );
    LogPrintf( "%s : pindexBestInvalid points to block=%d\n", __func__, pindexBestInvalid ? pindexBestInvalid->nHeight : 0 );

    //LogPrintf( "LoadBlockIndex() : Following the last skip map Ancestor chain...\n" );
    //while( pindex->pskip ) {
    //    LogPrintf( "LoadBlockIndex() : pindexBestInvalid points to block=%d\n", pindexBestInvalid ? pindexBestInvalid->nHeight : 0 );
    //}

    // Load block file info
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    LogPrintf("%s : last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s : last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

    // Check presence of blk files
    LogPrintf("%s : Checking all blk files are present...\n", __func__ );
    set<int> setBlkDataFiles;
    BOOST_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++)
    {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    // Check whether we need to continue reindexing
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    fReindex |= fReindexing;

    // Check whether we have a transaction index
    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("%s : transaction index %s\n", __func__, fTxIndex ? "enabled" : "disabled");

    // Load pointer to end of best chain
    uint256 viewBestBlock = pcoinsTip->GetBestBlock();
    BlockMap::iterator itBM = (viewBestBlock != 0) ? mapBlockIndex.find( viewBestBlock ) : mapBlockIndex.end();
    if( itBM == mapBlockIndex.end() ) {
        LogPrintf( "%s : WARNING - Coins BestBlock hash: %s not found in BlockIndex. Failed initializing...\n", __func__, viewBestBlock.ToString() );
        return true;
    }
    chainActive.SetTip(itBM->second);
    SetRetargetToBlock(itBM->second);

    PruneBlockIndexCandidates();

    LogPrintf("%s : hashBestChain=%s height=%d date=%s progress=%f\n", __func__,
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        Checkpoints::GuessVerificationProgress(chainActive.Tip()));

    return true;
}

bool VerifyDB(int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

    // Verify blocks in the best chain
    if (nCheckDepth <= 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = std::max(0, std::min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(pcoinsTip);
    CBlockIndex* pindexState = chainActive.Tip();
    CBlockIndex* pindexFailure = NULL;
    int nGoodTransactions = 0;
    CValidationState state;
    int nHeightOfLastV1block = 0;
    int nCountOfLastV1block = 0;
    // int nHeightOfLastV2FailedRuleBlock = 0;
    // int nCountOfLastV2FailedRuleBlock = 0;
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev)
    {
        boost::this_thread::interruption_point();
        if (pindex->nHeight < chainActive.Height()-nCheckDepth)
            break;
        CBlock block;
        // check level 0: read from disk
        if (!ReadBlockFromDisk(block, pindex))
            return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());

        if( block.nVersion < 2 ) {
            nCountOfLastV1block++;
            if( nHeightOfLastV1block < pindex->nHeight ) nHeightOfLastV1block = pindex->nHeight;
        } // else {  Think we're done with this test code, ToDo: remove from source master
            // Check for failed V2 coinbase rule
            // V2 blocks should ALL start with a coinbase that has a serialized block height
            //CScript expect = CScript() << pindex->nHeight;
            //if( block.vtx[0].vin[0].scriptSig.size() < expect.size() || !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin()) ) {
                //if( nHeightOfLastV2FailedRuleBlock < pindex->nHeight ) nHeightOfLastV2FailedRuleBlock = pindex->nHeight;
                //nCountOfLastV2FailedRuleBlock++;
            //}
        //}

        // check level 1: verify block validity
        if (nCheckLevel >= 1 && !CheckBlock(block, state))
            return error("VerifyDB() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
        // check level 2: verify undo validity
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!undo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
                    return error("VerifyDB() : *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nCheckLevel >= 3 && pindex == pindexState && (coins.GetCacheSize() + pcoinsTip->GetCacheSize()) <= 2*nCoinCacheSize + 32000) {
            bool fClean = true;
            if (!DisconnectBlock(block, state, pindex, coins, &fClean))
                return error("VerifyDB() : *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            pindexState = pindex->pprev;
            if (!fClean) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else
                nGoodTransactions += block.vtx.size();
        }
        if (ShutdownRequested())
            return true;
  }
    LogPrintf( "The height of the last V1 block was %d, and there were %d of them.\n", nHeightOfLastV1block, nCountOfLastV1block );
    // LogPrintf( "The height of the last V2 block that has a mismatch in the coinbase height was %d, and there were %d of them.\n", nHeightOfLastV2FailedRuleBlock, nCountOfLastV2FailedRuleBlock );

    if (pindexFailure)
        return error("VerifyDB() : *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

    // check level 4: try reconnecting blocks
    if (nCheckLevel >= 4) {
        CBlockIndex *pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex))
                return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            if (!ConnectBlock(block, state, pindex, coins))
                return error("VerifyDB() : *** found unconnectable block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        }
    }

    LogPrintf("No coin database inconsistencies in last %i blocks (%i transactions)\n", chainActive.Height() - pindexState->nHeight, nGoodTransactions);

    return true;
}

void UnloadBlockIndex()
{
    setBlockIndexCandidates.clear();
    mapBlockHashCrossReference.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
    pindexBestHeader = NULL;
    mempool.clear();
    mapOrphanTransactions.clear();
    mapOrphanTransactionsByPrev.clear();
    nSyncStarted = 0;
    mapBlocksUnlinked.clear();
    vinfoBlockFile.clear();
    nLastBlockFile = 0;
    nBlockSequenceId = 1;
    mapBlockSource.clear();
    mapBlocksInFlight.clear();
    nQueuedValidatedHeaders = 0;
    nPreferredDownload = 0;
    setDirtyBlockIndex.clear();
    setDirtyFileInfo.clear();
    mapNodeState.clear();

    BOOST_FOREACH(BlockMap::value_type& entry, mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
    // cross reference block hash map
    mapBlockHashCrossReference.clear();
 }

bool LoadBlockIndex()
{
    // Load block index from databases
    if (!fReindex && !LoadBlockIndexDB())
        return false;
    return true;
}

bool InitBlockIndex() {
    LOCK(cs_main);
    // Check whether we're already initialized
    if (chainActive.Genesis() != NULL)
        return true;

    // Use the provided setting for -txindex in the new database
    fTxIndex = GetBoolArg("-txindex", false);
    pblocktree->WriteFlag("txindex", fTxIndex);
    LogPrintf("Initializing databases...\n");

    // Only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!fReindex) {
        try {
            CBlock &block = const_cast<CBlock&>(Params().GenesisBlock());
            // Start new block file
            unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
            CDiskBlockPos blockPos;
            CValidationState state;
            if (!FindBlockPos(state, blockPos, nBlockSize+8, 0, block.GetBlockTime()))
                return error("LoadBlockIndex() : FindBlockPos failed");
            if (!WriteBlockToDisk(block, blockPos))
                return error("LoadBlockIndex() : writing genesis block to disk failed");
            CBlockIndex *pindex = AddToBlockIndex(block);
            if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
                return error("LoadBlockIndex() : genesis block not accepted");
            if (!ActivateBestChain(state, &block))
                return error("LoadBlockIndex() : genesis block cannot be activated");
            if( TestNet() && GetBoolArg( "-testnetstart", false ) ) {
                //! Starts the process of using the RPC to run the initial block generation commands
                //! once initialization has completed and the RPC warm up period has ended.
                fGenerateInitialTestNetState = true;
                LogPrintf( "Preparing to generate TestNet initial condition blocks...\n" );
            }
            // Force a chainstate write so that when we VerifyDB in a moment, it doesnt check stale data
            return FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
        } catch(const std::runtime_error& e) {
            return error("LoadBlockIndex() : failed to initialize block database: %s", e.what());
        }
    }

    return true;
}



bool LoadExternalBlockFile(FILE* fileIn, CDiskBlockPos *dbp)
{
    // Map of disk positions for blocks with unknown parent (only used for reindex)
    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {
        // This takes over fileIn and calls fclose() on it in the CBufferedFile destructor
        CBufferedFile blkdat(fileIn, 2*MAX_BLOCK_SIZE, MAX_BLOCK_SIZE+8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++; // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {
                // locate a header
                uint8_t buf[MESSAGE_START_SIZE];
                blkdat.FindByte(Params().MessageStart()[0]);
                nRewind = blkdat.GetPos()+1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, Params().MessageStart(), MESSAGE_START_SIZE))
                    continue;
                // read size
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SIZE)
                    continue;
            } catch (const std::exception&) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                CBlock block;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                // detect out of order blocks, and store them for later
                uint256 newRealHash = block.GetHash();
                uint256 prevRealHash = block.hashPrevBlock.GetRealHash();
                if (newRealHash != Params().HashGenesisBlock() && ( prevRealHash == 0 || mapBlockIndex.find(prevRealHash) == mapBlockIndex.end())) {
                    LogPrint("reindex", "%s : Out of order block %s, parent %s not known\n", __func__, newRealHash.ToString(),
                            prevRealHash.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(std::make_pair(prevRealHash, *dbp));
                    continue;
                }

                // process in case the block isn't known yet
                if (mapBlockIndex.count(newRealHash) == 0 || (mapBlockIndex[newRealHash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    CValidationState state;
                    if (ProcessNewBlock(state, NULL, &block, dbp))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (newRealHash != Params().HashGenesisBlock() && mapBlockIndex[newRealHash]->nHeight % 1000 == 0) {
                    LogPrintf("Block Import: already had block %s at height %d\n", newRealHash.ToString(), mapBlockIndex[newRealHash]->nHeight);
                }

                // Recursively process earlier encountered successors of this block
                deque<uint256> queue;
                queue.push_back(newRealHash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;
                        if (ReadBlockFromDisk(block, it->second))
                        {
                            LogPrintf("%s : Processing out of order child %s of %s\n", __func__, block.GetHash().ToString(),
                                    head.ToString());
                            CValidationState dummy;
                            if (ProcessNewBlock(dummy, NULL, &block, &it->second))
                            {
                                nLoaded++;
                                queue.push_back(block.GetHash());
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                    }
                }
            } catch (const std::exception& e) {
                LogPrintf("%s : Deserialize or I/O error - %s", __func__, e.what());
            }
        }
    } catch(const std::runtime_error& e) {
        AbortNode(std::string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

void static CheckBlockIndex()
{
    if (!fCheckBlockIndex) {
        return;
    }

    LOCK(cs_main);

    // Build forward-pointing map of the entire block tree.
    std::multimap<CBlockIndex*,CBlockIndex*> forward;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        forward.insert(std::make_pair(it->second->pprev, it->second));
    }

    assert(forward.size() == mapBlockIndex.size());

    std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(NULL);
    CBlockIndex *pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
    assert(rangeGenesis.first == rangeGenesis.second); // There is only one index entry with parent NULL.

    // Iterate over the entire block tree, using depth-first search.
    // Along the way, remember whether there are blocks on the path from genesis
    // block being explored which are the first to have certain properties.
    size_t nNodes = 0;
    int nHeight = 0;
    CBlockIndex* pindexFirstInvalid = NULL; // Oldest ancestor of pindex which is invalid.
    CBlockIndex* pindexFirstMissing = NULL; // Oldest ancestor of pindex which does not have BLOCK_HAVE_DATA.
    CBlockIndex* pindexFirstNotTreeValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_TREE (regardless of being valid or not).
    CBlockIndex* pindexFirstNotChainValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_CHAIN (regardless of being valid or not).
    CBlockIndex* pindexFirstNotScriptsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_SCRIPTS (regardless of being valid or not).
    while (pindex != NULL) {
        nNodes++;
        if (pindexFirstInvalid == NULL && pindex->nStatus & BLOCK_FAILED_VALID) pindexFirstInvalid = pindex;
        if (pindexFirstMissing == NULL && !(pindex->nStatus & BLOCK_HAVE_DATA)) pindexFirstMissing = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTreeValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE) pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotChainValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN) pindexFirstNotChainValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotScriptsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS) pindexFirstNotScriptsValid = pindex;

        // Begin: actual consistency checks.
        if (pindex->pprev == NULL) {
            // Genesis block checks.
            assert(pindex->GetBlockHash() == Params().HashGenesisBlock()); // Genesis block's hash must match.
            assert(pindex == chainActive.Genesis()); // The current active chain's genesis block must be this block.
        }
        assert((pindexFirstMissing != NULL) == (pindex->nChainTx == 0)); // nChainTx == 0 is used to signal that all parent block's transaction data is available.
        assert(pindex->nHeight == nHeight); // nHeight must be consistent.
        assert(pindex->pprev == NULL || pindex->nChainWork >= pindex->pprev->nChainWork); // For every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->nHeight < nHeight))); // The pskip pointer must point back for all but the first 2 blocks.
        assert(pindexFirstNotTreeValid == NULL); // All mapBlockIndex entries must at least be TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE) assert(pindexFirstNotTreeValid == NULL); // TREE valid implies all parents are TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN) assert(pindexFirstNotChainValid == NULL); // CHAIN valid implies all parents are CHAIN valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS) assert(pindexFirstNotScriptsValid == NULL); // SCRIPTS valid implies all parents are SCRIPTS valid
        if (pindexFirstInvalid == NULL) {
            // Checks for not-invalid blocks.
            assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); // The failed mask cannot be set for blocks without invalid parents.
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstMissing == NULL) {
            if (pindexFirstInvalid == NULL) { // If this block sorts at least as good as the current tip and is valid, it must be in setBlockIndexCandidates.
                 assert(setBlockIndexCandidates.count(pindex));
            }
        } else { // If this block sorts worse than the current tip, it cannot be in setBlockIndexCandidates.
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }
        // Check whether this block is in mapBlocksUnlinked.
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
        bool foundInUnlinked = false;
        while (rangeUnlinked.first != rangeUnlinked.second) {
            assert(rangeUnlinked.first->first == pindex->pprev);
            if (rangeUnlinked.first->second == pindex) {
                foundInUnlinked = true;
                break;
            }
            rangeUnlinked.first++;
        }
        if (pindex->pprev && pindex->nStatus & BLOCK_HAVE_DATA && pindexFirstMissing != NULL) {
            if (pindexFirstInvalid == NULL) { // If this block has block data available, some parent doesn't, and has no invalid parents, it must be in mapBlocksUnlinked.
                assert(foundInUnlinked);
            }
        } else { // If this block does not have block data available, or all parents do, it cannot be in mapBlocksUnlinked.
            assert(!foundInUnlinked);
        }
        // assert(pindex->GetBlockHash() == pindex->GetBlockHeader().GetHash()); // Perhaps too slow
        // End: actual consistency checks.

        // Try descending into the first subnode.
        std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
            // A subnode was found.
            pindex = range.first->second;
            nHeight++;
            continue;
        }
        // This is a leaf node.
        // Move upwards until we reach a node of which we have not yet visited the last child.
        while (pindex) {
            // We are going to either move to a parent or a sibling of pindex.
            // If pindex was the first with a certain property, unset the corresponding variable.
            if (pindex == pindexFirstInvalid) pindexFirstInvalid = NULL;
            if (pindex == pindexFirstMissing) pindexFirstMissing = NULL;
            if (pindex == pindexFirstNotTreeValid) pindexFirstNotTreeValid = NULL;
            if (pindex == pindexFirstNotChainValid) pindexFirstNotChainValid = NULL;
            if (pindex == pindexFirstNotScriptsValid) pindexFirstNotScriptsValid = NULL;
            // Find our parent.
            CBlockIndex* pindexPar = pindex->pprev;
            // Find which child we just visited.
            std::pair<std::multimap<CBlockIndex*,CBlockIndex*>::iterator,std::multimap<CBlockIndex*,CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
                assert(rangePar.first != rangePar.second); // Our parent must have at least the node we're coming from as child.
                rangePar.first++;
            }
            // Proceed to the next one.
            rangePar.first++;
            if (rangePar.first != rangePar.second) {
                // Move to the sibling.
                pindex = rangePar.first->second;
                break;
            } else {
                // Move up further.
                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

    // Check that we actually traversed the entire map.
    assert(nNodes == forward.size());
}

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

string GetWarnings(string strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (GetBoolArg("-testsafemode", false))
        strRPC = "test";

    if (CLIENT_VERSION_IS_RELEASE) {
#if defined( HARDFORK_BLOCK )
        ostringstream ss;
        ss << HARDFORK_BLOCK2;
        strStatusBar = "This is a HARDFORK build for block " + ss.str();
#endif
    } else {
#if defined( HARDFORK_BLOCK )
        ostringstream ss;
        ss << HARDFORK_BLOCK2;
        strStatusBar = "This is a HARDFORK pre-release test build, and will fork at block " + ss.str();
#else
        strStatusBar = _("This is a pre-release test build - use at your own risk");
#endif
        // The priority of this message remains @ 0, yet if nothing else is found, it will be displayed by default for pre-release or hardfork builds.
    }

    // These nPriority values set an upper limit on what should be used by the development team, when issuing alert messages,
    // as they are more important than anything else to this client's user..

    // Misc warnings like out of disk space and clock is wrong will be assigned a priority of 1000
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    if (fLargeWorkForkFound)
    {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    }
    else if (fLargeWorkInvalidChainFound)
    {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    // Alerts
    //
    // Any network wide alerts that have shown up, and have a greater priority than what is listed above, will now be checked and the highest
    // priority one is picked & shown to the user.
    // NOTE: If two alerts have the same priority, it will be the 1st one found, that gets shown to the user.
    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority)
            {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
                if( !isMainNetwork() ) {
                    //! A Special Alert message can be used to change the TestNet retarget parameters for all the nodes.
                    //! All Mining must be briefly stopped, then the alert issued and mining again resumed.  The new
                    //! settings will then be used for calculating a retarget difficulty value for all the next blocks
                    //! mined.  This causes a complete RetargetPID Controller reset to occur and the new constants
                    //! for P-I-D terms loaded, as if the software was just booting up.  No new TestNet miners can be
                    //! added to the network as any reload of the blockchain will fail, however it can be useful for
                    //! testing different settings on a small controlled TestNet, were those details can be ignored
                    //! and the user(s) simply want new values tried and experimented with for a time.
                    if( boost::algorithm::istarts_with(strStatusBar, "retargetpid ") ) {
                        RetargetPidReset( strStatusBar.substr(11, string::npos), chainActive.Tip() );
                    }
                }
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}

void static RelayAlerts(CNode* pfrom)
{
    LOCK(cs_mapAlerts);
    BOOST_FOREACH(PAIRTYPE(const uint256, CAlert)& item, mapAlerts)
        item.second.RelayTo(pfrom);
}







//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool static AlreadyHave(const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:
        {
            bool fTxMemPool = mempool.exists(inv.hash);
            bool fOrphan = mapOrphanTransactions.count(inv.hash) != 0;
            CCoinsViewCache &view = *pcoinsTip;
            const CCoins* pCoins = view.AccessCoins(inv.hash);
            bool fInCoins = pCoins != NULL;

            if( fInCoins && pCoins->vout.empty() )
                LogPrintf( "%s : Unexpected pruned coins found in database, reindexing maybe required.\n", __func__ );

            return fTxMemPool || fOrphan || fInCoins;
        }
    case MSG_BLOCK:
        // Don't really need to look in mapBlockIndex, if we had the block, a non-
        // zero hash would be found, and returned from the cross reference map.
        // GetRealHash() returns zero, if its not found, but we'll double check
        // anyway...
        uint256 aRealHash = inv.hash.GetRealHash();
        return aRealHash != 0 && mapBlockIndex.count(aRealHash) != 0;
    }
    // Don't know what it is, just say we already got one
    return true;
}


void static ProcessGetData(CNode* pfrom)
{
    std::deque<CInv>::iterator it = pfrom->vRecvGetData.begin();

    vector<CInv> vNotFound;

    LOCK(cs_main);

    while (it != pfrom->vRecvGetData.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        const CInv &inv = *it;
        {
            boost::this_thread::interruption_point();
            it++;

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK)
            {
                bool send = false;
                uint256 aRealHash = inv.hash.GetRealHash();
                BlockMap::iterator mi = ( aRealHash != 0 ) ? mapBlockIndex.find( aRealHash ) : mapBlockIndex.end();
                if (mi != mapBlockIndex.end())
                {
                    if (chainActive.Contains(mi->second)) {
                        send = true;
                    } else {
                        // To prevent fingerprinting attacks, only send blocks outside of the active
                        // chain if they are valid, and no more than a month older than the best header
                        // chain we know about.
                        send = mi->second->IsValid(BLOCK_VALID_SCRIPTS) && (pindexBestHeader != NULL) &&
                            (mi->second->GetBlockTime() > pindexBestHeader->GetBlockTime() - 30 * 24 * 60 * 60);
                        if (!send) {
                            LogPrintf("ProcessGetData(): ignoring request from %s for old block that isn't in the main chain\n", GetPeerLogStr(pfrom));
                        }
                    }
                }
                if (send)
                {
                    // Send block from disk
                    CBlock block;
                    ReadBlockFromDisk(block, (*mi).second);
                    if (inv.type == MSG_BLOCK)
                        pfrom->PushMessage("block", block);
                    else // MSG_FILTERED_BLOCK)
                    {
                        LOCK(pfrom->cs_filter);
                        if (pfrom->pfilter)
                        {
                            CMerkleBlock merkleBlock(block, *pfrom->pfilter);
                            pfrom->PushMessage("merkleblock", merkleBlock);
                            // CMerkleBlock just contains hashes, so also push any transactions in the block the client did not see
                            // This avoids hurting performance by pointlessly requiring a round-trip
                            // Note that there is currently no way for a node to request any single transactions we didnt send here -
                            // they must either disconnect and retry or request the full block.
                            // Thus, the protocol spec specified allows for us to provide duplicate txn here,
                            // however we MUST always provide at least what the remote peer needs
                            typedef std::pair<unsigned int, uint256> PairType;
                            BOOST_FOREACH(PairType& pair, merkleBlock.vMatchedTxn)
                                if (!pfrom->setInventoryKnown.count(CInv(MSG_TX, pair.second)))
                                    pfrom->PushMessage("tx", block.vtx[pair.first]);
                        }
                        // else
                            // no response
                    }

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue)
                    {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, chainActive.Tip()->GetBlockSha256dHash()));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue = 0;
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                    CTransaction tx;
                    if (mempool.lookup(inv.hash, tx)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                        pushed = true;
                    }
                }
                if (!pushed) {
                    vNotFound.push_back(inv);
                }
            }

            // Track requests for our stuff.
            g_signals.Inventory(inv.hash);

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK)
                break;
        }
    }

    pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), it);

    if (!vNotFound.empty()) {
        // Let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever. Currently only SPV clients actually care
        // about this message: it's needed when they are recursively walking the
        // dependencies of relevant unconfirmed transactions. SPV clients want to
        // do that because they want to know about (and store and rebroadcast and
        // risk analyze) the dependencies of transactions relevant to them, without
        // having to download the entire memory pool.
        pfrom->PushMessage("notfound", vNotFound);
    }
}

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    RandAddSeedPerfmon();

    LogPrint( "net", "received: %s (%u bytes) from %s\n", SanitizeString(strCommand), vRecv.size(), GetPeerLogStr(pfrom) );
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        LogPrintf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }



    if (strCommand == "version")
    {
        //! Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_DUPLICATE, string("Duplicate version message"));
            Misbehaving(pfrom->GetId(), 1);
            return false;
        }

        bool fBadProtocol = false;
        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;

        //! Start with getting a few things first, before the nightmare begins as what needs to happen with past protocols and address objects.
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime;

        //! If it's to old, its easy, just disconnect and your out of here.
        
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION)
        {
            //! relay alerts prior to disconnection
            RelayAlerts(pfrom);
            //! disconnect from peers older than this proto version
            LogPrintf("partner %s using obsolete version %i; disconnecting\n", pfrom->addr.ToString(), pfrom->nVersion);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                               strprintf("Version must be %d or greater", MIN_PEER_PROTO_VERSION));
            pfrom->fDisconnect = true;
            return true;
        }
#if defined( HARDFORK_BLOCK )
        if( pfrom->nVersion < MIN_PEER_PROTO_VERSION_AFTER_HF && chainActive.Height() > HARDFORK_BLOCK ) //! If the hardfork block is reached, send an error message and disconnect.
        {
            //! relay alerts prior to disconnection
            RelayAlerts(pfrom);
            //! disconnect from peers older than this proto version
            LogPrintf("partner %s using version %i obsolete since the HARDFORK_BLOCK %d was reached; disconnecting\n", pfrom->addr.ToString(), pfrom->nVersion, HARDFORK_BLOCK);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                               strprintf("ERROR: Version must be %d or greater after the HARDFORK_BLOCK %d, please update!", MIN_PEER_PROTO_VERSION_AFTER_HF, HARDFORK_BLOCK));
            pfrom->fDisconnect = true;
            return true;
        }

       if( pfrom->nVersion < MIN_PEER_PROTO_VERSION_AFTER_HF2 && chainActive.Height() > HARDFORK_BLOCK2 ) //! If the second hardfork block is reached, send an error message and disconnect.
       {
            //! relay alerts prior to disconnection
            RelayAlerts(pfrom);
            //! disconnect from peers older than this proto version
            LogPrintf("partner %s using version %i obsolete since the HARDFORK_BLOCK %d changing PID parameters was reached; disconnecting\n", pfrom->addr.ToString(), pfrom->nVersion, HARDFORK_BLOCK2);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                              strprintf("ERROR: Version must be %d or greater after the HARDFORK_BLOCK %d changing PID parameters, please update!", MIN_PEER_PROTO_VERSION_AFTER_HF, HARDFORK_BLOCK2));
            pfrom->fDisconnect = true;
            return true;
       }

#endif
              
        //! Protocol 70009 uses only IP addresses on initiating connections over clearnet, after that full size addresses
        //! are always used for any network type, the i2p destination maybe zero, or if not the ip field must be set to the
        //! new GarlicCat field (an IP6/48) specifier.  This is checked and fixed, if found to be incorrect.
        //! For processing inbound connection messages that contain addresses, you can expect the same behavior & other than
        //! that one detail, the format has not been changed, so it should work well with older versions too.  With the
        //! exception of 70008, which needs a correction to work properly, that version will quickly be abandoned as a
        //! 'buggy' pre-release test protocol this developer never got right.

        //! Because we are getting a version message that may NOT have the expected stream type setup correctly,
        //! the data caused an exception to be thrown, until this huge effort was made to correct the many problems
        //! found while attempting to initialize a version/verack cycle with other, even older buggy brained peers
        //! on the Anoncoin network.

        //! if we are expecting an i2p address, yet the version message itself is much shorter, and only contains
        //! the 2 ip addresses (addMe,addrFrom).  We need to not get the addrMe value from the data stream until after
        //! some additional checking.... try to figure out what to have our streams set to while dealing with these problems.
        if( (pfrom->nVersion < 70009) && !pfrom->addr.IsNativeI2P() ) {
            //! If the peer protocol version was 70008, then the inbound addresses are always full size, however the data within
            //! the message themselves is only ip information when sent over a clearnet connection.  If we're connecting that way
            //! it means the wrong stream type has been set in the ssSend DataStream field from that node.  At least until AFTER it
            //! responds to our response. (if this is an outbound connection attempt by it) its version message will not have its
            //! ssSend datastream field type set to anything other than full size, even though the data itself will only be 2 ip
            //! addresses without the i2p destinations.  This gets corrected on once it pushes it version message.

            //! Earlier protocol version also send us larger version message addresses all the time, if they think we support the
            //! NODE_I2P service they start out right away trying to connect to us with that full size address object, this code
            //! now works for those cases, no special version checking required.....other than knowing its less that 70009.
            //! Protocol 70006 lies about its services on clearnet, and is handled later on in this code.
            unsigned int nRemaining = vRecv.size();
            unsigned int nAddrWithI2P = ::GetSerializeSize(addrMe, SER_NETWORK, PROTOCOL_VERSION);
            // size_type nAddrWithIP = ::GetSerializeSize(addrMe, SER_NETWORK | SER_IPADDRONLY, PROTOCOL_VERSION);
            LogPrint( "version", "peer %s, so far we know nVersion=%d nServices=0x%016x nTime=%d, and there are %d bytes left in the message.\n", pfrom->addrName, pfrom->nVersion, pfrom->nServices, nTime, nRemaining );
            //! On clearnet protocol 70009 assumes ip addresses only, so if the inbound version is larger than that, then
            //! we need to switch the stream type to include those larger address objects, do it now, and do it for both recv/send streams.
            if( nRemaining > nAddrWithI2P * 2 ) {
                pfrom->SetStreamType( SER_NETWORK );
                LogPrint( "version", "switched stream type to full destination addresses.\n" );
                fBadProtocol = true;                                //! Protocol bugs have been a huge job to debug, this is only used for that.
            }
            vRecv >> addrMe;                                        //! Try it now anyway, or throw the error

        } else
            vRecv >> addrMe;

        //! If there is still more data to receive, it should be the "from" address
        // if( fBadProtocol ) LogPrintf( "Size of next object =%d, Remainder in buffer=%d\n", ::GetSerializeSize(addrFrom, SER_NETWORK, PROTOCOL_VERSION), vRecv.size() );

        if( !vRecv.empty() )
            vRecv >> addrFrom;

        //! Someone/Somehow we're getting addresses with garbage in the upper service bits, so clear everything above what we currently have defined.
        //! In this software release, we may remove this at a later date after more research by the development team is done.
        //! Values like these have been found: 0xa224000000000083 &  0xa124000000000083
        /* pfrom->nServices &= 0xFF; Leave unchanged as declared for viewing the full value sent to us. */
        pfrom->addr.nServices = pfrom->nServices & 0xFF;    //! Make sure our node copy of their address is the same while purging undefined service bits
        addrMe.nServices &= 0xFF;                           //! Purge undefined service bits for my address
        addrFrom.nServices = pfrom->nServices & 0xFF;       //! Make sure they are the same while purging undefined service bits for their from address
        //! The pfrom->addr and addFrom addresses will be used later to update addrman, which one depends on the inbound or outbound state
        //! Unauthorized sharing of undefined service bits through peer addresses is not allowed in this version

        //! Any peer with a lesser version than our newest level needs special attention.
        //! So we keep our inhouse database correct when receiving i2p destination addresses.
        //! Starting with v70009 we introduce the concept of a GarlicCat-agory address to
        //! the CNetAddr object classes private ip 16 byte array field, its value is now
        //! selected as a specific IP6/48 formated address, one we can always identify as
        //! indicating an I2P address.  This is left out of the details received from
        //! any peer with a lesser version.  In the future we'll change this again to a new
        //! protocol level, one which optimizes the storage required for I2P Destination
        //! keys (& possible optional certificate), without even using the base64 format
        //! anymore.

        //! Whenever we receive these addresses, if its from an i2p destination, the ip address should be the GarlicCat, if not, set them up correctly,
        //! as we count on it after this, and a peer could be trying to send us anything... So if we're not on I2P make sure these fields are blank
        //! As it came from the outside world, check everything and  fix it if necessary:
        if( pfrom->addr.IsNativeI2P() ) {
            addrMe.CheckAndSetGarlicCat();
            addrMe.SetPort( 0 );              //! Make sure the CService port is set to ZERO so AddrMan's commands can work with matching.
            //! Nodes lie about their From address, even over i2p.  When that happens, CheckAndSetGarlicCat below returns false,
            //! as it was not able to correctly fix the addrFrom field.  In that case, we 'COULD' correct the address here, as we know
            //! from which i2p destination they connect from.  However, this now takes on a new and special role for dynamic i2p peers
            //! An option has been added to the anoncoin.conf file -i2p.mydestination.shareaddr which allows the user to set the behavior,
            //! the default is to share our destination if we are running with static, and not share it if a dynamic destination is being used.
            //! Allowing the user to decide, is done in the config file otherwise.
            //! This new feature, does not require rebuilding software just to change the setting.
            addrFrom.SetPort( 0 );            //! Make sure the CService port is set to ZERO, so AddrMan's commands can work with matching
            bool fFromChanged = addrFrom.CheckAndSetGarlicCat();
            if( (CService)pfrom->addr != (CService)addrFrom ) {
                LogPrint( "version", "I2P Peer @ %s, protocol %d is reporting a different destination addrFrom=%s\n", pfrom->addr.ToString(), pfrom->nVersion, addrFrom.ToString() );
            }
        } else {            //! must be a clearnet address in the IP field, and not have a I2P address.
            //! In the future this could be used to autoswitch the node to a more secure network, like I2P!
            //! At the present time (March 2015) the development team opinion is: Being able to link an ip addresses to an i2p destination is a bad idea
            //! These next 2 lines wipe out the I2P field, if anything was there.
            addrMe.SetI2pDestination( "" );
            addrFrom.SetI2pDestination( "" );
            if( (CNetAddr)pfrom->addr != (CNetAddr)addrFrom )
                LogPrint( "version", "Clearnet Peer @ %s, protocol %d is not reporting the same addrFrom=%s\n", pfrom->addrName, pfrom->nVersion, addrFrom.ToString() );
            else if( !pfrom->fInbound && pfrom->addr.GetPort() != addrFrom.GetPort() )
                LogPrint( "version", "Clearnet Peer @ %s, protocol %d matches ip addrFrom, but ports differ, addrFrom=%s\n", pfrom->addrName, pfrom->nVersion, addrFrom.ToString() );
        }

        //! Now we should have the right address data for addrMe and addrFrom, with or without buggy brained builds.  If not, these LogPrintf lines help us debug it.
        // if( fBadProtocol ) LogPrintf( "Size of next object =%d, Remainder in buffer=%d\n", sizeof( nNonce ), vRecv.size() );

        if( !vRecv.empty() )
            vRecv >> nNonce;

        // if( fBadProtocol ) LogPrintf( "Size of next object =unknown, Remainder in buffer=%d\n", vRecv.size() );

        //! If it is still not empty input, get us the subversion string as well, plus sanitize it
        if (!vRecv.empty()) {
            vRecv >> LIMITED_STRING(pfrom->strSubVer, 256);
            if( pfrom->strSubVer.size() < 3 )
                pfrom->cleanSubVer = "/??:?/";
            else
                pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }

        //! Disconnect certain incompatible clients
        //! ToDo: At some point start disconnecting nodes that report no or very tiny subver strings, or need to be cleaned just to pass.
        //!       Also we have one satoshi client the keeps trying to connect, it is not compatible an should have its version added here.
        const char *badSubVers[] = { "/potcoinseeder", "/reddcoinseeder", "/worldcoinseeder" };
        for (int x = 0; x < 3; x++)
        {
            if (pfrom->cleanSubVer.find(badSubVers[x], 0) == 0)
            {
                LogPrintf("invalid subver %s at %s, disconnecting\n", pfrom->cleanSubVer, pfrom->addrName);
                pfrom->PushMessage("reject", strCommand, REJECT_INVALID, string("invalid client subver"));
                pfrom->fDisconnect = true;
                return true;
            }
        }

        // if( fBadProtocol ) LogPrintf( "Size of next object =%d, Remainder in buffer=%d\n", sizeof( pfrom->nStartingHeight ), vRecv.size() );

        //! Finally if there is still more data on the input stream, it should be the starting block height...
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;

        // if( fBadProtocol ) LogPrintf( "Size of next object =%d, Remainder in buffer=%d\n", sizeof( pfrom->fRelayTxes ), vRecv.size() );

        //! The last field possible is the optional Relay Tx value
        if (!vRecv.empty())
            vRecv >> pfrom->fRelayTxes; //! set to true after we get the first filter* message
        else
            pfrom->fRelayTxes = true;

        if (nNonce == nLocalHostNonce && nNonce > 1) {                                      //! Disconnect if we connected to ourself
            LogPrintf("connected to self at %s, disconnecting\n", pfrom->addrName);
            pfrom->fDisconnect = true;
            return true;
        }
        //! ToDo: Once we have the nNonce value, we want to compare it with the nNonce values for all the other nodes we are connected to
        //! if there is a match, it means we are connected to the same machine over multiple network types, and should disconnect this
        //! node.  We many need to do this instead periodically, as initially the nNonce is more than likely different right now, further
        //! investigation is required as to how to solve this problem.

        pfrom->addrLocal = addrMe;                      //! Always assign the nodes local address
        //! This next code is unique to Anoncoin too, we've added a new type of local address discovery source (LOCAL_PEER)
        //! If we are outbound, and connected and do not yet know our local address, we add it to the map with the lowest
        //! possible score above NONE.  As time progresses it may become the best possible local address we know of, if
        //! no other discovery finds our external IP address, if its an inbound connection we bump its score by one as it
        //! was seen, that part of this code works just like it always has in past versions, although for many coins
        //! they fail to do the AddLocal step so marking it as seen had no effect on the mapLocalHost list.
        //! One problem of course is that, the port given could be all over the place, so we be sure and cast the address
        //! passed to AddLocal as only a CNetAddr object, then the default listen port gets set to our default listen port,
        //! and that is what is added to the map correctly, otherwise you will get many different local addresses all with
        //! different ports.
        //! So for outbound connections this has the effect of bumping our score +1 on local addresses when AddLocal is
        //! called & we already have the address setup in the map, in that regard, it now works like SeenLocal does for only
        //! inbound connections, where the score is, and always bumped +1 when a new peer shows up.  The problem was it
        //! did nothing if that local address was never found in the 1st place.
        if( addrMe.IsRoutable() )
            pfrom->fInbound ? SeenLocal(addrMe) : AddLocal( (CNetAddr)addrMe, LOCAL_PEER );

        //! Here if the node does not seem to support that I2P service, we keep the IP only setting for our send stream.
        //! Both versions 70008 and 70007 work this way.  But have this code in different places.
        //! If it does support that I2P service, then we (should) switch to sending full CNetAddr objects with contain both
        //! IP and I2P destination information.
        //! Also see: SetRecvStreamType() in the verack message
        if( (pfrom->nVersion < 70009) && !pfrom->addr.IsNativeI2P() ) {
            //! Protocol 70006 builds on clearnet lie, they do not even have the i2p address space built into their wallet software and report NODE_I2P as a service
            //! which they can not offer.  Fix that now and don't bother changing the stream type
            if( pfrom->nVersion == 70006 ) {
                pfrom->nServices &= ~NODE_I2P;
                LogPrint( "version", "peer %s lies about the services it supports, correcting it before continuing.\n", pfrom->addrName );
            }
            else {
                //! If their services do not list NODE_I2P, we now have that covered in 70009, most <=70008 protocol nodes should switch,
                //! there stream type BEFORE pushing their version, if we've made an outbound connection to them, we've already told them
                //! that we support it.  So we do that here now too before pushing our version back to them.  But only for the Send Stream
                //! not until we get the verack message back, do we switch the receiver stream, by convention, as per the older protocols.
                u_int uBefore = pfrom->GetSendStreamType();
                pfrom->SetSendStreamType( uBefore & ((pfrom->nServices & NODE_I2P) ? ~SER_IPADDRONLY : SER_IPADDRONLY) );
                LogPrint( "version", "peer %s, setting send stream type, was %08x, now set to %08x\n", pfrom->addrName, uBefore, pfrom->GetSendStreamType() );
            }
        }
        //! If this message was inbound, they've sent us a version, so we should now send ours back.
        //! They did that, when creating a peer as our node address in the 1st place.
        //! Now that we have their version, we push ours so they know who we are too, after this all kind of things can happen.
        //! V9 clients (protocols > 70007) always have NODE_I2P set.  On clearnet, it could be either way for versions < 70008,
        //! depending on how they obtained our address and what services were listed!  Its NOT deterministic!
        if( pfrom->fInbound )
            pfrom->PushVersion();

        //! This flag, fClient gets set TRUE, if the other node is NOT even a full node supporting full blockchain data services
        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        //! Potentially mark this peer as a preferred download peer.
        UpdatePreferredDownload(pfrom, State(pfrom->GetId()));

        //! Change version
        pfrom->PushMessage("verack");

        //! When we send to that node, use his protocol version, or our version, whichever is less...
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        //! We still only exchange ip addresses in the version messages if on clearnet with Protocol 70009, This makes the handshake
        //! deterministic.  Not based at all on services, which may have been a lie, depending on how the information was obtained,
        //! or the software was built (70006).
        //! We must not change our send stream type, until after our version is pushed on inbound connections, because
        //! the other node does not yet know for sure, whom we are and what services we really support.
        //! If our connection was outbound, then the peer is now responding with it's version.  We do not want to change our receiver
        //! stream type until after that handshake has been made.

        //! So at this point we want to always send/receive to other 70009+ peers with full ip/i2p destinations as the stream type setting.
        //! Yet we can ONLY do that "if" they are NOW reporting that they support the NODE_I2P service.  This allows for the development
        //! of other software & services, one's that use the new protocol, but do not claim to support the larger address space, for
        //! whatever reason. As deterministically as possible, with a new routine, we attempt now to work for all the various ways the
        //! inbound/outbound connections may behave as peers using this software...
        if( pfrom->nVersion >= 70009 ) {
            bool fBefore = (pfrom->GetStreamType() & SER_IPADDRONLY) == 0;
            pfrom->SetStreamTypeBasedOnServices();      //! We set both in/out stream types strictly based on the peers NODE_I2P service
            bool fAfter = (pfrom->GetStreamType() & SER_IPADDRONLY) == 0;
            //! Over i2p we don't really need to report anything, as the streamtype is always full size, we are here initially for debugging.
            //! on clearnet we log the before & after states of the decision made, as the NODE_I2P service determines what this peer supports.
            if( !pfrom->addr.IsI2P() ) {
                LogPrint( "version", "clearnet peer %s is protocol 70009+, IP handshake completed. NODE_I2P service indicates %s, stream type %s.\n",
                          pfrom->addrName, fAfter ? "full I2P support" : "no I2P support", fBefore != fAfter ? "switched to full I2P" : "left unchanged" );
            } else if( (pfrom->nServices & NODE_I2P) == 0 ) {
                //! Just encase someone tries to not have NODE_I2P set, while running over I2P, make sure it IS set, because otherwise they it is a lie
                LogPrint( "version", "WARNING - I2P peer %s is protocol 70009+, Hard setting NODE_I2P service.  That should not be required.\n", pfrom->addrName );
                pfrom->nServices |= NODE_I2P;
            } else
                LogPrint( "version", "I2P peer %s is protocol 70009+, version handshake completed normally.\n", pfrom->addrName );
        }

        if (!pfrom->fInbound || IsDarknetOnly()) //Authorize inbound getaddr request if we are in Darknet only
        {
            //! Advertise our address, by doing this now after the version exchange, the other node may pass it on to nodes
            //! connected to them, nearly right away, at which point we may soon start to see inbound connections to us.
            //! so we want to be sure and listen for them (we always do on i2p), and only if we're pretty sure we know what
            //! our ip address is, on i2p we always know for sure, but over clearnet its only a maybe.
            //! NEW Parameter: If its over I2P, only push it if -i2p.mydestination.shareaddr=1 (true) This is now done in the
            //! call to GetLocalAddress(), making it incompatible with the new code for clearnet by calling IsPeerAddrLocalGood()
            //! So we need to NOT do that if operating over i2p.
            if( ( pfrom->addr.IsI2P() || fListen ) && !IsInitialBlockDownload() )
            {
                //! If its an i2p destination we needed to get in here, ignoring the fNoListen flag, if our
                //! destination is not shared, GetLocalAddress returns an unroutable 0.0.0.0 destination
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if( addr.IsRoutable() ) {                        //! One last check!  Is our local address routable?
                    pfrom->PushAddress(addr);
                } else if( !pfrom->addr.IsI2P() && IsPeerAddrLocalGood(pfrom) ) {
                    //! What this does is after this outbound connection has been made, where we
                    //! only have a bad local address for ourselves.  It will discover that the
                    //! peers address for us is much better and push that back to them instead.
                    //! Something we don't want to do for dynamic i2p destinations that are not shared.
                    //! CSlave: Changed that setting now dynamic i2p destination are also shared by default.
                    addr.SetIP(pfrom->addrLocal);
                    pfrom->PushAddress(addr);
                }
            }

            //! Get recent addresses, if we're new and hungry for peers, lets get them... But not from peers running the old protocol version!
            if ((pfrom->fOneShot || addrman.size() < 1000) && (pfrom->nVersion >= MIN_PEER_PROTO_VERSION_AFTER_HF))
            {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            //! ToDo: if any changes to the port setting are needed, diagnostic output is being logged for evaluation.
            addrman.Good(pfrom->addr);              //! If its an outbound connection, it must always be in the addrman already
        } else {
            //! Lots of peers on cryptocoin networks lie about what address they are really from, this standard code from bitcoin
            //! allows them to do that, in many cases the peer does not yet know for sure what their ip address is.
            //! Happens all the time.
            //! When an inbound connection ip does not match the reported ip in that peer's version information address, their
            //! 'real' ip address is not added to AddrMan, marked as Good, and used later on to find connections by this peer
            //! or otherwise passed on. They have connected to the network, while keeping their destination secret.
            //! If they have collected lots of addresses, they can make plenty of outbound connections, without ever seeing an
            //! inbound connection, because no other peers know about them.
            //!
            //! Best guess this developer can come up with is that in some cases they do not want their ip address advertised.
            //! The code is left as is in Anoncoin v9.4, protocol 70009, for clearnet operation.  It's place and use in
            //! cryptocurrency history is left upto the informed mind to figure out why a peer is reporting a different ip...
            //!
            //! This behavior is also seen on the i2p network, peers reporting their addrFrom address incorrectly, this is a
            //! situation which at first glance, seemed really stupid to this developer.  I2P destinations are not a 'maybe'
            //! like on clearnet, except their is a case where the destination is not very useful longterm.  When the node
            //! is using a dynamically generated destination.
            //! After further investigation, it now seems like a great idea to implement this in our standard code,
            //! it offers a solution to what is a different problem, one where the peer is not running a static i2p address,
            //! and it would be best to NOT include their address in our addrman or pass it on to other peers anyway.  So
            //! starting with v9.5 software running protocol 70009+, it will now have a new parameter and option in the
            //! anoncoin.conf file.  That is....it will allow any user to override what is our new default behavior.
            //! For dynamic destinations, the 'from' field gets zero'd out before pushing our version message out.  When
            //! running a static i2p destination it will report it correctly and work as it should. If a user does want it
            //! advertised, they can choose to override this setting in the config file.  Independent of the static/dynamic
            //! destination selection.  See our anoncoin.conf.sample file for more details.
            if( ((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom )
            {
                //! When addrman receives Add commands like this it realizes the address is inbound and checks to make sure
                //! that the address nService and Port values match what the node itself tells it they should be, if they
                //! are different it updates its record.
                //! The default port should already have been set be the peer when it created the CAddress object to send us,
                //! and tests show this must normally be the case.  As an inbound connection port is not likely the default
                //! Anoncoin network port  This needs to work really well for both I2P destinations and clearnet addresses.
                addrman.Add(addrFrom, addrFrom);
                addrman.Good(addrFrom);
            }
        }

        //! Relay alerts
        RelayAlerts(pfrom);

        pfrom->fSuccessfullyConnected = true;

        LogPrintf("receive version message: %s: version %d, blocks=%d, services=0x%016x us=%s, them=%s, peer=%s\n", pfrom->cleanSubVer, pfrom->nVersion, pfrom->nStartingHeight, pfrom->nServices, addrMe.ToString(), addrFrom.ToString(), pfrom->addrName);

        AddTimeData(pfrom->addr, nTime);
    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        Misbehaving(pfrom->GetId(), 1);
        return false;
    }


    else if (strCommand == "verack")
    {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
        //! Mark this node as currently connected, so we update its timestamp later.
        if (pfrom->fNetworkNode) {
            LOCK(cs_main);
            State(pfrom->GetId())->fCurrentlyConnected = true;
        }

        //! Older protocols < 70008 may not always have NODE_I2P in their services, if not we make sure
        //! our stream type for receiving stays IP only.  Why we wait to switch the receiver stream after
        //! the verack makes no sense to this developer, this should be done at the same time as the
        //! send stream is switched (if need be, based on the nodes service options), however it was
        //! in older protocols, so sticking with it here now as well in v70009.
        if( (pfrom->nVersion < 70009) && !pfrom->addr.IsNativeI2P() ) {
            //! Protocol 70006 builds on clearnet lie, they do not even have the i2p address space built into their wallet software
            //! and report NODE_I2P as a service which they can not offer.  That has already been fixed in the version message,
            //! and we do nothing here for those builds.
            if( pfrom->nVersion != 70006 ) {
                u_int uBefore = pfrom->GetRecvStreamType();
                pfrom->SetRecvStreamType( uBefore & ((pfrom->nServices & NODE_I2P) ? ~SER_IPADDRONLY : SER_IPADDRONLY) );
                LogPrint( "version", "verack from %s, recv stream type was %08x, now set to %08x\n", pfrom->addrName, uBefore, pfrom->GetRecvStreamType() );
            }
        }
    }


    else if (strCommand == "addr")
    {
        //! Nodes lie about the services they support, in some cases we receive addresses that are not serialized properly, even though the node
        //! claims to support NODE_I2P it sends us only IP address payloads.  An attempt was made to double check the vRecv buffers version type
        //! even it lies, so they must not be compiled to even include the i2p address payload.  The 1st error that hits on the CDatastream
        //! read now is the only place we can finally detect the error and correct the stream type and services for these type of
        //! nodes.  No special checking is done there now, it's been learned that Protocols 70006 are the nodes that are not setup properly,
        //! so actions have been taken to design the problem away before an error is generated...
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        //! Don't want addr from older versions... 
        //! Old 8.5.6 nodes are spamming the network with HUGE peers.dat, overwhelming the addrman!
        //if (pfrom->nVersion < MIN_PEER_PROTO_VERSION && addrman.size() > 1000)
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION_AFTER_HF) {
            LogPrint("addrman", "Don't want addr from older peer with older protocol versions such as %d \n", pfrom->nVersion);           
            return true;}
        if (vAddr.size() > 1000) {
            Misbehaving(pfrom->GetId(), 20);
            return error("message addr size() = %u", vAddr.size());
        }
        //! Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;                    //! Set to 10 minutes ago
        // bool fPossibleI2pAddrs = (pfrom->nServices & NODE_I2P ) != 0;
        //! Hopefully the services supported by the peer show it supports I2P addresses. (services can lie), look at the stream type for now
        bool fPossibleI2pAddrs = (pfrom->GetRecvStreamType() & SER_IPADDRONLY ) == 0;
       //! Hopefully that stream type was set correctly before we do this, it should have been done correctly before this message is processed.

        //! Any peer addresses should be checked, as their size could include I2P destination addresses,
        //! we don't know what they are sending us, could be clearnet ip's or full i2p base64 destinations,
        //! perhaps with or without the correct GarlicCat field set in the ip address space....
        LogPrint( "addrman", "received %d addresses, serialized size %u bytes each, from %s\n",
                   vAddr.size(), ::GetSerializeSize(pfrom->addr, pfrom->GetRecvStreamType(), pfrom->nVersion), GetPeerLogStr(pfrom) );
        //! Fix problems with Addresses, before normal processing is done:
        //! If any of those addresses are I2P destinations we introduce some new stress testing on them to confirm
        //! they are valid and setup correctly before adding or sharing them with others.
        //! If the StreamType is IP only, our memory should have been cleared and zeroed out when the CAddress object
        //! was created, only the ip/port fields have been filled in when the address deserialized, but at times the port has been zeroed out
        //! if it is an NODE_I2P source, but claims its an ip address, we make sure the i2p field is zero.  If it is an i2p addr we make sure
        //! the garliccat ip addr has been setup correctly.
        BOOST_FOREACH(CAddress& addr, vAddr) {
            LogPrint("addrman", "addrman: address received from %s \n", GetPeerLogStr(pfrom)); 
            addr.print ();
            if( fPossibleI2pAddrs ) {                       //! So there MAYBE valid I2P addresses from this peer, as they are running NODE_I2P as well
                addr.CheckAndSetGarlicCat();                //! Fix the address by adding the GarlicCat field, if it came in not set correctly
                if( addr.IsNativeI2P() )                    //! For native I2P Addresses...
                    addr.SetPort( 0 );                      //! Make sure the CService port is set to ZERO so AddrMan's lookups can match I2P addresses correctly
                else                                        //! This should not be necessary, but comparisons and everything else assumes the i2p adddress area is zero for ip addrs
                    addr.SetI2pDestination( "" );           //! Clears the I2P destination field, but leaves the ip field as it was found
            } else
                assert( addr.GetI2pDestination() == "" );   //! Only ip addresses came in, programming error if the i2p destination field is not zero
            //! Regardless of the node service type source of this address, if its an ip address, check and fix the port
            if( !addr.IsNativeI2P() && addr.GetPort() == 0 ) { //! Clearnet CAddress objects often get their ports zero'd out, due to our efforts to get i2p addrs to work?
                // LogPrintf( "Set address %s port to default\n", addr.ToString() );
                addr.SetPort( Params().GetDefaultPort() );    //! So set it to the default port, so 'most' will match correctly in AddrMan lookups later on
            }
            //! Someone/Somehow we're getting addresses with garbage in the upper service bits, so clear everything above what we currently have defined.
            //! Values like these have been found: 0xa224000000000083 &  0xa124000000000083
            addr.nServices &= 0xFF;         //! Purge undefined service bits
        }

        //! Many routines, GetNetwork(), IsI2P() and others on an address, only work properly if the above GarlicCat field has been setup
        BOOST_FOREACH(CAddress& addr, vAddr)
        {
            boost::this_thread::interruption_point();

            //! IsReachable looks at this nodes configuration, if this address is for a network that
            //! is outside of how this node is configuration atm, then it will be false.  If there
            //! is a local address available for the network which this addresses is for, then it
            //! returns true.  Regardless, we consider all I2P addresses to be reachable, so they
            //! are passed on and shared with all nodes.
            bool fReachable = IsReachable(addr) || addr.IsI2P();
            //! We're now going to start having all the folks on clearnet help us out.  While at the
            //! same time not destroy the beautiful random way this works.  Sharing good I2P addresses
            //! is something we want to have done by all peers, regardless of any personal benefit to
            //! those running on clearnet only.  Could really help out those of us running mixed mode,
            //! which in turn, will filter over into helping those out on i2p only as well.

            //! Set the time on this address to be 5 days old, if their time is all screwed up
            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);               // Mark this address as known by the peer

            //! Here if its just a few addresses, really new (10 minutes) & routeable we pay special
            //! attention, and forward those addresses on randomly to our peers.
            //! IsRoutable() looks at this node's setup, a call in there to IsLocal() makes sure those
            //! addresses will not be included  Any other considerations are just validity checks on
            //! the addresses themselves.  PushAddress does more checking based on the node its about
            //! to be pushed TO, if it gets that far.
            if( addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable() )
            {
                //! Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    //! Use deterministic randomness to send to the same nodes for 24 hours
                    //! at a time so the addrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH(CNode* pnode, vNodes)
                    {
                        if (pnode->nVersion < MIN_PEER_PROTO_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1;       //! limited relaying of addresses outside our network(s)
                    if( addr.IsI2P() ) nRelayNodes *= 2;        //! If its an I2P address double the number of peers we share it with
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi) {
                        CNode* pToNode = ((*mi).second);        //! Local copy of the node pointer, this time whom we are relaying it too...
                        //! If its not an I2P addr push it, if it is, consider the services that peer is capable of, before doing that.
                        //! ToDo: More logic here would make the pushes smarter about getting i2p addrs to nodes connected via i2p
                        if( !addr.IsI2P() || ( (pToNode->nServices & NODE_I2P) != 0 ) )
                            pToNode->PushAddress(addr);
                    }
                }
            }

            //! Do not store addresses outside our network, unless its an I2P destination, then we keep it
            //! The other special case is, if we're running i2ponly, then we only keep addresses that are also i2p
            if( fReachable && (!IsI2POnly() || addr.IsI2P()) )
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);         //! Add the vector of addresses to our book, marking them as 2hrs old
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }


    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            Misbehaving(pfrom->GetId(), 20);
            return error("message inv size() = %u", vInv.size());
        }

        LOCK(cs_main);

        std::vector<CInv> vToFetch;

        uint8_t nTxOrphansInPayload = 0;
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            boost::this_thread::interruption_point();
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(inv);
            LogPrint("net", "  got inventory: %s  %s\n", inv.ToString(), fAlreadyHave ? "have" : "new");
            if( inv.type != MSG_BLOCK ) {
                //! An additional check for transaction inventory. If we are not up-to-date with current
                //! blocks, do NOT ask for tx inventory...
                //! Otherwise we get a bunch of transaction orphans that would otherwise have been known about
                //! soon enough when the blocks arrive....
                if( !fAlreadyHave && !IsInitialBlockDownload() ) {
                    //! One last check, when found we must do something, there are good peers propagating bad Tx's too,
                    //! so we do one important step for sure.  We do NOT AskFor from that peer the Tx and stop
                    //! the propagation of the bad Inv messages as one of protection steps.  Until everyone upgrades we
                    //! can not mark the peer as misbehaving and disconnect.  Plus consideration of gateways between
                    //! I2P and clearnet maybe the source, all good traffic would also be cut off, so Misbehaving here
                    //! is not easy to know what action maybe best regarding this peer.
                    if( fTxIndex ) {
                        // LogPrintf( "Double checking TxIndex for hash %s\n", inv.hash.ToString() );
                        CDiskTxPos postx;
                        if( pblocktree->ReadTxIndex(inv.hash, postx) ) {
                            LogPrintf( "WARNING - Tx hash found, likely old pruned coins in blockchain. Double spend attack?\n" );
                            Misbehaving(pfrom->GetId(), 1);
                            fAlreadyHave = true;
                            if( ++nTxOrphansInPayload == 100 ) {
                                LogPrintf( "Dumping the remaining %d transactions in this inventory...\n", vInv.size() - nTxOrphansInPayload );
                                break;
                            }

                        } // else
                            // LogPrintf( "Inventory hash not found in TxIndex.\n");
                    } // else
                        // LogPrintf( "Need TxIndex enabled to ensure this node is protected from double spend transaction attacks.\n");

                    if( !fAlreadyHave )
                        pfrom->AskFor(inv);
                }
            } else {                        // inv.type == MSG_BLOCK
                UpdateBlockAvailability(pfrom->GetId(), inv.hash);
                if (!fAlreadyHave && !fImporting && !fReindex && !IsInitialBlockDownload() && !mapBlocksInFlight.count(inv.hash)) {
                    /* Cslave: !IsInitialBlockDownload() is needed otherwise it start to download the same headers several time per peer and from all peers, wasting a lot of computing time and bandwidth */
                    
                    // First request the headers preceeding the announced block. In the normal fully-synced
                    // case where a new block is announced that succeeds the current tip (no reorganization),
                    // there are no such headers.
                    // Secondly, and only when we are close to being synced, we request the announced block directly,
                    // to avoid an extra round-trip. Note that we must *first* ask for the headers, so by the
                    // time the block arrives, the header chain leading up to it is already validated. Not
                    // doing this will result in the received block being rejected as an orphan in case it is
                    // not a direct successor.
                    pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexBestHeader), inv.hash);
                    CNodeState *nodestate = State(pfrom->GetId());
                    if (chainActive.Tip()->GetBlockTime() > GetAdjustedTime() - nTargetSpacing * 20 &&
                        nodestate->nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
                        vToFetch.push_back(inv);
                        // Mark block as in flight already, even though the actual "getdata" message only goes out
                        // later (within the same cs_main lock, though).
                        MarkBlockAsInFlight(pfrom->GetId(), inv.hash);
                    }
                    LogPrint("net", "getheaders (%d) %s to %s\n", pindexBestHeader->nHeight, inv.hash.ToString(), GetPeerLogStr(pfrom));
                }
            }

            // Track requests for our stuff
            g_signals.Inventory(inv.hash);

            if (pfrom->nSendSize > (SendBufferSize() * 2)) {
                Misbehaving(pfrom->GetId(), 50);
                return error("send buffer size() = %u", pfrom->nSendSize);
            }
        }

        if (!vToFetch.empty())
            pfrom->PushMessage("getdata", vToFetch);
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        int32_t nInvSize = vInv.size();
        if( nInvSize > MAX_INV_SZ ) {
            Misbehaving(pfrom->GetId(), 20);
            return error("message getdata size() = %u", nInvSize);
        }

        if( nInvSize != 1 )
            LogPrint("net", "received getdata (%u invsz) from %s\n", nInvSize, GetPeerLogStr(pfrom));
        else
            LogPrint("net", "received getdata for: %s from %s\n", vInv[0].ToString(), GetPeerLogStr(pfrom));

        pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
        ProcessGetData(pfrom);
    }


    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uintFakeHash hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = FindForkInGlobalIndex(chainActive, locator);

        // Send the rest of the chain
        if (pindex)
            pindex = chainActive.Next(pindex);
        int nLimit = 500;
        LogPrint("net", "getblocks %d to %s limit %d\n", (pindex ? pindex->nHeight : -1), hashStop.GetRealHash().ToString(), nLimit);
        for (; pindex; pindex = chainActive.Next(pindex))
        {
            if (pindex->GetBlockSha256dHash() == hashStop)
            {
                LogPrint("net", "  getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockSha256dHash()));
            if (--nLimit <= 0)
            {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                LogPrint("net", "  getblocks stopping at limit %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                pfrom->hashContinue = pindex->GetBlockSha256dHash();
                break;
            }
        }
    }


    else if (strCommand == "getheaders")
    {
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION_AFTER_HF2 && chainActive.Height() > HARDFORK_BLOCK2) {
            LogPrintf("getheaders partner %s using version %i obsolete since the HARDFORK_BLOCK %d changing PID parameters was reached; disconnecting\n", pfrom->addr.ToString(), pfrom->nVersion, HARDFORK_BLOCK2);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
            strprintf("ERROR: Version must be %d or greater after the HARDFORK_BLOCK %d changing PID parameters, please update!", MIN_PEER_PROTO_VERSION_AFTER_HF, HARDFORK_BLOCK2));
            pfrom->fDisconnect = true;
        }

        CBlockLocator locator;
        uintFakeHash hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        if (IsInitialBlockDownload()) {
            LogPrint("net", "Ignoring getheaders from peer=%d because node is in initial block download\n", pfrom->id);
            return true;
        }

        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            uint256 aRealHash = hashStop.GetRealHash();
            BlockMap::iterator mi = ( aRealHash != 0 ) ? mapBlockIndex.find( aRealHash ) : mapBlockIndex.end();
            if (mi == mapBlockIndex.end())
                return true;
            pindex = (*mi).second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = FindForkInGlobalIndex(chainActive, locator);
            if (pindex)
                pindex = chainActive.Next(pindex);
        }

        // we must use CBlocks, as CBlockHeaders won't include the 0x00 nTx count at the end
        vector<CBlock> vHeaders;
        int nLimit = MAX_HEADERS_RESULTS;
        LogPrint("net", "getheaders %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString());
        for (; pindex; pindex = chainActive.Next(pindex))
        {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockSha256dHash() == hashStop)
                break;
        }
        pfrom->PushMessage("headers", vHeaders);
    }


    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        LOCK(cs_main);

        bool fMissingInputs = false;
        CValidationState state;

        mapAlreadyAskedFor.erase(inv);

        if (AcceptToMemoryPool(mempool, state, tx, true, &fMissingInputs))
        {
            mempool.check(pcoinsTip);
            RelayTransaction(tx);
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);

            LogPrint("mempool", "AcceptToMemoryPool: %s %s : accepted %s (poolsz %u)\n",
                GetPeerLogStr(pfrom), pfrom->cleanSubVer,
                tx.GetHash().ToString(),
                mempool.mapTx.size());

            // Recursively process any orphan transactions that depended on this one
            set<NodeId> setMisbehaving;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                map<uint256, set<uint256> >::iterator itByPrev = mapOrphanTransactionsByPrev.find(vWorkQueue[i]);
                if (itByPrev == mapOrphanTransactionsByPrev.end())
                    continue;
                for (set<uint256>::iterator mi = itByPrev->second.begin();
                     mi != itByPrev->second.end();
                     ++mi)
                {
                    const uint256& orphanHash = *mi;
                    const CTransaction& orphanTx = mapOrphanTransactions[orphanHash].tx;
                    NodeId fromPeer = mapOrphanTransactions[orphanHash].fromPeer;
                    bool fMissingInputs2 = false;
                    // Use a dummy CValidationState so someone can't setup nodes to counter-DoS based on orphan
                    // resolution (that is, feeding people an invalid transaction based on LegitTxX in order to get
                    // anyone relaying LegitTxX banned)
                    CValidationState stateDummy;

                    vEraseQueue.push_back(orphanHash);

                    if (setMisbehaving.count(fromPeer))
                        continue;
                    if (AcceptToMemoryPool(mempool, stateDummy, orphanTx, true, &fMissingInputs2))
                    {
                        LogPrint("mempool", "   accepted orphan tx %s\n", orphanHash.ToString());
                        RelayTransaction(orphanTx);
                        vWorkQueue.push_back(orphanHash);
                    }
                    else if (!fMissingInputs2)
                    {
                        int nDos = 0;
                        if (stateDummy.IsInvalid(nDos) && nDos > 0)
                        {
                            // Punish peer that gave us an invalid orphan tx
                            Misbehaving(fromPeer, nDos);
                            setMisbehaving.insert(fromPeer);
                            LogPrint("mempool", "   invalid orphan tx %s\n", orphanHash.ToString());
                        }
                        // too-little-fee orphan
                        LogPrint("mempool", "   removed orphan tx %s\n", orphanHash.ToString());
                    }
                    mempool.check(pcoinsTip);
                }
            }

            BOOST_FOREACH(uint256 hash, vEraseQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            AddOrphanTx(tx, pfrom->GetId());

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nMaxOrphanTx = (unsigned int)std::max((int64_t)0, GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
            unsigned int nEvicted = LimitOrphanTxSize(nMaxOrphanTx);
            if (nEvicted > 0)
                LogPrint("mempool", "mapOrphan overflow, removed %u tx\n", nEvicted);
        } else if (pfrom->fWhitelisted) {
            // Always relay transactions received from whitelisted peers, even
            // if they are already in the mempool (allowing the node to function
            // as a gateway for nodes hidden behind it).
            RelayTransaction(tx);
        }
        int nDoS = 0;
        if (state.IsInvalid(nDoS))
        {
            LogPrint("mempool", "%s from %s %s was not accepted into the memory pool: %s\n", tx.GetHash().ToString(),
                GetPeerLogStr(pfrom), pfrom->cleanSubVer,
                state.GetRejectReason());
            pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                               state.GetRejectReason(), inv.hash);
            if (nDoS > 0)
                Misbehaving(pfrom->GetId(), nDoS);
        }
    }

    else if (strCommand == "headers" && !fImporting && !fReindex) // Ignore headers received while importing
    {
        std::vector<CBlockHeader> headers;

        // Bypass the normal CBlock deserialization, as we don't want to risk deserializing 2000 full blocks.
        unsigned int nCount = ReadCompactSize(vRecv);
        if (nCount > MAX_HEADERS_RESULTS) {
            Misbehaving(pfrom->GetId(), 20);
            return error("headers message size = %u", nCount);
        }
        headers.resize(nCount);
        for (unsigned int n = 0; n < nCount; n++) {
            vRecv >> headers[n];
            ReadCompactSize(vRecv); // ignore tx count; assume it is 0.
        }

        LOCK(cs_main);

        if (nCount == 0) {
            // Nothing interesting. Stop asking this peers for more headers.
            return true;
        }

        CBlockIndex *pindexLast = NULL;
        BOOST_FOREACH(const CBlockHeader& header, headers) {
            CValidationState state;
            uint256 aRealHash = header.hashPrevBlock.GetRealHash();
            if (pindexLast != NULL && aRealHash != 0 && aRealHash != pindexLast->GetBlockHash()) {
                Misbehaving(pfrom->GetId(), 20);
                return error("non-continuous headers sequence");
            }
            if (!AcceptBlockHeader(header, state, &pindexLast)) {         
                int nDoS;
                if (state.IsInvalid(nDoS)) {          
                    if (pfrom->nVersion < MIN_PEER_PROTO_VERSION_AFTER_HF2 && chainActive.Height() > HARDFORK_BLOCK2)
                        nDoS = 100;
                    if (nDoS > 0)
                        Misbehaving(pfrom->GetId(), nDoS);
                    return error("invalid header received");
                }
            }
        }

        if (pindexLast)
            UpdateBlockAvailability(pfrom->GetId(), pindexLast->GetBlockSha256dHash());

        if (nCount == MAX_HEADERS_RESULTS && pindexLast) {
            // Headers message had its maximum size; the peer may have more headers.
            // TODO: optimize: if pindexLast is an ancestor of chainActive.Tip or pindexBestHeader, continue
            // from there instead.
            // For the HardFork we will not accept to synchronize with old version peers 2400 blocks prior to the hardfork.
#if defined( HARDFORK_BLOCK )
            if( pfrom->nVersion >= MIN_PEER_PROTO_VERSION_AFTER_HF2 || pfrom->nStartingHeight < HARDFORK_BLOCK2 - 2400) {                
#endif                    
                LogPrint("net", "more getheaders (%d) to end to %s (startheight:%d)\n", pindexLast->nHeight, GetPeerLogStr(pfrom), pfrom->nStartingHeight);
                pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexLast), uint256(0));
#if defined( HARDFORK_BLOCK )      
            }
#endif
        }

    }


    else if (strCommand == "block" && !fImporting && !fReindex) // Ignore blocks received while importing
    {
        CBlock block;
        vRecv >> block;

        CInv inv(MSG_BLOCK, block.CalcSha256dHash());
        LogPrint("net", "received block %s %s\n", block.GetHash().ToString(), GetPeerLogStr(pfrom));
        // LogPrint("net", "received block %s\n", block.GetHash().ToString());
        // block.print();

        pfrom->AddInventoryKnown(inv);

        CValidationState state;
        ProcessNewBlock(state, pfrom, &block);
        int nDoS;
        if (state.IsInvalid(nDoS)) {
            pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                               state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
            if (nDoS > 0) {
                LOCK(cs_main);
                Misbehaving(pfrom->GetId(), nDoS);
            }
        }
    }


    // This asymmetric behavior for inbound and outbound connections was introduced
    // to prevent a fingerprinting attack: an attacker can send specific fake addresses
    // to users' AddrMan and later request them by sending getaddr messages.
    // Making users (which are behind NAT and can only make outgoing connections) ignore
    // getaddr message mitigates the attack.
   // else if ((strCommand == "getaddr") && (pfrom->fInbound))
    
    else if (strCommand == "getaddr")
    {
        if (pfrom->nVersion < MIN_PEER_PROTO_VERSION_AFTER_HF2 && chainActive.Height() > HARDFORK_BLOCK2) {
            LogPrintf("getaddr partner %s using version %i obsolete since the HARDFORK_BLOCK %d changing PID parameters was reached; disconnecting\n", pfrom->addr.ToString(), pfrom->nVersion, HARDFORK_BLOCK2);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
            strprintf("ERROR: Version must be %d or greater after the HARDFORK_BLOCK %d changing PID parameters, please update!", MIN_PEER_PROTO_VERSION_AFTER_HF, HARDFORK_BLOCK2));
            pfrom->fDisconnect = true;
        }

        LogPrint("addrman", "addrman: getaddr received from %s (startheight:%d) nVersion %d \n", GetPeerLogStr(pfrom), pfrom->nStartingHeight, pfrom->nVersion);
        pfrom->vAddrToSend.clear();
        bool fIpOnly = (pfrom->addr.nServices & NODE_I2P) != 0;
        bool fI2pOnly = pfrom->addr.IsI2P();
        vector<CAddress> vAddr = addrman.GetAddr( fIpOnly, fI2pOnly );
        BOOST_FOREACH(const CAddress &addr, vAddr){
        LogPrint("addrman", "addrman: getaddr received, address sent to %s \n", GetPeerLogStr(pfrom)); 
        addr.print ();
        pfrom->PushAddress(addr);}
    }


    else if (strCommand == "mempool")
    {
        LOCK2(cs_main, pfrom->cs_filter);

        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        BOOST_FOREACH(uint256& hash, vtxid) {
            CInv inv(MSG_TX, hash);
            CTransaction tx;
            bool fInMemPool = mempool.lookup(hash, tx);
            if (!fInMemPool) continue; // another thread removed since queryHashes, maybe...
            if ((pfrom->pfilter && pfrom->pfilter->IsRelevantAndUpdate(tx)) ||
               (!pfrom->pfilter))
                vInv.push_back(inv);
            if (vInv.size() == MAX_INV_SZ) {
                pfrom->PushMessage("inv", vInv);
                vInv.clear();
            }
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }


    else if (strCommand == "ping")
    {
        uint64_t nonce = 0;
        vRecv >> nonce;
        // Echo the message back with the nonce. This allows for two useful features:
        //
        // 1) A remote node can quickly check if the connection is operational
        // 2) Remote nodes can measure the latency of the network thread. If this node
        //    is overloaded it won't respond to pings quickly and the remote node can
        //    avoid sending us more work, like chain download requests.
        //
        // The nonce stops the remote getting confused between different pings: without
        // it, if the remote node sends a ping once per second and this node takes 5
        // seconds to respond to each, the 5th ping the remote sends would appear to
        // return very quickly.
        pfrom->PushMessage("pong", nonce);
    }


    else if (strCommand == "pong")
    {
        int64_t pingUsecEnd = GetTimeMicros();
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
                        // Successful ping time measurement, replace previous
                        pfrom->nPingUsecTime = pingUsecTime;
                    } else {
                        // This should never happen
                        sProblem = "Timing mishap";
                    }
                } else {
                    // Nonce mismatches are normal when pings are overlapping
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
                        // This is most likely a bug in another implementation somewhere, cancel this ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
            // This is most likely a bug in another implementation somewhere, cancel this ping
            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            LogPrint("net", "pong %s %s: %s, %x expected, %x received, %u bytes\n",
                GetPeerLogStr(pfrom),
                pfrom->cleanSubVer,
                sProblem,
                pfrom->nPingNonceSent,
                nonce,
                nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }


    else if (strCommand == "alert")
    {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0)
        {
            if (alert.ProcessAlert())
            {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                        alert.RelayTo(pnode);
                }
            }
            else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                Misbehaving(pfrom->GetId(), 10);
            }
        }
    }


    else if (strCommand == "filterload")
    {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints())
            // There is no excuse for sending a too-large filter
            Misbehaving(pfrom->GetId(), 100);
        else
        {
            LOCK(pfrom->cs_filter);
            delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter(filter);
            pfrom->pfilter->UpdateEmptyFull();
        }
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "filteradd")
    {
        vector<uint8_t> vData;
        vRecv >> vData;

        // Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
        // and thus, the maximum size any matched object can have) in a filteradd message
        if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE)
        {
            Misbehaving(pfrom->GetId(), 100);
        } else {
            LOCK(pfrom->cs_filter);
            if (pfrom->pfilter)
                pfrom->pfilter->insert(vData);
            else
                Misbehaving(pfrom->GetId(), 100);
        }
    }


    else if (strCommand == "filterclear")
    {
        LOCK(pfrom->cs_filter);
        delete pfrom->pfilter;
        pfrom->pfilter = new CBloomFilter();
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "reject")
    {
        try {
            string strMsg; uint8_t ccode; string strReason;
            vRecv >> LIMITED_STRING(strMsg, CMessageHeader::COMMAND_SIZE) >> ccode >> LIMITED_STRING(strReason, 111);

            ostringstream ss;
            ss << strMsg << " code " << itostr(ccode) << ": " << strReason;

            if (strMsg == "block" || strMsg == "tx")
            {
                uint256 hash;
                vRecv >> hash;
                ss << ": hash " << hash.ToString();
            }
            LogPrint("net", "Reject %s from %s\n", SanitizeString(ss.str()), GetPeerLogStr(pfrom));
        } catch (const std::ios_base::failure& e) {
            // Avoid feedback loops by preventing reject messages from triggering a new reject message.
            LogPrint("net", "Unparseable reject message received from %s\n", GetPeerLogStr(pfrom));
        }
    }

    else
    {
        // Ignore unknown commands for extensibility
        LogPrint("net", "Unknown command \"%s\" from %s\n", SanitizeString(strCommand), GetPeerLogStr(pfrom));
    }

    return true;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
    //if (fDebug)
    //    LogPrintf("ProcessMessages(%u messages)\n", pfrom->vRecvMsg.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    if (!pfrom->vRecvGetData.empty())
        ProcessGetData(pfrom);

    // this maintains the order of responses
    if (!pfrom->vRecvGetData.empty()) return fOk;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        //if (fDebug)
        //    LogPrintf("ProcessMessages(message %u msgsz, %u bytes, complete:%s)\n",
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, Params().MessageStart(), MESSAGE_START_SIZE) != 0) {
            LogPrintf("\n\nPROCESSMESSAGE: INVALID MESSAGESTART\n\n");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid())
        {
            LogPrintf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand());
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum)
        {
            LogPrintf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x from %s\n",
               strCommand, nMessageSize, nChecksum, hdr.nChecksum, GetPeerLogStr(pfrom));
            continue;
        }

        // Process message
        bool fRet = false;
        try
        {
            fRet = ProcessMessage(pfrom, strCommand, vRecv);
            boost::this_thread::interruption_point();
        }
        catch (const std::ios_base::failure& e)
        {
            pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED, string("error parsing message"));
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length, from %s\n", strCommand, nMessageSize, e.what(), GetPeerLogStr(pfrom));
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                LogPrintf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, from %s\n", strCommand, nMessageSize, e.what(), GetPeerLogStr(pfrom));
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (const boost::thread_interrupted&) {
            throw;
        }
        catch (const std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            LogPrintf("ProcessMessage(%s, %u bytes) FAILED, from %s\n", strCommand, nMessageSize, GetPeerLogStr(pfrom));

        break;
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
    {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        //
        // Message: ping
        //
        bool pingSend = false;
        if (pto->fPingQueued) {
            // RPC ping request by user
            pingSend = true;
        }
        if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) {
            // Ping automatically sent as a latency probe & keepalive.
            pingSend = true;
        }
        if (pingSend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                GetRandBytes((uint8_t*)&nonce, sizeof(nonce));
            }
            pto->fPingQueued = false;
            pto->nPingUsecStart = GetTimeMicros();
            pto->nPingNonceSent = nonce;
            pto->PushMessage("ping", nonce);
        }

        TRY_LOCK(cs_main, lockMain); // Acquire cs_main for IsInitialBlockDownload() and CNodeState()
        if (!lockMain)
            return true;

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60))
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                // Periodically clear addrKnown to allow refresh broadcasts
                if (nLastRebroadcast)
                    pnode->addrKnown.clear();

                // Rebroadcast our address
                AdvertizeLocal(pnode);
            }
            if (!vNodes.empty())
                nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH(const CAddress& addr, pto->vAddrToSend)
            {
                if (!pto->addrKnown.contains(addr.GetKey()))
                 {
                    pto->addrKnown.insert(addr.GetKey());
                     vAddr.push_back(addr);
                    //! I2P addresses are MUCH larger than IP addresses, a trickle set to 1K is over 1/2 megabyte of payload
                    //! over 33x larger per addr, so lets reduce that amount, down to what the max addrman will return
                    //! or 1000, whichever is less.  Any more than 1K, and various nodes will start marking ours as misbehaving.
                    //! This change however does not stop addrman from generating what is still a very huge list and payload to
                    //! send, it just breaks it up into smaller chunks.  See the variable ADDRMAN_GETADDR_MAX as defined in
                    //! addrman.h for what that value is set to, and more details.
                    //! Also see: The 'getaddr' message processing for details on restricting the results based on the nodes
                    //! network and service settings.
                    //! was if (vAddr.size() >= 1000)
#if ADDRMAN_GETADDR_MAX < 1000
                    if( vAddr.size() >= ADDRMAN_GETADDR_MAX )
#else
                    if( vAddr.size() >= 1000 )
#endif
                    {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }

        CNodeState &state = *State(pto->GetId());
        if (state.fShouldBan) {
            if (pto->fWhitelisted)
                LogPrintf("Warning: not punishing whitelisted %s!\n", GetPeerLogStr(pto));
            else {
                pto->fDisconnect = true;
                if (pto->addr.IsLocal())
                    LogPrintf("Warning: not banning local %s!\n", GetPeerLogStr(pto));
                else
                {
                    CNode::Ban(pto->addr);
                }
                state.fShouldBan = false;
            }
        }

        BOOST_FOREACH(const CBlockReject& reject, state.rejects)
            pto->PushMessage("reject", (string)"block", reject.chRejectCode, reject.strRejectReason, reject.hashBlock);
        state.rejects.clear();

        // Start block sync
        if (pindexBestHeader == NULL)
            pindexBestHeader = chainActive.Tip();
        // Download if this is a nice peer, or we have no nice peers and this one might do.
        bool fFetch = state.fPreferredDownload || (nPreferredDownload == 0 && !pto->fClient && !pto->fOneShot);
        if (!state.fSyncStarted && !pto->fClient && !fImporting && !fReindex) {
            // Only actively request headers from up to two peers, unless we're close to today. This is to minimize the likelihood of downtime, while keeping bandwidth and computing ressource to a minimum. For the hardfork, we accept IBD from all peers till 2400 blocks before the Hardfork block when the new version will be enforced for synching. This ensure that there is no synching from peers not ready for the hardfork from this time on. Of course, after the hardfork block the new version will still be an obligation.
            if ( fFetch || pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 24 * 60 * 60) {
#if defined( HARDFORK_BLOCK )
                if( pto->nVersion >= MIN_PEER_PROTO_VERSION_AFTER_HF2 || !IsInitialBlockDownload() || pto->nStartingHeight < HARDFORK_BLOCK2 - 2400) {
#else
                if( pto->nVersion >= MIN_PEER_PROTO_VERSION || !IsInitialBlockDownload()) {
#endif
                    if ((nSyncStarted <= 1) || pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 24 * 60 * 60) {
                    state.fSyncStarted = true;
                    nSyncStarted++;                
                    CBlockIndex *pindexStart = pindexBestHeader->pprev ? pindexBestHeader->pprev : pindexBestHeader;
                    LogPrint("net", "initial getheaders (%d) to %s (startheight:%d)\n", pindexStart->nHeight, GetPeerLogStr(pto), pto->nStartingHeight);
                    pto->PushMessage("getheaders", chainActive.GetLocator(pindexStart), uint256(0));
                    }
                }
            }  
        }     
       

        // Resend wallet transactions that haven't gotten in a block yet
        // Except during reindex, importing and IBD, when old wallet
        // transactions become unconfirmed and spams other nodes.
        if (!fReindex && !fImporting && !IsInitialBlockDownload())
        {
            g_signals.Broadcast();
        }

        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH(const CInv& inv, pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint256 hashRand = inv.hash ^ hashSalt;
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    if (fTrickleWait)
                    {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);


        // Detect stalled peers. Require that blocks are in flight, we haven't
        // received a (requested) block in one minute, and that all blocks are
        // in flight for over two minutes, since we first had a chance to
        // process an incoming block.
        int64_t nNow = GetTimeMicros();
        if (!pto->fDisconnect && state.nStallingSince && state.nStallingSince < nNow - 1000000 * BLOCK_STALLING_TIMEOUT) {
            // Stalling only triggers when the block download window cannot move. During normal steady state,
            // the download window should be much larger than the to-be-downloaded set of blocks, so disconnection
            // should only happen during initial block download.
            LogPrintf("%s is stalling block download, disconnecting\n", GetPeerLogStr(pto));
            pto->fDisconnect = true;
        }
        // In case there is a block that has been in flight from this peer for (2 + 0.5 * N) times the block interval
        // (with N the number of validated blocks that were in flight at the time it was requested), disconnect due to
        // timeout. We compensate for in-flight blocks to prevent killing off peers due to our own downstream link
        // being saturated. We only count validated in-flight blocks so peers can't advertize nonexisting block hashes
        // to unreasonably increase our timeout.
        if (!pto->fDisconnect && state.vBlocksInFlight.size() > 0 && state.vBlocksInFlight.front().nTime < nNow - 500000 * nTargetSpacing * (4 + state.vBlocksInFlight.front().nValidatedQueuedBefore)) {
            LogPrintf("Timeout downloading block %s from %s, disconnecting\n", state.vBlocksInFlight.front().hash.ToString(), GetPeerLogStr(pto));
            pto->fDisconnect = true;
        }

        //
        // Message: getdata (blocks)
        //
        vector<CInv> vGetData;
       if (!pto->fDisconnect && !pto->fClient && (fFetch || !IsInitialBlockDownload()) && state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
            vector<CBlockIndex*> vToDownload;
            NodeId staller = -1;
            FindNextBlocksToDownload(pto->GetId(), MAX_BLOCKS_IN_TRANSIT_PER_PEER - state.nBlocksInFlight, vToDownload, staller);
            BOOST_FOREACH(CBlockIndex *pindex, vToDownload) {
                vGetData.push_back(CInv(MSG_BLOCK, pindex->GetBlockSha256dHash()));
                MarkBlockAsInFlight(pto->GetId(), pindex->GetBlockSha256dHash(), pindex);
                LogPrint("net", "Requesting block %s (%d) from %s\n", pindex->GetBlockHash().ToString(), pindex->nHeight, GetPeerLogStr(pto));
            }
            if (state.nBlocksInFlight == 0 && staller != -1) {
                if (State(staller)->nStallingSince == 0) {
                    State(staller)->nStallingSince = nNow;
                    LogPrint("net", "Stall started peer=%d\n", staller);
                }
            }
        }

        //
        // Message: getdata (non-blocks)
        //
        while (!pto->fDisconnect && !pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv))
            {
                LogPrint("net", "Requesting %s from %s\n", inv.ToString(), GetPeerLogStr(pto));
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);

    }
    return true;
}


bool CBlockUndo::WriteToDisk(CDiskBlockPos &pos, const uint256 &hashBlock)
{
    // Open history file to append
    CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("CBlockUndo::WriteToDisk : OpenUndoFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(*this);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write undo data
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("CBlockUndo::WriteToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << *this;

    // calculate & write checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    fileout << hasher.GetHash();

    return true;
}

bool CBlockUndo::ReadFromDisk(const CDiskBlockPos &pos, const uint256 &hashBlock)
{
    // Open history file to read
    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("CBlockUndo::ReadFromDisk : OpenBlockFile failed");

    // Read block
    uint256 hashChecksum;
    try {
        filein >> *this;
        filein >> hashChecksum;
    }
    catch (const std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Verify checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    if (hashChecksum != hasher.GetHash())
        return error("CBlockUndo::ReadFromDisk : Checksum mismatch");

    return true;
}

 std::string CBlockFileInfo::ToString() const {
     return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
 }

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup() {
        // This is far from complete, proper cleanup is a big job, the OS had best take care of it
        // ToDo: Detail everything in main the could be taking up memory and specifically deallocate it.

        // block headers
        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();
        // cross reference block hash map
        mapBlockHashCrossReference.clear();

        // orphan transactions
        mapOrphanTransactions.clear();
        mapOrphanTransactionsByPrev.clear();

    }
} instance_of_cmaincleanup;

// v9 code needs these defined for fee calculations in order to compile and link wallet.cpp and coincontroldialog+other options for QT
int64_t GetMinFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree, enum GetMinFee_mode mode)
{
    // Base fee is either nMinTxFee or nMinRelayTxFee
    int64_t nBaseFee = (mode == GMF_RELAY) ? tx_nMinRelayTxFee : tx_nMinTxFee;

    int64_t nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    if (fAllowFree)
    {
        // There is a free transaction area in blocks created by most miners,
        // * If we are relaying we allow transactions up to DEFAULT_BLOCK_PRIORITY_SIZE - 1000
        //   to be considered to fall into this category. We don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        // * If we are creating a transaction we allow transactions up to 1,000 bytes
        //   to be considered safe and assume they can likely make it into this section.
        if (nBytes < (mode == GMF_SEND ? 5000 : (DEFAULT_BLOCK_PRIORITY_SIZE - 1000)))
            nMinFee = 0;
    }

    // <This code was ported> from Anoncoin v0.8.5.6, claims originally came from Litecoin
    // To limit dust spam, add nBaseFee for each output smaller than DUST_SOFT_LIMIT
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
        if (txout.nValue < DUST_SOFT_LIMIT)
            nMinFee += nBaseFee;

    // Raise the price as the block approaches full
    // In the old code nBlockSize is known, and set to 1, except for mode GMF_RELAY, where
    // the value is set to 1000.  Here in v9 we don't have that nBlockSize value given as
    // a parameter, only nBytes. A new DEFINE'd variable in v9 is now used, called MAX_BLOCK_SIZE_GEN
    // this also came from the old client, and can be found main.h.  So as we know
    // the mode, we can run the same nMinFee calculations that was being used before,
    // and have this function made to work, as was intended.
    // Original code: unsigned int nNewBlockSize = nBlockSize + nBytes;
    unsigned int nNewBlockSize = 1000 + nBytes;
    // Original code: if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2)
    if( (mode == GMF_RELAY) && (nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2) )
    {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return MAX_MONEY;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }
    // ...end <this code was ported>

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

