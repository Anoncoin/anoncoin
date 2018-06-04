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
        (557124, uint256("0x18aecbb4d9fb97e609788c62e606da6a141ff519872319a2c343d011be265ce4"))
        (557125, uint256("0x6d43ebc872fb5ad2ac5209ee65b81bde283080ce39114e606ce284ef8c226df6"))
        (557126, uint256("0xfb1d32f7875de59148d7d33bf60c8a29faca3f79de77bf7867b1209d9d9fd88a"))
        (557127, uint256("0xe737a66b0cfc5d10f84199a037fa674af646fa52dcecbf6b0fdace147d1b67e3"))
        (557128, uint256("0xdd56b86006a675809e313538e5a43c0991f2af2aa9eb5b272abbce8d2b640f1d"))
        (557129, uint256("0x765a92a0cee64a4555bf93acb297b0975d9bdd13dbd0f0f6e40374105246ba73"))
        (557130, uint256("0x771b4cd647fcedff1fb4a960278d846620ec6c2864b0432c3ab89997df71a3d7"))
        (557131, uint256("0x21545e9a0ffc44020d99e7763cca6e8d6462332c52d7ccd6080aea0cf1a87984"))
        (557132, uint256("0xa825c5c302aeaaab7da66e9021050c50bc2c5a620f353e0d5f05b6c350a0ee39"))
        (557133, uint256("0x05840b7ddddec9726abb5428aba5e11cbc3e40685326619b07097236ac6e2237"))
        (557134, uint256("0x7285d2d8743b1988b78532f472a7f82bf2cbb9ceb792d0b09097cff130b57606"))
        (557135, uint256("0xbf69faca2169bb1f0a178346815df73bfcc8e194250854f4232c65b9a28e64f2"))
        (557136, uint256("0x96da7f3ac44e91293429a760add9adf4a2f1ff2e8488f6e53ac3497937f3bca4"))
        (557137, uint256("0xd1c2c3842f9119a7cf048c8988a74e41f5c0209ba38c653c744d38a477a15769"))
        (557138, uint256("0x531a8e3df1fbb76a35a70cad0295dafcdccb5a49cc9d27fd5b8f83ffd279545d"))
        (557139, uint256("0xaba8fdda12f7dad02c300174f1842c041119950c0f1422b6cc9063f96ffa15fc"))
        (557140, uint256("0x21ec1dcfb19e6073f0b8cd518da4c9d73eea27016e4a10ded9561c80f2a9b38b"))
        (557141, uint256("0x4be6c25bb4ef8a154f7ec1e1587ab36a948330b8be5552de37add969817cc90f"))
        (557142, uint256("0x8f389ee68683920659c18f2b482aff20e4fb69947fd43a1d2c98b133fe41534b"))
        (557143, uint256("0xfaa7d906db811f1bc33170d85857cb26b2eafe7505c6b3137d592791750ac684"))
        (557144, uint256("0x3033a118ce7997e603024b0f50c740fa47838e3725f8599c26147a0c6bd87977"))
        (557145, uint256("0xfeaed17f4bcfd044a4c66c30dcbda62338af1ec96c55e4919ac75b9e93422f59"))
        (557146, uint256("0x03ec94983b934eee79ab518dc6693680754f9c012261b5b257f20248999b035e"))
        (557147, uint256("0xe67e6c77c15203f55528f1cfb4b78524f06c0a50410f7e393c3c2018c47e0d89"))
        (557148, uint256("0x0b34ecb9db70a68bfc6c44cead8dd68e2b57a4713cafa337712c23bcce3558d5"))
        (557149, uint256("0x504d4586392bbe50b6c2225ed126cdde713e7c36cd7dfc93af8cc15c52f8e544"))
        (557150, uint256("0xb3886ce206efba804b5917a76f0163e5d3110434b49759c9f79732bdef1e149f"))
        (557151, uint256("0x430fec696a9203091fec46a432b3fde78f7246757436351d0d61f82348ab540a"))
        (557152, uint256("0xd63c6f9298a8790db1ebc4b726547538db7c47d3fbceece940a7c67c6a1e0681"))
        (557153, uint256("0xdde4c95151014df2a0be883d40ffdcdb4a71f0f7438f5a2b3ac90d4e7d6e6879"))
        (557154, uint256("0x54c21cc1e829d93635cf8f5f85952d38825d9ea474c7686299b70ced18098984"))
        (557155, uint256("0x89bed3f8f68303aa1f8dd62b6907d2d6085a7ca305a523b32f24cd409aaa55cf"))
        (557156, uint256("0x1d33ef3c1c6c2dcb8fbd4e69d5b92003aeb898a6586301ba2ed245a649793934"))
        (557157, uint256("0x43743f41846914326336f8803dd3baf41073c2621c5cfb858c5071baf6ac2838"))
        (557158, uint256("0x7d05508b0b443d9b78907194c7bc52bcd0c439c7fc3b24b2bc283c68c8f59265"))
        (557159, uint256("0x693377bff39fbb08363fd0242fcb83b0599f253ae823f5c8fc17cf571b547919"))
        (557160, uint256("0x38a05c43debd2beccfed23a048f76c2462ec03c1ee0fb8ef57762f8f40170f5d"))
        (557161, uint256("0x984df26e2a57bf8fd564c24d99065cfc2cb5045db524a8260b2ee9e1f205bbe3"))
        (557162, uint256("0x8b50794ee18c5664de82cd1319a96570bb1a6fa4ffd5917f38f7be4f34367d90"))
        (557163, uint256("0x930ff3c8d496f31377479e0f7fadb2f1a45c132e1299096a0285c8ab4a0ff896"))
        (557164, uint256("0x798f1c3ad10aa15258c50a664cf16ff3fe7c1a18686d3316b7b55721e4a2c0e2"))
        (557165, uint256("0xdc2847da7f20c28499df4aad7b05c4585d6e05c2cc51754112b95c6ec8c60edf"))
        (557166, uint256("0xf468eaaac212fc266914cfe3cf57dd8b8f5d720790600e7a4eedd4799bfe3f75"))
        (557167, uint256("0x185b90004da23e783bae07f47ca5f7fe1ee87266338965b8ca752b22111c0f3b"))
        (557168, uint256("0x3005de6e31f8aa8283c4bd479966b0c9432d7f43088671a319d08804a23ae3b0"))
        (557169, uint256("0x3bcd8e3fddc80d3059e7411d275f98de09865f4eeaa8fb942d15e64abce1cd17"))
        (557170, uint256("0x7be5e79ac2169184290c5c20ee6f39de904db0cd645a621cb80827268977e74a"))
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

    bool IsBlockInCheckpoints(int nHeight)
    {
        const MapCheckpoints& checkpoints = *Checkpoints().mapCheckpoints;
        return checkpoints.count(nHeight) == 1;
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
