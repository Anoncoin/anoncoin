// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"

#include "main.h"
#include "uint256.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/foreach.hpp>

namespace Checkpoints
{
    typedef std::map<int, uint256> MapCheckpoints;

    // How many times we expect transactions after the last checkpoint to
    // be slower. This number is a compromise, as it can't be accurate for
    // every system. When reindexing from a fast disk with a slow CPU, it
    // can be up to 20, while when downloading from a slow network with a
    // fast multicore CPU, it won't be much higher than 1.
    static const double SIGCHECK_VERIFICATION_FACTOR = 5.0;

    struct CCheckpointData {
        const MapCheckpoints *mapCheckpoints;
        int64_t nTimeLastCheckpoint;
        int64_t nTransactionsLastCheckpoint;
        double fTransactionsPerDay;
    };

    bool fEnabled = true;

    // What makes a good checkpoint block?
    // + Is surrounded by blocks with reasonable timestamps
    //   (no blocks before with a timestamp after, none after with
    //    timestamp before)
    // + Contains no strange transactions
    static MapCheckpoints mapCheckpoints =
        boost::assign::map_list_of
        ( 1, uint256("0x8fe2901fc0999bc86ea2668c58802ee87165438166d18154f1bd4f917bf25e0f"))
        ( 7, uint256("0x4530df06d98fc77d04dab427630fc63b45f10d2b0ad3ad3a651883938986d629"))
        ( 7777, uint256("0xae3094030b34a422c44b9832c84fe602d0d528449d6940374bd43b4472b4df5e"))
        (15420, uint256("0xfded6a374d071f59d738a3009fc4d8461609052c3e7e91aa89146550d179c1b0"))
        (16000, uint256("0x683517a8cae8530f39e636f010ecd1750665c3d91f57ba71d6556535972ab328"))
        (77777, uint256("0xf5c98062cb1ad75c792a1851a388447f0edd7cb2271b67ef1241a03c673b7735"))
        (77778, uint256("0xd13f93f9fdac82ea26ed8f90474ed2449c8c24be50a416e43c323a38573c30e5"))
        (100000, uint256("0xcc4f0b11e9e17f7a406ac4a71e6e192b9b43e32b300ddecba229c789392497eb"))
        (125000, uint256("0x52a933553dda61b4af6a418f10ce563f0c84df93619ec97f8f360f9ecc51cf5b"))
        (150000, uint256("0xb1dc241da3e8e7d4ca2df75187d66f4f096840634667b987a7b60d5dde92853c"))
        (210000, uint256("0xb9c2c5030199a87cb99255551b7f67bff6adf3ef2caf97258b93b417d14f9051"))
        (314123, uint256("0x45236d336950cb1ce679b88d03ed6ceb26cbcdd85f0e3ac4c98e7e005962f65c"))
        (314777, uint256("0x43025b1f235f76621bb71b801fd6721c49ea3aedcbb194c4f3d25998adb2fd4d"))
        (377777, uint256("0xf548661d5c74e759c7d6df40bc08392bbc00f1a2b02e59fd905b052dfe37062a"))
        (444444, uint256("0x8a3a51b07002f89137c0f11ebc44dff44df892604f2d747dad5c20d8e9e937c2"))
        (444888, uint256("0xbe18e09ef49954796004654c18a7f8c765cd71afc28a63b682588388dc4575a1"))
        (530000, uint256("0x50886d5daf308449e9880ccdb2d19411bc27f7c8e91e7f6dd7c997fba17277b6"))
        (557106, uint256("0xc959f6d9caea281d70fa8ba5708b7f79ac683f519adfd671a8f13302823199ae"))
        (557107, uint256("0xbdf257c9cd9f7c2e7f356dc47d825c0ca82cd010f1f5a96888c66da359eee906"))
        (557108, uint256("0x48309248260ca896cd092e7192a0da5b228c00e337be5c59d1f61499ef67c425"))
        (588888, uint256("0x75252a2130da5560c7fb844c8635d68154817d29f8b56e5f4fe065d29003c0cb"))
        (588889, uint256("0xa4622868ce0baa68d94a7a2673abf8bb9546b30aef6ab459b6f3ec0c1049687d"))
        (588890, uint256("0xdca582a28fda7c117d3f45fa139808d2e3e84c23f4372b9d553ac5c1e4cf1b88"))
        ;

    static const CCheckpointData data = {
        &mapCheckpoints,
        1463198622, // * UNIX timestamp of last checkpoint block
        808785,     // * total number of transactions between genesis and last checkpoint
                    //   (the tx=... number in the SetBestChain debug.log lines)
         580.0      // * estimated number of transactions per day after checkpoint
    };

    // Testnet or RegTest checkpoints, same as the genesis block,
    // Anoncoin est. tx/day = 480 blocks x 1 transaction for 3min/block
    static MapCheckpoints mapCheckpointsTestnet =
        boost::assign::map_list_of
        ( 0, uint256("0x66d320494074f837363642a0c848ead1dbbbc9f7b854f8cda1f3eabbf08eb48c"))
        ;
    static const CCheckpointData dataTestnet = {
        &mapCheckpointsTestnet,
        1373625296,
        0,
        480
    };

    // Initial checkpoint map for Anoncoin Regression tests is the genesis block
    static MapCheckpoints mapCheckpointsRegtest =
        boost::assign::map_list_of
        ( 0, uint256("0x03ab995e27af2435ad33284ccb89095e6abe47d0846a4e8c34a3d0fc2d167ceb"))
        ;
    static const CCheckpointData dataRegtest = {
        &mapCheckpointsRegtest,
        1296688602,
        0,
        480
    };

    const CCheckpointData &Checkpoints() {
        if (BaseParams().GetNetworkID() == CBaseChainParams::TESTNET)
            return dataTestnet;
        else if (isMainNetwork())
            return data;
        else
            return dataRegtest;
    }

    bool CheckBlock(int nHeight, const uintFakeHash& hash)
    {
        if (!fEnabled)
            return true;

        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;

        MapCheckpoints::const_iterator i = checkpoints.find(nHeight);
        if (i == checkpoints.end()) return true;
        return hash == i->second;
    }

    // Guess how far we are in the verification process at the given block index
    double GuessVerificationProgress(CBlockIndex *pindex, bool fSigchecks) {
        if (pindex==NULL)
            return 0.0;

        int64_t nNow = time(NULL);

        double fSigcheckVerificationFactor = fSigchecks ? SIGCHECK_VERIFICATION_FACTOR : 1.0;
        double fWorkBefore = 0.0; // Amount of work done before pindex
        double fWorkAfter = 0.0;  // Amount of work left after pindex (estimated)
        // Work is defined as: 1.0 per transaction before the last checkpoint, and
        // fSigcheckVerificationFactor per transaction after.

        const CCheckpointData &data = Checkpoints();

        if (pindex->nChainTx <= data.nTransactionsLastCheckpoint) {
            double nCheapBefore = pindex->nChainTx;
            double nCheapAfter = data.nTransactionsLastCheckpoint - pindex->nChainTx;
            double nExpensiveAfter = (nNow - data.nTimeLastCheckpoint)/86400.0*data.fTransactionsPerDay;
            fWorkBefore = nCheapBefore;
            fWorkAfter = nCheapAfter + nExpensiveAfter*fSigcheckVerificationFactor;
        } else {
            double nCheapBefore = data.nTransactionsLastCheckpoint;
            double nExpensiveBefore = pindex->nChainTx - data.nTransactionsLastCheckpoint;
            double nExpensiveAfter = (nNow - pindex->nTime)/86400.0*data.fTransactionsPerDay;
            fWorkBefore = nCheapBefore + nExpensiveBefore*fSigcheckVerificationFactor;
            fWorkAfter = nExpensiveAfter*fSigcheckVerificationFactor;
        }

        return fWorkBefore / (fWorkBefore + fWorkAfter);
    }

    int GetTotalBlocksEstimate()
    {
        if (!fEnabled)
            return 0;

        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;

        return checkpoints.rbegin()->first;
    }

    CBlockIndex* GetLastCheckpoint()
    {
        if (!fEnabled)
            return NULL;

        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;

        BOOST_REVERSE_FOREACH(const MapCheckpoints::value_type& i, checkpoints)
        {
            const uintFakeHash& aFakeHash = i.second;
            const uint256 aRealHash = aFakeHash.GetRealHash();
            BlockMap::const_iterator t = ( aRealHash != 0 ) ? mapBlockIndex.find( aRealHash ) : mapBlockIndex.end();
            if (t != mapBlockIndex.end())
                return t->second;
        }
        return NULL;
    }
}
