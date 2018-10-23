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
        (557109, uint256("0x753697d170c477e0e3e1e5fa03d9ef2fc292fc65a04d6ddea8dc5d115f6b7ceb"))
        (557110, uint256("0x453fecdc3f504f68f6498483687db46723c37d42af3c59b0d2404009ea1e5fe4"))
        (557111, uint256("0xa9234065a74c4bf605e91bc18209c75e55b840bf95246091a93fd7859c04eeb9"))
        (557112, uint256("0x59ff6d25483d848e5612316907677020be0b44423f40acb465a1dde3529e8dd5"))
        (557113, uint256("0x21bc78dd81adbc0f2e9874b27f140239b285809317496cca1f135e05ee55479b"))
        (557114, uint256("0x2d4fd5bee808651208ed3dbeb85db531f898a7f46f935fc63ce2e95cd1a09047"))
        (557115, uint256("0xea2582fcd6605086a9e482c076bb9d557bdebcc5b20094e4f5c27311291143e8"))
        (557116, uint256("0x982f1a851828232acd8ebfeedd2f54349b62035148915084531722fd805ce209"))
        (557117, uint256("0x47bb8cdb06547df33d11f3a1f7fe47657c7e020964e533fd7ed51e6fb0e9c929"))
        (557118, uint256("0xc98e471b034b716fe7c07cf3dd65d1f7901607e1e065505b6cb1a739edd44505"))
        (557119, uint256("0x14c8b79f913cdf04b1e670ca459e9156ad340361f853913873baf71148fdac30"))
        (557120, uint256("0xd27e6b4ee6e70fd533a195f7e02dcd986df7ea78ba62c8c1bce129fb9532ec4f"))
        (557121, uint256("0x8d43d41fe48d8fcb0ff437dc1ccf036b48440456713f52aaed52bc4d169800de"))
        (557122, uint256("0xff2398e8caf2476683549bb15d5904e69174ce5675290426e7b3bb09505e5bb6"))
        (557123, uint256("0x13b702e6cd45899c2ff8e9d2bb64e19536bb8ae4e91da0f25ab4f690fd68ead4"))
        (557137, uint256("0xd1c2c3842f9119a7cf048c8988a74e41f5c0209ba38c653c744d38a477a15769"))
        (557138, uint256("0x531a8e3df1fbb76a35a70cad0295dafcdccb5a49cc9d27fd5b8f83ffd279545d"))
        (557139, uint256("0xaba8fdda12f7dad02c300174f1842c041119950c0f1422b6cc9063f96ffa15fc"))
        (557140, uint256("0x21ec1dcfb19e6073f0b8cd518da4c9d73eea27016e4a10ded9561c80f2a9b38b"))
        (557141, uint256("0x4be6c25bb4ef8a154f7ec1e1587ab36a948330b8be5552de37add969817cc90f"))
        (557142, uint256("0x8f389ee68683920659c18f2b482aff20e4fb69947fd43a1d2c98b133fe41534b"))
        (557143, uint256("0xfaa7d906db811f1bc33170d85857cb26b2eafe7505c6b3137d592791750ac684"))
        (557144, uint256("0x3033a118ce7997e603024b0f50c740fa47838e3725f8599c26147a0c6bd87977"))
        (557169, uint256("0x3bcd8e3fddc80d3059e7411d275f98de09865f4eeaa8fb942d15e64abce1cd17"))
        (557170, uint256("0x7be5e79ac2169184290c5c20ee6f39de904db0cd645a621cb80827268977e74a"))
        (585596, uint256("0x0d6a17d985a6fa9efea87c72003a9f0fbbbfedf9c12f72a31c31b1b2b705bcbc"))
        (585597, uint256("0x0f8b140890c1efe6b913e8632708fee80ce1a647db77e1374f0e3ee7d8d88748"))
        (585598, uint256("0xe3cf10a6a0d2ecc70d2c373371f66e12e03c5010b4d1678296f4366135a8dee0"))
        (585555, uint256("0x2799a4939cee3523e510904a755e892c57734154e0bf67387a2cb579fefa7c48"))
        (585556, uint256("0xef910cb250db66a8879f826e3fe23a3a6552cb37601331ec7914221cd9f73548"))
        (628918, uint256("0x7732f13d7fd9906c35b6b25e033c4e446a7da18e670f4287ed33ac206cdc9bb0"))
        (640913, uint256("0x747e790f030ca0506fd46b18337ba50516dd4e610e97104052a32def06e736ac"))
        (640914, uint256("0xc5f894032f2598ee3d68eb67cd3d6eed7fd19bc2706013ddb4ec52d710bc556d"))
        (640915, uint256("0x908cf47026b3c0242aa2c99c55be3148bab8afc90845593109c0fea66f7febba"))
        (689599, uint256("0x315c44e22e3f474da0164f6e1008ace50f26a87d700ef238e7294e92e0073eac"))
        (690315, uint256("0x603c9d2b34e4f50c829ff248a583596f83e44cd8c71f23251cf1ce23dbb3bbda"))
        (690676, uint256("0xd78066fcec71f509aa8d29190630f57ad4b5602266072e2e396f783ad06353f6"))
        (690678, uint256("0x6b436b8b22f718746a8520714b75450687672b46a19919a4eead14c90787f1fe"))
        (744631, uint256("0x3f7db18baef8181a07fbd9024d6c2b8a8f1f3ef7d238b3ca9ae537ab1f5ae3c9"))
        (744632, uint256("0xb07a656e4282a592ce76461eead6a9ca29d4291b663ae3333acce232ac2f7109"))
        (744635, uint256("0x05470ee7bd7b7d2475768dc8aadbf40c6c57f29f1b3a9ded5130a7b2de31ba1c"))
        (744964, uint256("0x89733e9232f96e2af3c1daf77a2a98b751b378cd61b962e0d0d8b71c0b7266a5"))
        (744965, uint256("0xcfcc578269cb20432e24d0ac033a945d999c6abb8bddc2fe9140217a0957ab41"))
        (744966, uint256("0x13728009fe91265ec04882f9ff64a7eae6db1730ca9b56b2309145cd65f3da1c"))
        (745045, uint256("0xa1d3a33fff91e6a4bb372fa495662a98ccd131b69231fdfe2567824b299560aa"))
        (745397, uint256("0xd864bc0469005091e970197b7f8fd3ef389e3342f2a00ebf492476af5bf31ee6"))
        (746687, uint256("0x6467a8b1fa796ce1f6b1fca601846d17994d0b792746d5c839cda79d64451358"))
        (746841, uint256("0x1c90201dff45eaf6bf9b2c2daf8def6e733a8ba5c3bb4ca6104ccd78fe62c13f"))
        (746842, uint256("0x0b55a770f8ee39f48abb617367e6930282bf1c88aa7e7ae6913cd4c7744a5f0b"))
        (746843, uint256("0x48645ed323be17ffff6a90dcf0a1a962140eb6e7fde18cbafca6c7c2db2e39af"))
        (746844, uint256("0x71fbbdf9fcbe8848ef8dae9f5ee99bca7accafbea9db8356294e3e31696ba651"))
        (746845, uint256("0xfd84b1d99f0bbde379e10631ad66f7f16f9a15dfe270bfee45469bd2990f62e0"))
        (746846, uint256("0x29073a82dacf129a3ab8723d9100406871311b637a516516fd733c73f080259d"))
        (772322, uint256("0xfa2265ae9b2848a44169cdaf17751954f4d67a1c9eeed73ce6d3b13c83ed00b2"))
        (773393, uint256("0x1d4aab92092ee905b9fb41d0cdd52c56e1c056e0fc37288ecbe3102daab21f30"))
        (773774, uint256("0x347374f00b439a301331b1664bf45c82fd62dd1a0e946866e17140afcb575c58"))
        (773775, uint256("0x6ea4b801fb56b53fa214f4c67d683394b5a1355212ef171e15ddf46f42296c3a"))
        (784561, uint256("0x61848c094e8494119bc3403122d95c1d82153e55f0909f69e3649b7718a8ff6d"))
        (799679, uint256("0xe9b9d60614ac67944270b5c58f7f871cf3603cbd51a2870150980f4905e4492d"))
        (810845, uint256("0x6a998d0e948d9eb2a4a34766893f9bcf0e9df6b0f60a771f37f8c16d0011230a"))
        (810847, uint256("0xc7089ea0e480e4d18c926d0cccbf5270dde93da227b84026c412bcf1e0153184"))
        (810848, uint256("0x85d9accfe2a6df6c2352beb9ceed57b3e6d9c511b2fd1dcfeb37db409e4a9113"))
        (810849, uint256("0x16053b84c7cc25c294883712b8054f5cb6edb75870c1e959da5683a1d7c7c8d2"))
        (813339, uint256("0xce2755d2b791f70d6b7131078157e139437a25f5ea9a720a6d505f45ad966c89"))
        (813340, uint256("0x8b78db8c8c7d01a6c11ced86ef6522ea6e710da6ec5b31c7896962e425451076"))
        (813341, uint256("0xe5281e033e5be50d3e19f5c8783747ca7c5bd80f32f48003c9dc84bc82434e2f"))
        (842287, uint256("0x717b92a047d96ea0746c0f2057c06f2cf760f5a746bf623be58c6be5c17780ee"))
        (871111, uint256("0x000009bcebb6d627c7bfd7857b33d0e1a2b0e69a42c6100f5434f8cb76e050aa"))
        (871112, uint256("0x000000017cc6d6ffa393a93a6956e4d2cb76a49066981b4f1f85d751abd18504"))
        (888000, uint256("0x0000000001070042e6a4f26b3629f8d7dc12db60348e3cdb6a3f4b912c1075c6"))
        (895000, uint256("0x0000000000ab6bce9f1aa8e5d1907e0c486ed9f7e56d12713e365bfd9ef00550"))
        (900000, uint256("0x0000000000c6ec80bd71952e35804e61cb27246717f518d06d409949d15a7890"))
        (916198, uint256("0x00000000005a4b5a3bd9fd45f6327051830187a037cc427c33d4fc0608edcbf1"))
        (916199, uint256("0xb530d9d9e703f6bbddb56b2a877b6b9ef43122f6eec9a37a9fde8316fa3d55a1"))
        (916235, uint256("0x16b166763308d97e9390351a2904ed7ba99fbaf0ce45f8c80f60966c8e859df3"))
        (916243, uint256("0xe4d6005bac1d1990812065aa1c67d2f6623f75120f4e1b8513a01716ef790851"))
        (916267, uint256("0x33944f530ec68b2a683a9a97abd8f78576407edb69e1bf3da6c8fde460049828"))
        (916270, uint256("0xdeb8b72decf97f301260da0afde8b29cfb5ab4c541a99eab6fe557486310639f"))
        (916281, uint256("0xadcec3ee4ef1dbb2f5664b563e4b50b2c9f957b33fa9c65ec4c7a988ed2af87d"))
        (916282, uint256("0x00000000006227c1cbdbff2555a0bfa691dfcc214db78ba0ab4f96bfc26a4d10"))
        (917037, uint256("0x00000000007b79bafabb5eab76f955734661cdcd20dd3e253b4201f2aef301aa"))
        (917038, uint256("0xf4682db29bf76fbc2b7b8dd2064a1c646bc8c08b281db579560ee27f0a7ae7b9")) 
        ;
    
    // Blocks with exceptions that don't pass PoW checks
    uint256 gExceptionBlocks [7] = {
            uint256("0xadcec3ee4ef1dbb2f5664b563e4b50b2c9f957b33fa9c65ec4c7a988ed2af87d"),
            uint256("0xdeb8b72decf97f301260da0afde8b29cfb5ab4c541a99eab6fe557486310639f"),
            uint256("0x33944f530ec68b2a683a9a97abd8f78576407edb69e1bf3da6c8fde460049828"),
            uint256("0xe4d6005bac1d1990812065aa1c67d2f6623f75120f4e1b8513a01716ef790851"),
            uint256("0x16b166763308d97e9390351a2904ed7ba99fbaf0ce45f8c80f60966c8e859df3"),
            uint256("0xb530d9d9e703f6bbddb56b2a877b6b9ef43122f6eec9a37a9fde8316fa3d55a1"),
            uint256("0xf4682db29bf76fbc2b7b8dd2064a1c646bc8c08b281db579560ee27f0a7ae7b9")
            }; 

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

    bool IsBlockInCheckpoints(int nHeight)
    {
        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;
        return checkpoints.count(nHeight) == 1;
    }

    bool IsExceptionBlock(uintFakeHash hash){
        return std::find(std::begin(gExceptionBlocks), std::end(gExceptionBlocks), 
        uint256(hash)) != std::end(gExceptionBlocks);
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
