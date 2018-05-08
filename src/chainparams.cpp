// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#include <assert.h>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = CScript() << 0x0 << OP_CHECKSIG;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "02/Jun/2013:  The Universe, we're all one. But really, fuck the Central banks. - Anonymous 420";
    const CScript genesisOutputScript = CScript() << 0x0 << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";

        // Anoncoin Genesis block details:
        //2ca51355580bb293fe369c5f34954069c263e9a9e8d70945ebb4c38f05778558
        //238467b132120c9660156a6752663593e01b2f3e79b2abbc21b71308daa22ec4
        //7ce7004d764515f9b43cb9f07547c8e2e00d94c9348b3da33c8681d350f2c736
        //block.nTime = 1370190760
        //block.nNonce = 347089008
        //block.GetHash = 2c85519db50a40c033ccb3d4cb729414016afa537c66537f7d3d52dcd1d484a3
        //CBlock(hash=2c85519db50a40c033cc, PoW=00000be19c5a519257aa, ver=1, hashPrevBlock=00000000000000000000, hashMerkleRoot=7ce7004d76, nTime=1370190760, nBits=1e0ffff0, nNonce=347089008, vtx=1)
        // CTransaction(hash=7ce7004d76, ver=1, vin.size=1, vout.size=1, nLockTime=0)
        // CTxIn(COutPoint(0000000000, -1), coinbase 04ffff001d01044c5e30322f4a756e2f323031333a202054686520556e6976657273652c20776527726520616c6c206f6e652e20427574207265616c6c792c206675636b207468652043656e7472616c2062616e6b732e202d20416e6f6e796d6f757320343230)
        // CTxOut(error)
        // vMerkleTree: 7ce7004d76

        consensus.nSubsidyHalvingInterval = 840000;
        consensus.BIP16Height = 218579; // 87afb798a3ad9378fcd56123c81fb31cfd9a8df4719b9774d71730c16315a092 - October 1, 2012
        consensus.BIP34Height = 710000;
        consensus.BIP34Hash = uint256S("fa09d204a83a768ed5a7c8d441fa62f2043abf420cff1226c7b4329aeb9d51cf");
        consensus.BIP65Height = 918684; // bab3041e8977e0dc3eeff63fe707b92bde1dd449d8efafb248c27c8264cc311a
        consensus.BIP66Height = 811879; // 7aceee012833fa8952f8835d8b1b3ae233cd6ab08fdb27a771d2bd7bdc491894
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); 
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 6048; // 75% of 8064
        consensus.nMinerConfirmationWindow = 8064; // nPowTargetTimespan / nPowTargetSpacing * 4
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1526321714; // Monday, 14 May 2018 18:15:14
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1526321714; // Monday, 14 May 2018 18:15:14

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1526321714; // Monday, 14 May 2018 18:15:14
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1526321714; // Monday, 14 May 2018 18:15:14

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1526321714; // Monday, 14 May 2018 18:15:14
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1526321714; // Monday, 14 May 2018 18:15:14

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000002ebcfe2dd9eff82666");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x000000000085d0ad7b56d614e503d6d6ead9d114d2ad541fabc8d416c651b455"); //500000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xca;
        pchMessageStart[2] = 0xba;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 9377;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1370190760, 347089008, 0x1e0ffff0, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000be19c5a519257aa921349037d55548af7cabf112741eb905a26bb73e468"));
        assert(genesis.hashMerkleRoot == uint256S("0x7ce7004d764515f9b43cb9f07547c8e2e00d94c9348b3da33c8681d350f2c736"));

        // Note that of those with the service bits flag, most only support a subset of possible options
        /*vSeeds.emplace_back("seed-a.litecoin.loshan.co.uk");
        vSeeds.emplace_back("dnsseed.thrasher.io");
        vSeeds.emplace_back("dnsseed.litecointools.com");
        vSeeds.emplace_back("dnsseed.litecoinpool.org");
        vSeeds.emplace_back("dnsseed.koin-project.com");*/

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,23);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,50);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,176);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x77, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x77, 0xAD, 0xE4};

        bech32_hrp = "anc";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                {  1000, uint256S("0x00000b0e2c2b0ce41331b24dbeab9a2b13323c80008a3e6a28330c4fa04562e2")},
                {  4200, uint256S("0x0000015d22f8a48b8685657b92d7cf2886c926f4c59f800bfd9ef314ef61d577")},
                { 42000, uint256("0x00000007c544cbf45e3975c0168a8233384bc92a075f1c6cc8126de592a898f8")},
                {420000, uint256("0x00000000002f8e58b723820c2d5ce7f6c4699d39d216fea12b2962985cab1ab8")}
            }
        };

#ifdef ENABLE_I2PSAM
        /**
         *  See net.cpp->ThreadDNSAddressSeed() for how these are used.
         */

        i2pvSeeds.push_back(CDNSSeedData("5oo3enrz7fp77ojrfk7hjsniohsxqmhuxdhdx6ur7iwumsrjzkwq.b32.i2p", "5oo3enrz7fp77ojrfk7hjsniohsxqmhuxdhdx6ur7iwumsrjzkwq.b32.i2p")); // Cryptoslave's seednode
        i2pvSeeds.push_back(CDNSSeedData("xowpui5nxkarsg2uwjllc6wdteheytknicbbqsnbkjjwde5iq6ma.b32.i2p", "xowpui5nxkarsg2uwjllc6wdteheytknicbbqsnbkjjwde5iq6ma.b32.i2p")); // Cryptoslave's seednode        
        i2pvSeeds.push_back(CDNSSeedData("if3pj2dv3cv3ljmjy3gism45r54lvjck5moavdjiroukrxlfjfia.b32.i2p", "if3pj2dv3cv3ljmjy3gism45r54lvjck5moavdjiroukrxlfjfia.b32.i2p")); // Cryptoslave's seednode
        i2pvSeeds.push_back(CDNSSeedData("xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p", "xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p")); // Cryptoslave's seednode
        i2pvSeeds.push_back(CDNSSeedData("a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p", "a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p")); // K1773R's seednode
        i2pvSeeds.push_back(CDNSSeedData("b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p", "b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p")); // K1773R's seednode (dnsseed01)
        i2pvSeeds.push_back(CDNSSeedData("7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p", "7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p")); // lunokhod's seednode

        // As of 12/26/2014, there are NO entries for the above clearnet pnSeed array,
        // only using I2P for fixed seeding now, the strings are base64 encoded I2P
        // Destination addresses, and we set the port to 0.
        for (unsigned int i = 0; i < ARRAYLEN( I2pDestinationSeeds ); i++ ) {
            const int64_t nOneWeek = 7*24*60*60;
            // Fixed seed nodes get our standard services bits set, this is after creating a CService obj with port 0,
            // and a CNetAddr obj, where setspecial is called given the I2P Destination string so it is setup correctly
            // with our new GarlicCat value for routing...
            //
            // ToDo: Change i2p fixed seed node details to also include the services they support.
            // The seed nodes should be added with the correct value, if that information does not
            // matching in addrman, it means creating double entries, when the node is connected,
            // and the correct values found out after the peer version details have been learned.
            CAddress addr( CService( CNetAddr( I2pDestinationSeeds[i] ), 0 ), NODE_NETWORK | NODE_I2P );
            addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            vFixedI2PSeeds.push_back(addr);
        }
#endif // ENABLE_I2PSAM

        chainTxData = ChainTxData{
            // Data as of block 59c9b9d3fec105bdc716d84caa7579503d5b05b73618d0bf2d5fa639f780a011 (height 1353397).
            1516406833, // * UNIX timestamp of last known number of transactions
            19831879,  // * total number of transactions between genesis and that timestamp
                    //   (the tx=... number in the SetBestChain debug.log lines)
            0.06     // * estimated number of transactions per second after that timestamp
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.BIP16Height = 0; // always enforce P2SH BIP16 on regtest
        consensus.BIP34Height = 76;
        consensus.BIP34Hash = uint256S("8075c771ed8b495ffd943980a95f702ab34fce3c8c54e379548bda33cc8c0573");
        consensus.BIP65Height = 76; // 8075c771ed8b495ffd943980a95f702ab34fce3c8c54e379548bda33cc8c0573
        consensus.BIP66Height = 76; // 8075c771ed8b495ffd943980a95f702ab34fce3c8c54e379548bda33cc8c0573
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // 3.5 days
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1483228800; // January 1, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517356801; // January 31st, 2018

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1483228800; // January 1, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1517356801; // January 31st, 2018

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000007d006a402163e");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xa0afbded94d4be233e191525dc2d467af5c7eab3143c852c3cd549831022aad6"); //343833

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xc4;
        pchMessageStart[2] = 0xa7;
        pchMessageStart[3] = 0x4b;
        nDefaultPort = 24735;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1486949366, 293345, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x4966625a4b2851d9fdee139e56211a0d88575f59ed816ff5e6a63deb4e3e29a0"));
        assert(genesis.hashMerkleRoot == uint256S("0x97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        /*vSeeds.emplace_back("testnet-seed.litecointools.com");
        vSeeds.emplace_back("seed-b.litecoin.loshan.co.uk");
        vSeeds.emplace_back("dnsseed-testnet.thrasher.io");*/

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,48);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x34, 0x47, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x34, 0x47, 0x83, 0x94};

        bech32_hrp = "tltc";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            {
                {2056, uint256S("17748a31ba97afdc9a4f86837a39d287e3e7c7290a08a1d816c5969c78a83289")},
            }
        };

        chainTxData = ChainTxData{
            // Data as of block a0afbded94d4be233e191525dc2d467af5c7eab3143c852c3cd549831022aad6 (height 343833)
            1516406749,
            794057,
            0.01
        };

    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP16Height = 0; // always enforce P2SH BIP16 on regtest
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 3.5 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 19444;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1296688602, 0, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x530827f38f93b43ed12af0b3ad25a288dc02ed74d6d7857862df51fc56c416f9"));
        assert(genesis.hashMerkleRoot == uint256S("0x97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true; 

        checkpointData = {
            {
                {0, uint256S("530827f38f93b43ed12af0b3ad25a288dc02ed74d6d7857862df51fc56c416f9")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "rltc";
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
