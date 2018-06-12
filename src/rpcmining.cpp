// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
// anoncoin-config.h loaded...

#include "amount.h"
#include "chainparams.h"
#include "core_io.h"
#include "init.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "pow.h"
#include "sync.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "db.h"
#include "wallet.h"
#endif

#include <sstream>
#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
Value getnetworkhashps(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getnetworkhashps [blocks] [height]\n"
            "\nReturns the estimated network hashes per second, optionally for a given height and based on the last n blocks.\n"
            "\nArguments:\n"
            "1. blocks  (numeric, optional, default='tipfiltersize') Hint: The 'getretargetpid 1' query tells you the tip filters size.\n"
            "2. height  (numeric, optional, default=0) Default to current chain tip, or specify at what height the calculation is to be made.\n"
            "\nNOTES: Block spacing is measured, so at least 2 blocks are needed to calculate one spacing interval, the more the better.\n"
            " Pass in [height] to estimate the network speed at the time when a certain block was mined. Pass in [blocks] to override the # of\n"
            " blocks used in the calculation, any value < 2 sets the min of 2 blocks.  Expect a poor estimate, with so few.\n"
            "\nResult:\n"
            "  (numeric) Estimated hashes per second, the calculation made is the chain work proofs (latest - oldest) / time delta.\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkhashps", "")
            + HelpExampleCli("getnetworkhashps", "200 350000")
            + HelpExampleRpc("getnetworkhashps", "")
            + HelpExampleRpc("getnetworkhashps", "5000,390000")
       );

    LOCK(cs_main);
    //! Allow the 1st parameter to be null...
    RPCTypeCheck( params, boost::assign::list_of(int_type)(int_type), true );

    if( pRetargetPid == NULL )
        throw JSONRPCError(RPC_INTERNAL_ERROR, "RetargetPID has not been initialized");

    int32_t nTipFilterSize = pRetargetPid->GetTipFilterSize();
    int32_t nLookup = params.size() > 0 && params[ 0 ].type() == int_type ? params[ 0 ].get_int() : nTipFilterSize;
    int32_t nHeight = params.size() > 1 && params[ 1 ].type() == int_type ? params[ 1 ].get_int() : 0;
    CBlockIndex *pb = chainActive.Tip();

    //! If 'height' is positive and less that the Tip() height, compute the estimate at the time when a given block was found.
    if( nHeight > 0 && nHeight < chainActive.Height() ) pb = chainActive[nHeight];

    return CalcNetworkHashPS( pb, nLookup );
}

#ifdef ENABLE_WALLET
Value getgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "\n Return if the server is set to generate coins or not. The default is false.\n"
            " It is set with the command line argument -gen (or anoncoin.conf setting gen)\n"
            " It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "true|false      (boolean) If the server is set to generate coins or not\n"
            "\nExamples:\n"
            + HelpExampleCli("getgenerate", "")
            + HelpExampleRpc("getgenerate", "")
        );

    LOCK(cs_main);
    return GetBoolArg("-gen", false);
}

Value generate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "generate <numblocks> [\"startingdifficulty\"]\n"
            "\n Mine blocks immediately (before the RPC call returns)\n"
            "\n Note: This command can only be used on a test network.\n"
            "\nArguments:\n"
            "1. numblocks          (numeric) How many blocks are generated immediately.\n"
            "2. startingdifficulty (hex string, optional) The initial Testnet starting difficulty as a 256 bit value.\n"
            "                              This is only used on TestNet, and then only for the 1st 'TipFilterBlocks'.\n"
            "\nResult\n"
            "[ blockhashes ]       (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            + HelpExampleCli("generate", "11")
        );

    if (pwalletMain == NULL)
        throw JSONRPCError(RPC_INVALID_REQUEST, "No wallet available (disabled)");
    if (isMainNetwork())
        throw JSONRPCError(RPC_INVALID_REQUEST, "For regression testing and initializing testnet. Non mainnet modes only");
    if( pRetargetPid == NULL )
        throw JSONRPCError(RPC_INTERNAL_ERROR, "RetargetPID has not been initialized");

    int nHeightStart = 0;
    int nHeightEnd = 0;
    int nHeight = 0;
    int nGenerate = params[0].get_int();
    CReserveKey reservekey(pwalletMain);

    {   // Don't keep cs_main locked
        LOCK(cs_main);
        nHeightStart = chainActive.Height();
        nHeight = nHeightStart;
        nHeightEnd = nHeightStart+nGenerate;
    }

    uint256 uintStartingHash;
    if( params.size() > 1 ) uintStartingHash.SetHex( params[1].get_str() );

    unsigned int nExtraNonce = 0;
    Array blockHashes;
    while (nHeight < nHeightEnd)
    {
        auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey));
        if (!pblocktemplate.get())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet keypool empty");
        CBlock *pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
        }

        uint256 uintTargetHash;
        if( TestNet() && nHeight < (pRetargetPid->GetTipFilterBlocks() + 1) ) {
            pblock->nBits = uintStartingHash.GetCompact();
            uintTargetHash = Params().ProofOfWorkLimit( CChainParams::ALGO_GOST3411 );
        } else
            uintTargetHash.SetCompact(pblock->nBits);

        //! Calling GetHash with true, invalidates any previously calculated hashes for this block, as they have changed
        //! while (!CheckProofOfWork(pblock->GetHash(true), pblock->nBits)) {
        while (pblock->GetHash(true) > uintTargetHash) {
            //! Yes, there is a chance every nonce could fail to satisfy the -regtest
            //! target -- 1 in 2^(2^32). That ain't gonna happen.
            ++pblock->nNonce;
        }
        assert( pblock->CalcSha256dHash(true) != uintFakeHash(0) ); //! Force both hash calculations to be updated
        CValidationState state;
        if (!ProcessNewBlock(state, NULL, pblock))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());
    }
    return blockHashes;
}


Value setgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate <generate> [genproclimit]\n"
            "\nSet 'generate' true or false to turn generation on or off.\n"
            "Generation is limited to 'genproclimit' processors, -1 is unlimited.\n"
            "See the getgenerate call for the current setting.\n"
            "\nArguments:\n"
            "1. generate      (boolean, required) Set to true to turn on generation, off to turn off.\n"
            "2. genproclimit  (numeric, optional) Set the processor limit for when generation is on. Can be -1 for unlimited.\n"
            "                        Note: in -regtest mode, genproclimit controls how many blocks are generated immediately.\n"
            "\nResult\n"
            "[ blockhashes ]  (array, -regtest only) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nSet the generation on with a limit of one processor\n"
            + HelpExampleCli("setgenerate", "true 1") +
            "\nCheck the setting\n"
            + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation\n"
            + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n"
            + HelpExampleRpc("setgenerate", "true, 1")
        );

    if (pwalletMain == NULL)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");

    bool fGenerate = true;
    if (params.size() > 0)
        fGenerate = params[0].get_bool();

    int nGenProcLimit = -1;
    if (params.size() > 1)
    {
        nGenProcLimit = params[1].get_int();
        if (nGenProcLimit == 0)
            fGenerate = false;
    }

    // -regtest mode: don't return until nGenProcLimit blocks are generated
    if (fGenerate && RegTest())
    {
        int nHeightStart = 0;
        int nHeightEnd = 0;
        int nHeight = 0;
        int nGenerate = (nGenProcLimit > 0 ? nGenProcLimit : 1);
        CReserveKey reservekey(pwalletMain);

        {   // Don't keep cs_main locked
            LOCK(cs_main);
            nHeightStart = chainActive.Height();
            nHeight = nHeightStart;
            nHeightEnd = nHeightStart+nGenerate;
        }
        unsigned int nExtraNonce = 0;
        Array blockHashes;
        while (nHeight < nHeightEnd)
        {
            auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey));
            if (!pblocktemplate.get())
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet keypool empty");
            CBlock *pblock = &pblocktemplate->block;
            {
                LOCK(cs_main);
                IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
            }
            uint256 uintTargetHash;
            uintTargetHash.SetCompact(pblock->nBits);
            //! Calling GetHash with true, invalidates any previously calculated hashes for this block, as they have changed
            // while (!CheckProofOfWork(pblock->GetHash(true), pblock->nBits)) {
            while (pblock->GetHash(true) > uintTargetHash) {
                //! Yes, there is a chance every nonce could fail to satisfy the -regtest
                //! target -- 1 in 2^(2^32). That ain't gonna happen.
                ++pblock->nNonce;
            }
            assert( pblock->CalcSha256dHash(true) != uintFakeHash(0) ); //! Force both hash calculations to be updated
            CValidationState state;
            if (!ProcessNewBlock(state, NULL, pblock))
                throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
            ++nHeight;
            blockHashes.push_back(pblock->GetHash().GetHex());
        }
        return blockHashes;
    }
    else // Not -regtest: start generate thread, return immediately
    {
        mapArgs["-gen"] = (fGenerate ? "1" : "0");
        mapArgs ["-genproclimit"] = itostr(nGenProcLimit);
        GenerateAnoncoins(fGenerate, pwalletMain, nGenProcLimit);
    }

    return Value::null;
}
#endif // ENABLE_WALLET

Value gethashmeter(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "gethashmeter [clear]\n"
            "\n Anoncoin's v2.0 Scrypt miners have Multi-Threaded Hash Meter (MTHM) technology built-in. This query now returns a subset of that information\n"
            " which can be provided by the system while generating Anoncoin is turned on. Exact performance measurements are made by each thread (core) you\n"
            " have running every 10 seconds and 10 minutes.  The most recent 10 second samples are kept from all the threads for 10 minutes, and the 10 minute\n"
            " values for up to 24 hours. Also an accumulated total hash power your system has been producing since mining started is shown in a new format\n"
            " Mega Hash per Hour. Null(0) will be returned, if the system was busy and unable to respond within 2 seconds.  Some longer term results are kept\n"
            " and available even after mining has been turned off. See the getgenerate and setgenerate calls to turn generation on and off.\n"
/*            " If mining has been run in the past, the fast short term results will be zero, although the\n"
            " slow count and KHPS will be kept and returned by this query between runs.  Once mining\n"
            " resumes, the total accumulation values are always based on the current fast result cache,
            " otherwise they are based on the number of threads you last had running.\n" */
            "\nArguments:\n"
            "1. clear: true|false  (boolean, optional) If provided and set true, this allows you to clear the 10 minute results accumulator.\n"
            "\nResult:\n"
            "{\n"
            "  \"miners\": nnn,      (numeric) The # of miners reporting results, should match the # of threads you have mining.\n"
            "  \"runtime\": \"time\"   (string) The # of days HH:MM:SS you have been mining Anoncoins!\n"
            "  \"fastcount\": nnn,   (numeric) The # of samples found in the most recent 10s result cache. If gen has been turned off, this will be 0.\n"
            "  \"slowcount\": nnn,   (numeric) The # of samples found in the most recent 10m result cache.\n"
            "  \"fastkhps\": nn.nnn, (numeric) Kilo Hashes/Second. Averaged 10s samples from over the last 10 minutes, reports come from all miner threads.\n"
            "  \"slowkhps\": nn.nnn, (numeric) Kilo Hashes/Second. Average of 10 minute samples from up to the last 24 hours, comes from all miner threads.\n"
            "  \"corehash\": nnn,    (numeric) The # of hash calculations your miners have produced. Taken from the sum of all thread(s) you have running.\n"
            "  \"coretime\": nnn,    (numeric) Seconds. Multiple cores means the power of time multiplication, 4 miners can produce 40m of work in one 10m interval.\n"
            "  \"coremhph\": nn.nnn, (numeric) Cumulative Mega Hash/Hour. Result is produced from the most recent available sample, from each of the miner threads\n"
            "                                you have, or had running last. 'corehash' divided by elapsed realtime is used to produce this unit of measurement.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gethashmeter", "")
            + HelpExampleRpc("gethashmeter", "")
        );

    RPCTypeCheck(params, boost::assign::list_of(bool_type));

    bool fClearSlowMRU = false;
    if( params.size() > 0 && params[0].get_bool() == true )
        if( !ClearHashMeterSlowMRU() )
            return (int64_t)0;

    HashMeterStats HashMeterState;
    if( GetHashMeterStats(HashMeterState) ) {
        int64_t nRunTime = HashMeterState.nRunTime;
        string sRunTime;

        sRunTime = strprintf( "%d days %02d:%02d:%02d",
                              nRunTime / SECONDSPERDAY, (nRunTime % SECONDSPERDAY) / 3600, (nRunTime % 3600) / 60, nRunTime % 60 );
        Object obj;
        obj.push_back(Pair("miners",    (int)HashMeterState.nIDsReporting ));
        obj.push_back(Pair("runtime",   sRunTime));
        obj.push_back(Pair("fastcount", (int64_t)HashMeterState.nFastCount ));
        obj.push_back(Pair("slowcount", (int64_t)HashMeterState.nSlowCount ));
        obj.push_back(Pair("fastkhps",  (double)HashMeterState.dFastKHPS ));
        obj.push_back(Pair("slowkhps",  (double)HashMeterState.dSlowKHPS ));
        obj.push_back(Pair("corehash", (int64_t)HashMeterState.nCumulativeHashes ));
        obj.push_back(Pair("coretime", (int64_t)HashMeterState.nCumulativeTime ));
        obj.push_back(Pair("coremhph", (double)HashMeterState.dCumulativeMHPH ));
        return obj;
    } else
        return (int64_t)0;
}

Value getmininginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\n Returns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,           (numeric) The current active chain block height.\n"
            "  \"currentblocksize\": nnn, (numeric) The size of the last created block.\n"
            "  \"currentblocktx\": nnn,   (numeric) The last blocks transaction count, excluding the coinbase.\n"
            "  \"difficulty\" : x.xxx,    (numeric) The current required difficulty. Based on the minimum, smaller = harder, larger = easier.\n"
            "  \"errors\": \"...\"          (string)  Any current status bar errors are reported here.\n"
            "  \"genproclimit\": n        (numeric) The processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)\n"
            "  \"networkhashps\": n       (numeric) The estimated network hashes per second. Based on a smoothed result over the default time period.\n"
            "  \"pooledtx\": n            (numeric) The size of the memory pool\n"
            "  \"chain\": \"xxxx\",         (string)  Current network name.  Anoncoin defines this as either 'main', 'testnet' or 'regtest'.\n"
            "  \"generate\": true|false   (boolean) If the generation is on or off (see getgenerate or setgenerate calls)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmininginfo", "")
            + HelpExampleRpc("getmininginfo", "")
        );

    LOCK(cs_main);

    if( pRetargetPid == NULL )
        throw JSONRPCError(RPC_INTERNAL_ERROR, "RetargetPID has not been initialized");

    Object obj;
    obj.push_back(Pair("blocks",           (int)chainActive.Height()));
    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx",   (uint64_t)nLastBlockTx));
    obj.push_back(Pair("difficulty",       (double)GetDifficulty()));
    obj.push_back(Pair("errors",           GetWarnings("statusbar")));
    obj.push_back(Pair("genproclimit",     (int)GetArg("-genproclimit", -1)));
    obj.push_back(Pair("networkhashps",    CalcNetworkHashPS( chainActive.Tip(), pRetargetPid->GetTipFilterSize() )));
    obj.push_back(Pair("pooledtx",         (uint64_t)mempool.size()));
    obj.push_back(Pair("chain",            Params().NetworkIDString()));
#ifdef ENABLE_WALLET
    obj.push_back(Pair("generate",         getgenerate(params, false)));
#endif
    return obj;
}


// NOTE: Unlike wallet RPC (which use ANC values), mining RPC follows GBT (BIP 22) in using satoshi amounts
Value prioritisetransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "prioritisetransaction <txid> <priority delta> <fee delta>\n"
            "\n Accepts the transaction into mined blocks at a higher (or lower) priority\n"
            "\nArguments:\n"
            "1. \"txid\"         (string, required) The transaction id.\n"
            "2. priority delta (numeric, required) The priority to add or subtract.\n"
            "                                      The transaction selection algorithm considers the tx as it would have a higher priority.\n"
            "                                      (priority of a transaction is calculated: coinage * value_in_satoshis / txsize)\n"
            "3. fee delta      (numeric, required) The fee value (in satoshis) to add (or subtract, if negative).\n"
            "                                      The fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                                      considers the transaction as it would have paid a higher (or lower) fee.\n"
            "\nResult\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n"
            + HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000")
            + HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000")
        );

    LOCK(cs_main);

    uint256 hash = ParseHashStr(params[0].get_str(), "txid");
    CAmount nAmount = params[2].get_int64();

    mempool.PrioritiseTransaction(hash, params[0].get_str(), params[1].get_real(), nAmount);
    return true;
}


// NOTE: Assumes a conclusive result; if result is inconclusive, it must be handled by caller
static Value BIP22ValidationResult(const CValidationState& state)
{
    if (state.IsValid())
        return Value::null;

    string strRejectReason = state.GetRejectReason();
    if (state.IsError())
        throw JSONRPCError(RPC_VERIFY_ERROR, strRejectReason);
    if (state.IsInvalid())
    {
        if (strRejectReason.empty())
            return "rejected";
        return strRejectReason;
    }
    // Should be impossible
    return "valid?";
}

Value getblocktemplate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getblocktemplate [\"jsonrequestobject\"]\n"
            "\n If the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
            " It returns data needed to construct a block to work on.\n"
            " See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments:\n"
            "1. \"jsonrequestobject\"          (string, optional) A json object in the following spec\n"
            "     {\n"
            "       \"mode\":\"template\"        (string, optional) This must be set to \"template\" or omitted\n"
            "       \"capabilities\":[         (array, optional) A list of strings\n"
            "           \"support\"            (string) client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'\n"
            "           ,...\n"
            "         ]\n"
            "     }\n"
            "\n"

            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                (numeric) The block version\n"
            "  \"previousblockhash\" : \"xxxx\", (string) The sha256d hash of current highest block\n"
            "  \"transactions\" : [            (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",       (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"hash\" : \"xxxx\",       (string) hash/id encoded in little-endian hexadecimal\n"
            "         \"depends\" : [          (array) array of numbers \n"
            "             n                  (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final\n"
            "                                block if this one is.\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                (numeric) difference in value between transaction inputs and outputs (in Satoshis); for coinbase transactions,\n"
            "                                  this is a negative Number of the total collected block fees (ie, not including the block subsidy); if key is not\n"
            "                                  present, fee is unknown and clients MUST NOT assume there isn't one\n"
            "         \"sigops\" : n,            (numeric) total number of SigOps, as counted for purposes of block limits; if key is not present, sigop count is\n"
            "                                  unknown and clients MUST NOT assume there aren't any.\n"
            "         \"required\" : true|false  (boolean) if provided and true, this transaction must be in the final block\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
            "  \"coinbaseaux\" : {               (json object) data that should be included in the coinbase's scriptSig content\n"
            "      \"flags\" : \"flags\"           (string) \n"
            "  },\n"
            "  \"coinbasevalue\" : n,            (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in Satoshis)\n"
            "  \"coinbasetxn\" : { ... },        (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",              (string) The hash target\n"
            "  \"mintime\" : xxx,                (numeric) The minimum timestamp appropriate for next block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                   (array of string) list of ways the block template may be changed \n"
            "     \"value\"                      (string) A way the block template may be changed, e.g. Anoncoin default is 'transactions' only\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\", (string) A range of valid nonces\n"
            "  \"sigoplimit\" : n,               (numeric) limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                (numeric) limit of block size\n"
            "  \"curtime\" : ttt,                (numeric) current timestamp in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxx\",                 (string) compressed target of next block\n"
            "  \"height\" : n                    (numeric) The height of the next block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("getblocktemplate", "")
            + HelpExampleRpc("getblocktemplate", "")
         );

    LOCK(cs_main);

    if( pRetargetPid == NULL )
        throw JSONRPCError(RPC_INTERNAL_ERROR, "RetargetPID has not been initialized");

    string strMode = "template";
    Value lpval = Value::null;
    if (params.size() > 0)
    {
        const Object& oparam = params[0].get_obj();
        const Value& modeval = find_value(oparam, "mode");
        if (modeval.type() == str_type)
            strMode = modeval.get_str();
        else if (modeval.type() == null_type)
        {
            /* Do nothing */
        }
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
        lpval = find_value(oparam, "longpollid");

        if (strMode == "proposal")
        {
            const Value& dataval = find_value(oparam, "data");
            if (dataval.type() != str_type)
                throw JSONRPCError(RPC_TYPE_ERROR, "Missing data String key for proposal");

            CBlock block;
            if (!DecodeHexBlk(block, dataval.get_str()))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

            uint256 aRealHash = block.GetHash();
            BlockMap::iterator mi = mapBlockIndex.find(aRealHash);
            if (mi != mapBlockIndex.end()) {
                CBlockIndex *pindex = mi->second;
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                return "duplicate-inconclusive";
            }

            CBlockIndex* const pindexPrev = chainActive.Tip();
            // TestBlockValidity only supports blocks built on the current Tip
            uint256 prevRealHash = block.hashPrevBlock.GetRealHash();
            if( prevRealHash == 0 || prevRealHash != pindexPrev->GetBlockHash())
                return "inconclusive-not-best-prevblk";
            CValidationState state;
            TestBlockValidity(state, block, pindexPrev, false, true);
            return BIP22ValidationResult(state);
        }
    }

    if (strMode != "template")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Anoncoin is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Anoncoin is downloading blocks...");

    static unsigned int nTransactionsUpdatedLast;

    if (lpval.type() != null_type)
    {
        // Wait to respond until either the best block changes, OR 30 seconds have passed and there are more transactions
        uintFakeHash hashWatchedChain;
        boost::system_time checktxtime;
        unsigned int nTransactionsUpdatedLastLP;

        if (lpval.type() == str_type)
        {
            // Format: <hashBestChain><nTransactionsUpdatedLast>
            string lpstr = lpval.get_str();

            hashWatchedChain.SetHex(lpstr.substr(0, 64));
            nTransactionsUpdatedLastLP = atoi64(lpstr.substr(64));
        }
        else
        {
            // NOTE: Spec does not specify behaviour for non-string longpollid, but this makes testing easier
            hashWatchedChain = chainActive.Tip()->GetBlockSha256dHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

        // Release the main lock while waiting
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime = boost::get_system_time() + boost::posix_time::seconds(30);    // Anoncoin waits 30 seconds

            boost::unique_lock<boost::mutex> lock(csBestBlock);
            while (chainActive.Tip()->GetBlockSha256dHash() == hashWatchedChain && IsRPCRunning())
            {
                if (!cvBlockChange.timed_wait(lock, checktxtime))
                {
                    // Timeout: Check transactions for update
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLastLP)
                        break;
                    checktxtime += boost::posix_time::seconds(10);
                }
            }
        }
        ENTER_CRITICAL_SECTION(cs_main);

        if (!IsRPCRunning())
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Shutting down");
        // TODO: Maybe recheck connections/IBD and (if something wrong) send an expires-immediately template to stop miners?
    }

    // Update block
    static CBlockIndex* pindexPrev = NULL;
    static int64_t nStart;
    static CBlockTemplate* pblocktemplate = NULL;
    if (pindexPrev != chainActive.Tip() ||
        (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 5))
    {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = NULL;

        // Store the pindexBest used before CreateNewBlock, to avoid races
        nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrevNew;
        pindexPrevNew = chainActive.Tip();
        nStart = GetTime();

        // Create new block
        if(pblocktemplate)
        {
            delete pblocktemplate;
            pblocktemplate = NULL;
        }
        CScript scriptDummy = CScript() << OP_TRUE;
        pblocktemplate = CreateNewBlock(scriptDummy);
        if (!pblocktemplate)
            throw JSONRPCError(RPC_OUT_OF_MEMORY, "No blocktemplate object created - Out of memory or other error");

        // Need to update only after we know CreateNewBlock succeeded and no errors were found.
        pindexPrev = pindexPrevNew;
    }

    CBlock* pblock = &pblocktemplate->block; // pointer for convenience
    // DO NOT Update nTime, as it was set and locked to the retarget pid next work required when CreateNewBlock was called.
    // UpdateTime(pblock, pindexPrev);
    pblock->nNonce = 0;

    static const Array aCaps = boost::assign::list_of("proposal");

    Array transactions;
    map<uint256, int64_t> setTxIndex;
    int i = 0;
    BOOST_FOREACH (CTransaction& tx, pblock->vtx)
    {
        uint256 txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        if (tx.IsCoinBase())
            continue;

        Object entry;

        entry.push_back(Pair("data", EncodeHexTx(tx)));

        entry.push_back(Pair("hash", txHash.GetHex()));

        Array deps;
        BOOST_FOREACH (const CTxIn &in, tx.vin)
        {
            if (setTxIndex.count(in.prevout.hash))
                deps.push_back(setTxIndex[in.prevout.hash]);
        }
        entry.push_back(Pair("depends", deps));

        int index_in_template = i - 1;
        entry.push_back(Pair("fee", pblocktemplate->vTxFees[index_in_template]));
        entry.push_back(Pair("sigops", pblocktemplate->vTxSigOps[index_in_template]));

        transactions.push_back(entry);
    }

    Object aux;
    aux.push_back(Pair("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end())));

    uint256 hashTarget = uint256().SetCompact(pblock->nBits);

    static Array aMutable;
    if( aMutable.empty() ) {
        //! In order to work with a retarget system that is dynamic some things need to be changed here.
        //! Time and prevblock values must be held by the miner as un-mutable values, if that is being done.
        //! Only transactions can be allowed to change regardless of the state UsesHeader() returns.
        //! If the Headers time is being used, the 'bits' field also needs to be mutable, as it constantly changes.
        //! If they were allowed to be change that, the difficulty required will need to be re-computed by this node,
        //! or the miner themselves.
        //! Plans are to allow for that in future releases, as well as allowing the miner to change the difficulty
        //! on the fly.  This will only be possible if they have the necessary information, which is part of the goal
        //! in developing the 'getretargetpid' query.  Results returned need to be what is required to calculate
        //!  difficulty themselves, for a given height.  The Integrator value would need to be given to them each time
        //! the height changes, as that calculation can involve several thousand blocks.
        aMutable.push_back("time");
        if( !pRetargetPid->UsesHeader() ) {
            aMutable.push_back("prevblock");
        }
        // else
            // aMutable.push_back("bits");
        aMutable.push_back("transactions");
    }

    Object result;
    result.push_back(Pair("capabilities", aCaps));
    result.push_back(Pair("version", pblock->nVersion));
    result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));
    result.push_back(Pair("transactions", transactions));
    result.push_back(Pair("coinbaseaux", aux));
    result.push_back(Pair("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nValue));
    result.push_back(Pair("longpollid", chainActive.Tip()->GetBlockSha256dHash().GetHex() + i64tostr(nTransactionsUpdatedLast)));
    result.push_back(Pair("target", hashTarget.GetHex()));
    result.push_back(Pair("mintime", (int64_t)pindexPrev->GetMedianTimePast()+1));
    result.push_back(Pair("mutable", aMutable));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    result.push_back(Pair("sigoplimit", (int64_t)MAX_BLOCK_SIGOPS));
    result.push_back(Pair("sizelimit", (int64_t)MAX_BLOCK_SIZE));
    result.push_back(Pair("curtime", pblock->GetBlockTime()));
    result.push_back(Pair("bits", HexBits(pblock->nBits)));
    result.push_back(Pair("height", (int64_t)(pindexPrev->nHeight+1)));

    return result;
}

class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 aRealHash;
    bool found;
    CValidationState state;

    submitblock_StateCatcher(const uint256 &hashIn) : aRealHash(hashIn), found(false), state() {};

protected:
    void BlockChecked(const CBlock& block, const CValidationState& stateIn) {
        if (block.GetHash() != aRealHash)
            return;
        found = true;
        state = stateIn;
    };
};

Value submitblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "submitblock <\"hexdata\"> [\"jsonparametersobject\"]\n"
            "\n Attempts to submit new block to network.\n"
            " The 'jsonparametersobject' parameter is currently ignored.\n"
            " See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"
            "\nArguments\n"
            "1. \"hexdata\"              (string, required) the hex-encoded block data to submit\n"
            "2. \"jsonparametersobject\" (string, optional) object of optional parameters\n"
            "    {\n"
            "      \"workid\" : \"id\"     (string, optional) if the server provided a workid, it MUST be included with submissions\n"
            "    }\n"
            "\nResult: The BIP22 validation result or error that occurred.\n"
            "\nExamples:\n"
            + HelpExampleCli("submitblock", "\"mydata\"")
            + HelpExampleRpc("submitblock", "\"mydata\"")
        );

    CBlock block;
    if (!DecodeHexBlk(block, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

    uint256 aRealHash = block.GetHash();
    bool fBlockPresent = false;
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(aRealHash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex *pindex = mi->second;
            if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                return "duplicate";
            if (pindex->nStatus & BLOCK_FAILED_MASK)
                return "duplicate-invalid";
            // Otherwise, we might only have the header - process the block before returning
            fBlockPresent = true;
        }
    }

    CValidationState state;
    submitblock_StateCatcher sc(aRealHash);
    RegisterValidationInterface(&sc);
    bool fAccepted = ProcessNewBlock(state, NULL, &block);
    UnregisterValidationInterface(&sc);
    if (fBlockPresent)
    {
        if (fAccepted && !sc.found)
            return "duplicate-inconclusive";
        return "duplicate";
    }
    if (fAccepted)
    {
        if (!sc.found)
            return "inconclusive";
        state = sc.state;
    }
    return BIP22ValidationResult(state);
}

Value estimatefee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatefee <nblocks>\n"
            "\n Estimates the approximate fee per kilobyte needed for a transaction to begin confirmation within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, required) the proposed number of blocks.\n"
            "\nResult:\n"
            "n :            (numeric) estimated fee-per-kilobyte\n"
            "\nNote: -1.0 is returned if not enough transactions and blocks have been observed to make an estimate.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatefee", "6")
            );

    RPCTypeCheck(params, boost::assign::list_of(int_type));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    CFeeRate feeRate = mempool.estimateFee(nBlocks);
    if (feeRate == CFeeRate(0))
        return -1.0;

    return ValueFromAmount(feeRate.GetFeePerK());
}

Value estimatepriority(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatepriority <nblocks>\n"
            "\n Estimates the approximate priority a zero-fee transaction needs to begin confirmation within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, required) the proposed number of blocks.\n"
            "\nResult:\n"
            "n :            (numeric) estimated priority\n"
            "\nNote: -1.0 is returned if not enough transactions and blocks have been observed to make an estimate.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatepriority", "6")
            );

    RPCTypeCheck(params, boost::assign::list_of(int_type));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    return mempool.estimatePriority(nBlocks);
}

typedef map<uint256, CBlock*> mapNewBlock_t;
static mapNewBlock_t mapNewBlock;
static CCriticalSection cs_getwork;
static CReserveKey* pMiningKey = NULL;
static vector<CBlockTemplate*> vNewBlockTemplate;

Value getwork(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getwork [\"data\"]\n"
            "\n If 'data' is not specified, it returns the formatted hash data to work on.\n"
            " If 'data' is specified, tries to solve the block and returns true if it was successful.\n"
            "\nArguments:\n"
            "1. \"data\"              (string, optional) The hex encoded data to solve\n"
            "\nResult (when 'data' is not specified):\n"
            "{\n"
            "  \"midstate\" : \"xxxx\", (hex string) The precomputed hash state after hashing the first half of the data.\n" // deprecated
            "  \"data\" : \"xxxxx\",    (hex string) The block data.\n"
            "  \"hash1\" : \"xxxxx\",   (hex string) The formatted hash buffer for second hash.\n" // deprecated
            "  \"target\" : \"xxxx\"    (hex string) The little endian hash target.\n"
            "}\n"
            "\nResult (when 'data' is specified):\n"
            "true|false             (boolean) If solving the block specified in the 'data' was successful.\n"
            "\nExamples:\n"
            + HelpExampleCli("getwork", "")
            + HelpExampleRpc("getwork", "")
        );

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Anoncoin is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Anoncoin is downloading blocks...");

    if( pRetargetPid == NULL )
        throw JSONRPCError(RPC_INTERNAL_ERROR, "RetargetPID has not been initialized");

    TRY_LOCK(cs_getwork, lockGetwork);
    if (!lockGetwork)
        throw JSONRPCError(RPC_MISC_ERROR, "Lock failed, getwork busy...");

    if (params.size() == 0)
    {
        // Update block
        static CBlockIndex* pindexPrev = NULL;
        static unsigned int nExtraNonce = 0;

        if( !pMiningKey )
            pMiningKey = new CReserveKey(pwalletMain);

        CBlockIndex* pindexPrevNew = chainActive.Tip();
        if( pindexPrev != pindexPrevNew ) {
            //! Deallocate old blocks since they're obsolete if another new block has arrived.
            BOOST_FOREACH(CBlockTemplate* pATemplate, vNewBlockTemplate)
                delete pATemplate;
            vNewBlockTemplate.clear();
            //!  Also clear the MerkleTree hash lookup map
            mapNewBlock.clear();
        }
        // Clear pindexPrev so future getworks make a new block, despite any failures from here on
        pindexPrev = NULL;

        // Create new block
        CBlockTemplate* pblocktemplate = CreateNewBlockWithKey(*pMiningKey);
        if( !pblocktemplate )
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet keypool empty");

        //! Remember the auto pointer to the block template, this keeps it from being destroyed
        vNewBlockTemplate.push_back(pblocktemplate);

        //! Setup a local point to the block within the template, as that is primarily what we will be working with
        CBlock* pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            //! Update nExtraNonce, sets the MerkleTree hash and the vtx[0].vin[0].scriptSig to the correct values too..
            IncrementExtraNonce(pblock, pindexPrevNew, nExtraNonce);
        }

        //! Now we can set, and know its current the previous blockindex pointer value.
        //! This need to be updated only after we know CreateNewBlock succeeded
        pindexPrev = pindexPrevNew;

        //! Save the block pointer with a key on the MerkleRoot hash, this is how we will know that is the one being
        //! returned later from an external miner if it finds a result.
        mapNewBlock[pblock->hashMerkleRoot] = pblock;
        LogPrintf( "GetWork MerkleRoot hash saved as 0x%s\n", pblock->hashMerkleRoot.ToString() );
        LogPrintf( "GetWork Previous hash saved as 0x%s\n", pblock->hashPrevBlock.ToString() );

        /**
         * Pre-build hash buffers
         */
        char pmidstate[32];
        char pdata[128];
        char phash1[64];
        FormatHashBuffers(pblock, pmidstate, pdata, phash1);

        uint256 hashTarget;
        hashTarget.SetCompact(pblock->nBits);

        Object result;
        result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate)))); // deprecated
        result.push_back(Pair("data",     HexStr(BEGIN(pdata), END(pdata))));
        result.push_back(Pair("hash1",    HexStr(BEGIN(phash1), END(phash1)))); // deprecated
        result.push_back(Pair("target",   HexStr(BEGIN(hashTarget), END(hashTarget))));
        return result;
    }
    else
    {
        //! Parse the one hex string parameter returned from the miner, 1st we make sure it is hex
        if( !IsHex(params[0].get_str()) )
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Parameter must be hex");

        vector<unsigned char> vchData = ParseHex(params[0].get_str());
        //! Its size must be exactly right too...
        if (vchData.size() != 128)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Parameter size mis-match");

        //! Byte reverse the words, and then the words themselves, as that was how the work was sent to it.
        uint32_t* pdata = (uint32_t*)&vchData[0];
        for (uint8_t i = 0; i < 128/4; i++)
            pdata[i] = ByteReverse(pdata[i]);

        //! Previous code here was bad pointer arithmetic & broke this operation, because it 'assumed' our 'CBlock' structure was
        //! layed out a specific way.  As Anoncoin now has an advanced pre calculated scrypt and shad256 hash subsystem within its
        //! CBlock class structure than, assumption was no longer valid and the following replaces it with 'no' such assumption made.
        //! And with much better exception handling, if there was a problem with the string that as yet gone undetected.
        CBlockHeader aBlockHeader;
        CDataStream ssBlock(vchData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssBlock >> aBlockHeader;
        }
        catch (const exception&) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Unable to de-serialize block");
        }

        // If that MerkleRoot is still found in our NewBlock map, we have a chance to save a new mined block...
        // LogPrintf( "GetWork MerkleRoot hash retrieved from miner 0x%s\n", aBlockHeader.hashMerkleRoot.ToString() );
        // LogPrintf( "GetWork Previous hash retrieved as 0x%s\n", aBlockHeader.hashPrevBlock.ToString() );
        if( !mapNewBlock.count(aBlockHeader.hashMerkleRoot) )
            throw JSONRPCError(RPC_INVALID_PARAMETER, "MerkleRoot hash was not found");

        CBlock* pblock = mapNewBlock[aBlockHeader.hashMerkleRoot];

        //! Make sure this block can still be added as the next new one for the active chain.
        {
            LOCK(cs_main);
            if( pblock->hashPrevBlock != chainActive.Tip()->GetBlockSha256dHash() )
                throw JSONRPCError(RPC_VERIFY_REJECTED, "generated block is stale");
        }
        if( pRetargetPid->UsesHeader() ) {
            if( pblock->nTime != aBlockHeader.nTime ) {
                pblock->nTime = aBlockHeader.nTime;
                LogPrintf( "WARNING - getwork results may not match NextWorkRequired, your miner changed the block time. nBits must be re-calculated when that is done.\n");
            }
        }
        pblock->nNonce = aBlockHeader.nNonce;
        // This code is not needed, as the block state is already correct in memory
        //CMutableTransaction TxNew( pblock->vtx[0] );
        //TxNew.vin[0].scriptSig = mapNewBlock[aBlockHeader.hashMerkleRoot].second;
        //pblock->vtx[0] = TxNew;
        //pblock->hashMerkleRoot = pblock->BuildMerkleTree();

        // Remove the hash so another miner can not submit the same duplicate work
        // mapNewBlock.erase(aBlockHeader.hashMerkleRoot);

        //! Calling GetHash with true, invalidates any previously calculated hashes for this block, as they have changed
        uint256 aRealHash = pblock->GetHash(true);
        //! Force both hash calculations to be updated
        assert( pblock->CalcSha256dHash(true) != uintFakeHash(0) );

        bool fBlockPresent = false;
        {
            LOCK(cs_main);
            BlockMap::iterator mi = mapBlockIndex.find(aRealHash);
            if( mi != mapBlockIndex.end() ) {
                CBlockIndex *pindex = mi->second;
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                // Otherwise, we might only have the header - process the block before returning
                fBlockPresent = true;
            }
        }
        CValidationState state;
        submitblock_StateCatcher sc(aRealHash);
//        RegisterValidationInterface(&sc);
        bool fAccepted = ProcessNewBlock(state, NULL, pblock);
//        UnregisterValidationInterface(&sc);
        if( fAccepted ) {
            // Remove key from key pool
            pMiningKey->KeepKey();
            sc.found = true;
        }

        if (fBlockPresent)
        {
            if (fAccepted && !sc.found)
                return "duplicate-inconclusive";
            return "duplicate";
        }
        if (fAccepted)
        {
            if (!sc.found)
                return "inconclusive";
            state = sc.state;
        }

        // Track how many getdata requests this block gets
        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->mapRequestCount[pblock->CalcSha256dHash()] = 0;
        }
        return BIP22ValidationResult(state);
    }
}

Value getretargetpid(const Array& params, bool fHelp)
{
    if( fHelp || params.size() > 2 )
        throw runtime_error(
            "getretargetpid [height] [verbose]\n"
            "\n Provides detailed information on the Retarget P-I-D controller state and proof-of-calculation results.\n"
            "\n If no parameter is specified, the argument given is 0, or the height greater than the current index...\n"
            " Then the current tip height is used with a new header based on the present moment.  Retarget data is created,\n"
            " based on that possible new block time, and the results returned to you.\n"
            "\n If height = 1 only the set of generic constants for the RetargetPID controller are returned.\n"
            " Otherwise the height given must be > the Tip Filter size and calculations are made for the block at that given\n"
            " height.  This is based primarily on its previous block index entries.  Proof-Of-Work will then match that block\n"
            " exactly, as must have been done by the miner, and again whenever the blockchain is being initially loaded by you.\n"
            "\nArguments:\n"
            "1. height                       (numeric, optional, default=0) for heights > then the HARDFORK block, these validate the chain.\n"
            "2. verbose                      (boolean, optional, default=false) If true, the raw tipfilter data points will also be returned.\n"
/*            "2. algo  Future releases may include a 2nd parameter, multi-algo means multiple PIDs, a master, and one for each algo.\n" */
            "\nReturned when: (height >= 1 and height <= 'tipfiltersize')\n"
            "Result: The Constant RetargetPID parameters used to calculate difficulty adjustments.\n"
            "{\n"
            "  \"proportionalgain\" : n         (numeric) The Proportional gain of the control loop.\n"
            "  \"integratortime\" : n           (numeric) The Integrator Time Constant, in seconds.\n"
            "  \"derivativegain\" : n           (numeric) The Derivative gain of the control loop.\n"
            "  \"targetspacing\" : n            (numeric) The Anoncoin block time spacing target, in seconds.\n"
            "  \"usesheader\" : true|false      (bool)    True if the header block time is considered as part of the Tip Filter calculations.\n"
            "  \"tipfiltersize\" : n            (numeric) The number of blocks used to calculate, previous difficulty, spacing and rate of change errors at the tip.\n"
            "  \"maxdiffincrease\" : n          (numeric) The previous difficulty, divided by this maximum increase is the most change any 1 block can have.\n"
            "  \"maxdiffdecrease\" : n          (numeric) The previous difficulty, multiplied by this maximum decrease is the most change any 1 block can have.\n"
            "  \"prevdiffweight\" : n           (numeric) Total weight sum used for the final previous difficulty calculation.\n"
            "  \"spaceerrorweight\" : n         (numeric) Total weight sum used for the final block spacing error calculation.\n"
            "  \"ratechangeweight\" : n         (numeric) Total weight sum used for the final rate of change calculation.\n"
            "}\n"
            "\nReturned when: (no param given, height < 1 or height > 'tipfiltersize')\n"
            "Result: The dynamically computed values for the block at the given height, if 'usesheader' is true the specified 'retargettime' is also used.\n"
            "{\n"
            "  \"retargetheight\" : n           (numeric) The block height for which the calculations are being run at.\n"
            "  \"allowmintime\" : n             (numeric) The minimum timestamp allowed for the block.\n"
            "  \"retargettime\" : n             (numeric) The timestamp for which these calculations were run.\n"
            "  \"adjustedtime\" : n             (numeric) The system adjusted timestamp for this moment.\n"
            "\nWhen [verbose] is true           (array of strings) is also returned.\n"
            " The Tip Filters raw data is based on block index entries and first sorted by block times before any processing is done.\n"
            " If 'usesheader' is true, then the next blocks time is included and 'tipfiltersize' grows by 1. The number of entries is\n"
            " based on the size of the filter and because of its complexity, only one formatted string is used for each line.\n"
            " Each entry consists of multiple fields: the block time, the difficulty, and for entries greater than the first, the\n"
            " spacing and spacing errors. For entries greater than the 2nd, a rate of change is also calculated for the derivative term.\n"
            " Each difficulty, spacing error and rate of change value is followed by a number in (), which defines the weight used as a\n"
            " multiplier on that value to produce a final sum.\n"
            "\n Following this table, you will also see weight totals in () on the final previous difficulty, spacing error and rate of\n"
            " change calculations. These weights were used as a divider on the sum of all the Tip Filter entries, to produce the value\n"
            " seen. 'spacingerror' and 'rateofchange' define the inputs used to calculate controller P and D terms, combined with the\n"
            " Integrator, a P-I-D output result is created. The 'prevdiff' can then be used to calculate a new difficulty target. Four\n"
            " different limit bound checks will also have been made.\n"
            "\n  \"tipfilter\" : [\n"
            "      \"BlockTime Difficulty(Weight)\",\n"
            "      \"BlockTime Difficulty(Weight) Spacing SpacingError(Weight)\",\n"
            "      \"BlockTime Difficulty(Weight) Spacing SpacingError(Weight) RateOfChange(Weight)\",\n"
            "      ,...\n"
            "  ],\n"
            "\n  \"prevdiff\" : n(n)              (hexdigits)(weight) Previous difficulty value computed from the weighted tip filter result, represented as a compact number.\n"
            "  \"spacingerror\" : n(n)          (numeric)(weight) The Tip Filter calculated spacing error and used directly by the P term calculation.\n"
            "  \"rateofchange\" : n(n)          (numeric)(weight) The Tip Filters calculated rate of change in block spacing used by the D term.\n"
            "  \"integratorheight\" : n         (numeric)   Block index height for which the Integrator charge is set.\n"
            "  \"chargetime\" : n               (numeric)   The seconds for which the Integrator considered block spacing, this will be as close to the constant as possible.\n"
            "  \"integratorblocks\" : n         (numeric)   The number of blocks for which the Integrator was charged.\n"
            "  \"proportionterm\" : n           (numeric)   Final Proportional Term calculation after multiplying the gain by 'spacingerror'.\n"
            "  \"integratorterm\" : n           (numeric)   Final Integrators computed output block time, this is based on 'chargetime' / ('integratorblocks' - 1).\n"
            "  \"derivativeterm\" : n           (numeric)   Final Derivative Term calculation after multiplying its gain by 'rateofchange'.\n"
            "  \"pidoutputtime\" : n            (numeric)   The final computed PID output control value required to correct block time error.\n"
            "  \"prevdiffx256\" :               (hexdigits) This is also the previous difficulty, represented as the complete 256 bit hex value, used in the calculation.\n"
            "  \"hitlimits\" : true|false       (bool)      If any limits have been tripped, this field will indicate true, otherwise it is false.\n"
            "\n NOTE: nextdiff seen here 3 different ways, is defined as 'prevdiff' x 'pidoutputtime' / 'targetspacing'.  Limit bounds have been checked, and if necessary applied.\n"
            "  \"nextdiffbits\" : n             (hexdigits) The actual nBits field value which would need to be included in the mined block.\n"
            "  \"nextdifflog2\" : n             (numeric)   This same value is based on a work proof and provided for logarithm scale analysis.\n"
            "  \"nextdiffx256\" : n             (hexdigits) The calculated new target difficulty as a full 256 bit hexadecimal value.\n"
            "\n  \"tipspacing\" : n               (numeric)   A simple average of the TipFilters block spacings, computed and shown here for you in seconds.\n"
            "  \"blkspacing\" : n               (numeric)   Based only on block time and the previous index entry, this spacing in seconds could be positive or negative.\n"
            "  \"prevshad\" : n                 (hexdigits) The sha256d hash of the previous block, which would also need to be included in a block for this height.\n"
            "}\n"
            "\nExamples: Returns the constants upon which the retarget system is operating.\n"
            + HelpExampleCli("getretargetpid", "1") +
            "\nExamples: Returns the calculations for the block at height 250000.\n"
            + HelpExampleCli("getretargetpid", "250000") +
            "\nExamples: Returns  the calculations for block at the tip of chain right now.\n"
            + HelpExampleRpc("getretargetpid", "") +
            "\nExamples: Returns  the calculations for block at the tip of chain right now, and includes all the raw tipfilter data points.\n"
            + HelpExampleCli("getretargetpid", "0 true") +
            "\nExamples: Returns the calculations for the block at height 375000 and also includes the raw tipfilter data.\n"
            + HelpExampleRpc("getretargetpid", "375000 true")
        );

    LOCK(cs_main);

    RPCTypeCheck(params, boost::assign::list_of(int_type)(bool_type));

    if( pRetargetPid == NULL )
        throw JSONRPCError(RPC_INTERNAL_ERROR, "RetargetPID has not been initialized");

    uint32_t nHeight = 0;
    if (params.size() > 0) {
        nHeight = params[0].get_int();
    }
    bool fVerbose = false;
    if (params.size() > 1) {
        fVerbose = params[1].get_bool();
    }

    bool fStaticValues = nHeight == 1;
    const CBlockIndex* pIndexAtTip = chainActive.Tip();
    int64_t nTimeNow = GetAdjustedTime();

    RetargetStats RetargetState;
    bool fStatsResult = pRetargetPid->GetRetargetStats( RetargetState, nHeight, pIndexAtTip );
    if( !fStatsResult ) fStaticValues = true;       // All we can report are the constants

    Object result;
    if( fStaticValues ) {
        result.push_back(Pair("proportionalgain", (double)RetargetState.dProportionalGain));
        result.push_back(Pair("integratortime", (int64_t)RetargetState.nIntegrationTime));
        result.push_back(Pair("derivativegain", (double)RetargetState.dDerivativeGain));
        result.push_back(Pair("targetspacing", (int64_t)nTargetSpacing ));
        result.push_back(Pair("usesheader", (bool)RetargetState.fUsesHeader));
        result.push_back(Pair("tipfiltersize", (int64_t)RetargetState.nTipFilterSize));

        result.push_back(Pair("maxdiffincrease", (int64_t)RetargetState.nMaxDiffIncrease));
        result.push_back(Pair("maxdiffdecrease", (int64_t)RetargetState.nMaxDiffDecrease));

        result.push_back(Pair("prevdiffweight", (int64_t)RetargetState.nPrevDiffWeight));
        result.push_back(Pair("spaceerrorweight", (int64_t)RetargetState.nSpacingErrorWeight));
        result.push_back(Pair("ratechangeweight", (int64_t)RetargetState.nRateChangeWeight));
    } else {
        result.push_back(Pair("retargetheight", (uint64_t)nHeight));
        result.push_back(Pair("allowmintime", (int64_t)RetargetState.nMinTimeAllowed));
        result.push_back(Pair("retargettime", (int64_t)RetargetState.nLastCalculationTime));
        result.push_back(Pair("adjustedtime", nTimeNow));

        if( fVerbose ) {
            bool fHeaderEntry;
            uint32_t nBitsWeight = 0;
            FilterPoint aFilterPoint;
            Array TipFilterData;
            for( int32_t i = 0; i < RetargetState.nTipFilterSize; i++ ) {
                aFilterPoint = RetargetState.vTipFilter[ i ];
                fHeaderEntry = aFilterPoint.nDiffBits == 0;
                string sPoint = strprintf( "%d %08x(%02u)", aFilterPoint.nBlockTime, aFilterPoint.nDiffBits, fHeaderEntry ? 0 : ++nBitsWeight );
                if( i > 0 ) {
                    sPoint += strprintf( " %4d %+4d(%02u)", aFilterPoint.nSpacing, aFilterPoint.nSpacingError, i );
                    if( i > 1 )
                        sPoint += strprintf( " %+4d(%02u)", aFilterPoint.nRateOfChange, i - 1 );
                }
                TipFilterData.push_back(sPoint);
            }
            result.push_back(Pair("tipfilter", TipFilterData));
        }

        string sFormat;
        sFormat = strprintf( "%08x(%u)", RetargetState.uintPrevDiff.GetCompact(), RetargetState.nPrevDiffWeight );
        result.push_back(Pair("prevdiff", sFormat));

        sFormat = strprintf( "%+d(%u)", RetargetState.dSpacingError, RetargetState.nSpacingErrorWeight );
        result.push_back(Pair("spacingerror", sFormat));

        sFormat = strprintf( "%+d(%u)", RetargetState.dRateOfChange, RetargetState.nRateChangeWeight );
        result.push_back(Pair("rateofchange", sFormat));

        result.push_back(Pair("integratorheight", (uint64_t)(RetargetState.nIntegratorHeight)));
        result.push_back(Pair("integrationtime", (int64_t)RetargetState.nIntegratorChargeTime));
        result.push_back(Pair("integratorblocks", (uint64_t)RetargetState.nBlocksSampled));

        result.push_back(Pair("proportionterm", (double)RetargetState.dProportionalTerm));
        result.push_back(Pair("integratorterm", (double)RetargetState.dIntegratorTerm));
        result.push_back(Pair("derivativeterm", (double)RetargetState.dDerivativeTerm));
        result.push_back(Pair("pidoutputtime", (double)RetargetState.dPidOutputTime));
        result.push_back(Pair("prevdiffx256", RetargetState.uintPrevDiff.ToString()));

        result.push_back(Pair("hitlimits", (bool)(RetargetState.fPidOutputLimited || RetargetState.fDifficultyLimited)));

        sFormat = strprintf( "%08x", RetargetState.uintTargetDiff.GetCompact() );
        result.push_back(Pair("nextdiffbits", sFormat));
        result.push_back(Pair("nextdifflog2", (double)GetLog2Work(RetargetState.uintTargetDiff)));
        result.push_back(Pair("nextdiffx256", RetargetState.uintTargetDiff.ToString()));

        result.push_back(Pair("tipspacing", (double)RetargetState.dAverageTipSpacing));
        result.push_back(Pair("blkspacing", (int64_t)RetargetState.nBlockSpacing));
        result.push_back(Pair("prevshad", RetargetState.uintPrevShad.ToString()));
    }

    return result;
}
