// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
// anoncoin-config.h loaded...

#include "checkpoints.h"
#include "main.h"
#include "sync.h"
#include "util.h"

#include "chainparams.h"
#include "consensus.h"

#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <stdint.h>

#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;

extern void TxToJSON(const CTransaction& tx, const uintFakeHash hashBlock, Object& entry);
void ScriptPubKeyToJSON(const CScript& scriptPubKey, Object& out, bool fIncludeHex);

//! Returns the floating point number that represents a multiple of the minimum
//! difficulty that was required, and found within the given block index entries
//! nBits field.  If the block index pointer given is NULL, then we use the active
//! chain tip block index as the one for which this evaluation is made.
//! minimum difficulty = 1.0, and the value goes down when difficulty is harder,
//! and up when the difficulty is easier.
double GetDifficulty(const CBlockIndex* blockindex)
{
    if (blockindex == NULL)
    {
        if (chainActive.Tip() == NULL)
            return 1.0;
        else
            blockindex = chainActive.Tip();
    }
    uint256 uintBlockDiff;
    uintBlockDiff.SetCompact( blockindex->nBits );
    if (ancConsensus.IsUsingGost3411Hash()) {
        return GetLinearWork( uintBlockDiff, Params().ProofOfWorkLimit( CChainParams::ALGO_GOST3411 ) );
    } else {
        return GetLinearWork( uintBlockDiff, Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ) );
    }
}


Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false)
{
    Object result;
    result.push_back(Pair("hash", blockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("shad", blockindex->GetBlockSha256dHash().GetHex()));
    result.push_back(Pair("gost", blockindex->GetBlockGost3411Hash().GetHex()));
    int confirmations = -1;
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    Array txs;
    BOOST_FOREACH(const CTransaction&tx, block.vtx)
    {
        if(txDetails)
        {
            Object objTx;
            TxToJSON(tx, uint256(0), objTx);
            txs.push_back(objTx);
        }
        else
            txs.push_back(tx.GetHash().GetHex());
    }
    result.push_back(Pair("tx", txs));
    result.push_back(Pair("time", block.GetBlockTime()));
    result.push_back(Pair("nonce", (uint64_t)block.nNonce));
    result.push_back(Pair("bits", HexBits(block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    return result;
}


Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0) {
        string sTranslateText = _(
            "\nReturns the number of blocks in the longest block chain.\n"
            "\nResult:\n"
            "n    (numeric) The current block count\n"
            "\nExamples:\n"
        );
        throw runtime_error(
            "getblockcount\n"
            + sTranslateText
//            "\nReturns the number of blocks in the longest block chain.\n"
//            "\nResult:\n"
//            "n    (numeric) The current block count\n"
//            "\nExamples:\n"
            + HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
        );
    }

    LOCK(cs_main);
    return chainActive.Height();
}

Value getbestblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "\nReturns the hash of the best (tip) block in the longest block chain.\n"
            "\nResult\n"
            "\"hex\"      (string) the block hash hex encoded\n"
            "\nExamples\n"
            + HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
        );

    LOCK(cs_main);
    return chainActive.Tip()->GetBlockHash().GetHex();
}

Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "\nReturns the proof-of-work required difficulty now at the tip of the block chain.\n"
            "\nResult:\n"
            "n.nnn       (numeric) The minimum difficulty is defined as 1 and this result is linear relative to that value.\n"
            "            Smaller values indicate harder, larger an easier difficulty and all blocks will have a value < 1.\n"
            "\nExamples:\n"
            + HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
        );

    LOCK(cs_main);
    return GetDifficulty();
}


Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getrawmempool [verbose]\n"
            "\n Returns all transaction ids in memory pool as a json array of string transaction ids.\n"
            "\nArguments:\n"
            " 1. verbose                 (boolean, optional, default=false) true for a json object, false for array of transaction ids\n"
            "\nResult: (for verbose = false)\n"
            "[                           (json array of string)\n"
            "  \"transactionid\"           (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nResult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            "    \"size\" : n,             (numeric) transaction size in bytes\n"
            "    \"fee\" : n,              (numeric) transaction fee in anoncoins\n"
            "    \"time\" : n,             (numeric) local time transaction entered pool in seconds since 1 Jan 1970 GMT\n"
            "    \"height\" : n,           (numeric) block height when transaction entered pool\n"
            "    \"startingpriority\" : n, (numeric) priority when transaction entered pool\n"
            "    \"currentpriority\" : n,  (numeric) transaction priority now\n"
            "    \"depends\" : [           (array) unconfirmed transactions used as inputs for this transaction\n"
            "        \"transactionid\",    (string) parent transaction id\n"
            "       ... ]\n"
            "  }, ...\n"
            "]\n"
            "\nExamples\n"
            + HelpExampleCli("getrawmempool", "true")
            + HelpExampleRpc("getrawmempool", "true")
        );

    LOCK(cs_main);

    bool fVerbose = false;
    if (params.size() > 0)
        fVerbose = params[0].get_bool();

    if (fVerbose)
    {
        LOCK(mempool.cs);
        Object o;
        BOOST_FOREACH(const PAIRTYPE(uint256, CTxMemPoolEntry)& entry, mempool.mapTx)
        {
            const uint256& hash = entry.first;
            const CTxMemPoolEntry& e = entry.second;
            Object info;
            info.push_back(Pair("size", (int)e.GetTxSize()));
            info.push_back(Pair("fee", ValueFromAmount(e.GetFee())));
            info.push_back(Pair("time", e.GetTime()));
            info.push_back(Pair("height", (int)e.GetHeight()));
            info.push_back(Pair("startingpriority", e.GetPriority(e.GetHeight())));
            info.push_back(Pair("currentpriority", e.GetPriority(chainActive.Height())));
            const CTransaction& tx = e.GetTx();
            set<string> setDepends;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                if (mempool.exists(txin.prevout.hash))
                    setDepends.insert(txin.prevout.hash.ToString());
            }
            Array depends(setDepends.begin(), setDepends.end());
            info.push_back(Pair("depends", depends));
            o.push_back(Pair(hash.ToString(), info));
        }
        return o;
    }
    else
    {
        vector<uint256> vtxid;
        mempool.queryHashes(vtxid);

        Array a;
        BOOST_FOREACH(const uint256& hash, vtxid)
            a.push_back(hash.ToString());

        return a;
    }
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "\nReturns hash of block in best-block-chain at index provided.\n"
            "\nArguments:\n"
            "1. index           (numeric, required) The block index\n"
            "\nResult:\n"
            "{\n"
            "  \"hash\" : \"hash\", (string) the mined block hash\n"
            "  \"shad\" : \"hash\", (string) the sha256d hash of the block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
        );

    LOCK(cs_main);

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > chainActive.Height())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");

    CBlockIndex* pblockindex = chainActive[nHeight];

    Object result;
    result.push_back(Pair("hash", pblockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("shad", pblockindex->GetBlockSha256dHash().GetHex()));
    result.push_back(Pair("gost", pblockindex->GetBlockGost3411Hash().GetHex()));
    return result;
}

Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock <\"hash\"> [verbose]\n"
            "\n If verbose is false, returns a string that is serialized, hex-encoded data for block 'hash'.\n"
            " If verbose is true, returns an Object with information about block <hash>.\n"
            "\nArguments:\n"
            " 1. \"hash\"    (string, required) The block mined or sha256d hash, either should work.\n"
            " 2. verbose   (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",              (hex string) The mined block hash\n"
            "  \"shad\" : \"hash\",              (hex string) The sha256d hash of the block\n"
            "  \"confirmations\" : n,          (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,                   (numeric) The size of the block.\n"
            "  \"height\" : n,                 (numeric) The blocks height in the chain, or the index if it is not yet confirmed.\n"
            "  \"version\" : n,                (numeric) The blocks version number.\n"
            "  \"merkleroot\" : \"hash\",        (hex string) The merkle root hash for this block.\n"
            "  \"tx\" : [                      (array of string) The transaction ids\n"
            "     \"transactionid\"            (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,                 (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,                  (numeric) The nonce\n"
            "  \"bits\" : \"xxxxxxxx\",          (hex string) The compact number representing the work required.\n"
            "  \"difficulty\" : x.xxx,         (numeric) This blocks required difficulty as a multiple of the minimum.\n"
            "  \"chainwork\" : \"xxxx\",         (hex string) The sum of all work proofs up to this point in the chain.\n"
            "  \"previousblockhash\" : \"hash\", (hex string) The hash of the previous block.\n"
            "  \"nextblockhash\" : \"hash\"      (hex string) The next blocks hash, only if it is available.\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"                          (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        );

    LOCK(cs_main);

    uintFakeHash GivenHash;
    GivenHash.SetHex(params[0].get_str());
    //! Allow the user to enter either the real block hash value or the sha256d hash,
    //! so we can better have backwards compatibility while working with rpc input.
    uint256 aRealHash = GivenHash.GetRealHash();
    if(aRealHash != 0)  GivenHash = aRealHash;

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    if (mapBlockIndex.count(GivenHash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[GivenHash];

    if(!ReadBlockFromDisk(block, pblockindex))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

    if (!fVerbose)
    {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        return strHex;
    }

    return blockToJSON(block, pblockindex);
}

Value gettxoutsetinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gettxoutsetinfo\n"
            "\n Returns statistics about the unspent transaction output set.\n"
            " Note this call may take some time.\n"
            "\nResult:\n"
            "{\n"
            "  \"height\":n,                (numeric) The current block height (index)\n"
            "  \"bestblock\": \"hex\",        (string) the best block hash hex\n"
            "  \"transactions\": n,         (numeric) The number of transactions\n"
            "  \"txouts\": n,               (numeric) The number of output transactions\n"
            "  \"bytes_serialized\": n,     (numeric) The serialized size\n"
            "  \"hash_serialized\": \"hash\", (string) The serialized hash\n"
            "  \"total_amount\": x.xxx      (numeric) The total amount\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettxoutsetinfo", "")
            + HelpExampleRpc("gettxoutsetinfo", "")
        );

    LOCK(cs_main);

    Object ret;

    CCoinsStats stats;
    FlushStateToDisk();
    if (pcoinsTip->GetStats(stats)) {
        ret.push_back(Pair("height", (int64_t)stats.nHeight));
        ret.push_back(Pair("bestblock", stats.hashBlock.GetHex()));
        ret.push_back(Pair("transactions", (int64_t)stats.nTransactions));
        ret.push_back(Pair("txouts", (int64_t)stats.nTransactionOutputs));
        ret.push_back(Pair("bytes_serialized", (int64_t)stats.nSerializedSize));
        ret.push_back(Pair("hash_serialized", stats.hashSerialized.GetHex()));
        ret.push_back(Pair("total_amount", ValueFromAmount(stats.nTotalAmount)));
    }
    return ret;
}

Value gettxout(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "gettxout <\"txid\"> <n> [includemempool]\n"
            "\n Returns details about an unspent transaction output.\n"
            "\nArguments:\n"
            "1. \"txid\"                   (string, required) The transaction id\n"
            "2. n                        (numeric, required) vout value\n"
            "3. includemempool           (boolean, optional) Whether to included the mem pool\n"
            "\nResult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",     (string) the block hash\n"
            "  \"confirmations\" : n,      (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,          (numeric) The transaction value in anc\n"
            "  \"scriptPubKey\" : {        (json object)\n"
            "     \"asm\" : \"code\",        (string) \n"
            "     \"hex\" : \"hex\",         (string) \n"
            "     \"reqSigs\" : n,         (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [        (array of string) array of anoncoin addresses\n"
            "        \"anoncoinaddress\"   (string) anoncoin address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"version\" : n,            (numeric) The version\n"
            "  \"coinbase\" : true|false   (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
        );

    LOCK(cs_main);

    Object ret;

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    int n = params[1].get_int();
    bool fMempool = true;
    if (params.size() > 2)
        fMempool = params[2].get_bool();

    CCoins coins;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip, mempool);
        if (!view.GetCoins(hash, coins))
            return Value::null;
        mempool.pruneSpent(hash, coins); // TODO: this should be done by the CCoinsViewMemPool
    } else {
        if (!pcoinsTip->GetCoins(hash, coins))
            return Value::null;
    }
    if (n<0 || (unsigned int)n>=coins.vout.size() || coins.vout[n].IsNull())
        return Value::null;

    uint256 viewBestBlock = pcoinsTip->GetBestBlock();
    BlockMap::iterator itBM = (viewBestBlock != 0) ? mapBlockIndex.find( viewBestBlock ) : mapBlockIndex.end();
    if( itBM == mapBlockIndex.end() ) {
        LogPrintf( "%s : pcoinsTip->GetBestBlock() returned:%s not setup correctly. Failed to finish steps...", __func__, viewBestBlock.ToString() );
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't find coin view best block");
    }
    CBlockIndex *pindex = itBM->second;
    ret.push_back(Pair("bestblock", pindex->GetBlockHash().GetHex()));
    if ((unsigned int)coins.nHeight == MEMPOOL_HEIGHT)
        ret.push_back(Pair("confirmations", 0));
    else
        ret.push_back(Pair("confirmations", pindex->nHeight - coins.nHeight + 1));
    ret.push_back(Pair("value", ValueFromAmount(coins.vout[n].nValue)));
    Object o;
    ScriptPubKeyToJSON(coins.vout[n].scriptPubKey, o, true);
    ret.push_back(Pair("scriptPubKey", o));
    ret.push_back(Pair("version", coins.nVersion));
    ret.push_back(Pair("coinbase", coins.fCoinBase));

    return ret;
}

Value verifychain(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "verifychain [checklevel] [numblocks]\n"
            "\n Verifies blockchain database.\n"
            "\nArguments:\n"
            "1. checklevel   (numeric, optional, 0-4, default=3) How thorough the block verification is.\n"
            "2. numblocks    (numeric, optional, default=980, 0=all) The number of blocks to check.\n"
            "\nResult:\n"
            " true|false     (boolean) Verified or not\n"
            "\nExamples:\n"
            + HelpExampleCli("verifychain", "")
            + HelpExampleRpc("verifychain", "")
        );

    LOCK(cs_main);

    int nCheckLevel = GetArg("-checklevel", 3);
    int nCheckDepth = GetArg("-checkblocks", 980);
    if (params.size() > 0)
        nCheckLevel = params[0].get_int();
    if (params.size() > 1)
        nCheckDepth = params[1].get_int();

    return VerifyDB(nCheckLevel, nCheckDepth);
}

Value getblockchaininfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockchaininfo\n"
            " Returns an object containing various state info regarding block chain processing.\n"
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",              (string) current chain (main, testnet, regtest)\n"
            "  \"blocks\": xxxxxx,             (numeric) the current number of blocks processed in the server\n"
            "  \"headers\": xxxxxx,            (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\",       (string) the hash of the currently best block\n"
            "  \"difficulty\" : x.xxx,         (numeric) The current required difficulty. Based on the minimum, smaller = harder, larger = easier.\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of verification progress [0..1], based on the last checkpoint.\n"
            "  \"chainwork\": \"xxxx\"           (hex string) Total amount of work in the active chain.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        );

    LOCK(cs_main);

    // proxyType proxy;
    // GetProxy(NET_IPV4, proxy);
    Object obj;
    obj.push_back(Pair("chain",         Params().NetworkIDString()));
    obj.push_back(Pair("blocks",        (int)chainActive.Height()));
    obj.push_back(Pair("headers",       pindexBestHeader ? pindexBestHeader->nHeight : -1));
    obj.push_back(Pair("bestblockhash", chainActive.Tip()->GetBlockHash().GetHex()));
    obj.push_back(Pair("bestblockGOST3411hash", chainActive.Tip()->GetBlockGost3411Hash().GetHex()));
    obj.push_back(Pair("difficulty",    (double)GetDifficulty()));
    uint256 hexVal;
    hexVal.SetCompact(chainActive.Tip()->nBits);
    obj.push_back(Pair("difficulty_hex",    strprintf( "0x%08x",hexVal.GetCompact()) ));
    obj.push_back(Pair("verificationprogress", Checkpoints::GuessVerificationProgress(chainActive.Tip())));
    obj.push_back(Pair("chainwork",     chainActive.Tip()->nChainWork.GetHex()));
    return obj;
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
          return (a->nHeight > b->nHeight);

        return a < b;
    }
};

Value getchaintips(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getchaintips\n"
            "Return information about all known tips in the block tree,"
            " including the main chain as well as orphaned branches.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,    (numeric) height of the chain tip\n"
            "    \"hash\" : \"xxxx\",   (string) block hash of the tip\n"
            "    \"shad\" : \"xxxx\",   (string) the sha256d hash of the tip\n"
            "    \"branchlen\": 0     (numeric) zero for main chain\n"
            "    \"status\": \"active\" (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1     (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"   (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "Possible values for status:\n"
            " 1.  \"invalid\"             This branch contains at least one invalid block\n"
            " 2.  \"headers-only\"        Not all blocks for this branch are available, but the headers are valid\n"
            " 3.  \"valid-headers\"       All blocks are available for this branch, but they were never fully validated\n"
            " 4.  \"valid-fork\"          This branch is not part of the active chain, but is fully validated\n"
            " 5.  \"active\"              This is the tip of the active main chain, which is certainly valid\n"
            "\nExamples:\n"
            + HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
        );

    LOCK(cs_main);

    /* Build up a list of chain tips.  We start with the list of all
       known blocks, and successively remove blocks that appear as pprev
       of another block.  */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    BOOST_FOREACH(const PAIRTYPE(const uint256, CBlockIndex*)& item, mapBlockIndex)
        setTips.insert(item.second);
    BOOST_FOREACH(const PAIRTYPE(const uint256, CBlockIndex*)& item, mapBlockIndex)
    {
        const CBlockIndex* pprev = item.second->pprev;
        if (pprev)
            setTips.erase(pprev);
    }

    // Always report the currently active tip.
    setTips.insert(chainActive.Tip());

    /* Construct the output array.  */
    Array res;
    BOOST_FOREACH(const CBlockIndex* block, setTips)
    {
        Object obj;
        obj.push_back(Pair("height", block->nHeight));
        obj.push_back(Pair("hash", block->phashBlock->GetHex()));
        obj.push_back(Pair("shad", block->GetBlockSha256dHash().GetHex()));
        obj.push_back(Pair("gost", block->GetBlockGost3411Hash().GetHex()));

        const int branchLen = block->nHeight - chainActive.FindFork(block)->nHeight;
        obj.push_back(Pair("branchlen", branchLen));

        string status;
        if (chainActive.Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nChainTx == 0) {
            // This block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
            // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
            // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }
        obj.push_back(Pair("status", status));

        res.push_back(obj);
    }

    return res;
}

Value getmempoolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmempoolinfo\n"
            "\nReturns details on the active state of the TX memory pool.\n"
            "\nResult:\n"
            "{\n"
            "  \"size\": xxxxx   (numeric) Current tx count\n"
            "  \"bytes\": xxxxx  (numeric) Sum of all tx sizes\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolinfo", "")
            + HelpExampleRpc("getmempoolinfo", "")
        );

    Object ret;
    ret.push_back(Pair("size", (int64_t) mempool.size()));
    ret.push_back(Pair("bytes", (int64_t) mempool.GetTotalTxSize()));

    return ret;
}

Value invalidateblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "invalidateblock <\"hash\">\n"
            "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n"
            "Note: This command can only be used on a test network.\n"
            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to mark as invalid\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
        );

    if (isMainNetwork())
        throw JSONRPCError(RPC_INVALID_REQUEST, "For testnet use only.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        InvalidateBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return Value::null;
}

Value reconsiderblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "reconsiderblock <\"hash\">\n"
            "\nRemoves invalidity status of a block and its descendants, reconsider them for activation.\n"
            "Notes: This can be used to undo the effects of invalidateblock.\n"
            "       This command can only be used on a test network.\n"
            "\nArguments:\n"
            "1. hash   (string, required) the hash of the block to reconsider\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("reconsiderblock", "\"blockhash\"")
            + HelpExampleRpc("reconsiderblock", "\"blockhash\"")
        );

    if (isMainNetwork())
        throw JSONRPCError(RPC_INVALID_REQUEST, "For testnet use only.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);
    CValidationState state;

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

        CBlockIndex* pblockindex = mapBlockIndex[hash];
        ReconsiderBlock(state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return Value::null;
}
