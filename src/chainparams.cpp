// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <random.h>
#include <utilstrencodings.h>
#include <netaddress.h>

#include <assert.h>

#include <chainparamsseeds.h>

using namespace std;

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

#ifdef ENABLE_I2PSAM
// As of March 30, 2015
static const string I2pDestinationSeeds[] = {
 
"d-Puxd~OdmFGywsIE4cIpbCfZLId8xWJEeelq18VgnZykEYiPS0K0C9w7eb2UMfeqamYejmODi9ITePyZpyOT3vRgKR~I88AiQbiH1YjiR17-R7KDRJQXmbKfOqMpEMW7JUh-hZBI87PNkC4TTv65lSpKyt8Kz~SaX21jVEMX34klJg7ZI48JIaSvRXWFPCZoyadKOryjPIS-tqVQyX4yEbVcypLfouJ02uRYvo2SAyluDBpZ71Rn5FpnlDKkLx0srh5k6CJYMz7vozNYUub9xvKEZK0SLcKsLzy~FRyuHEbOHUZ76~ibvxUUWORt4hHdT3O-WDre17pMDnuEG8YDGHPotX-Y7CcfH-fXRacVLwhK4Y5k9BvdmiuaZZoU3GxOIGxyMGeK5MJhinNBNdZpyEvUnFPpfu7R33offpE50vKmnwim9YZVa76VG3xlRpYWu9Gj2IAfzR-3JSTvtxFPqMDHR2-D-MOeOp98Qe0CtHcAvx3kHhuCJW4gbwOJa2TAAAA", //a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p, K12
"wV3VH8LCTMrdkf3-qYuEw6BkhC6ChQyczQRxRDRrTFPEUiAipqNGsofXHAmTtF1MzirB2aBeuuHuOszcy3209NNmxr2RZD5Gz~XTyT55Juw2Qoc2VitdeaAwlaLzYIM1z1Amw9yIV43G~~B318N3iaxlNoEPN4YLzpipIl6h6zcDi6pJdLsMO7hKxagKzPLAx6scrui4GsNO6sQ-6eUBpiO3XM~gbE3yag82ShhIsxabTrrSCowqM~nFVNdZ58hBrwSOGpCKZ3-rfrJ3I0-ZFR52CyfZY-O8jasftGMQFF0QpKT60uLwTZwsExBuqYKWumXojdq07B4VO~1w~CprF0OEJpX8Uul2Q-1r7P1ANmWJI8fg~~B1~c7xtfoWW7omd6lNvQRgmuH~w2yQZ59wIuuDMZTmJB4S9dnGbCr0B4G0kAgpuaV9hfVkZzOddsM9e5o0mYJZE1BPCEMTbm817xaRi5DVQMd2bTTO0YRdgXe6wylXo9KyeL-fMbmnD7AbAAAA", // xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p, CS1
"Oea~tAaOV4IK2UlpMihCDfmbKtDGL6kItPE2kqESQDYuip6Jp4cTNwwsGS7Bw433sBGGHA0YlViaei0aXsXzpnq2-o1Uh5QD1mAVjHIe6iVAOo7RWqVvASQfjUuD3eqVgdLqxN3ifdbJw3~-aYFN2YlThDTYumi-Ut6aLU44AMTCqBb-su9bbJAZLh3wrn2-fOZ88Ayqz5BB5cRYWUPVxKpdlbFA5MWGjOUIWeo2lqfGLLr9vfvWb5yXs1ewPvwKdgA7JzvTPhXTEl3VeocD0FHbB76b43roamIWhgVme-MMPmSacELcGrOa3HLT55grdfbimao1yJgerbjA8K4xZILhN4cYghZp6VADfTwoeb~rlttvZQIu~rZZ6Y6B2PmSej8V-esdpJEbVHHjpb1924BCKAZOkW8x4Xd9J9h7JpShgCjcjX2nXdIhMCImqui3wOgfNjl1Zd5oQAPgXh4KePAi3QuXiJD0UOzUjnEzJjjEKpWscKxrZfvLcj2L~VZeAAAA", // if3pj2dv3cv3ljmjy3gism45r54lvjck5moavdjiroukrxlfjfia.b32.i2p, CS2
"apuLsXH2KdmTVUgY-PIJcRyVEOExQCmX4-1olVrOg1g5adVW~DQX9wfwXEMVZTPQn9FqyaU2vrvgXsJuQEECRWGewf4DIylJG9dn-ac6N9LniTbWmbSNyWDOv54qc4yO3LeHyp3Gm2UvaSpdmjXQ0PnLirWXo-HxmvTpD~UunIraX4SRZcijNzBG6jYAdjp8-sTq17kjb9S3Ar33UmJR0G9ir4UrY93zKvUojiylLpNrJKeBkp4YB2RurXkwy6zHt2mavhae7~sKa0YfXcn-ZnUIVbIp~KC~dxhEO~L6VBsbtfki-4M1xRn39~ygI0Y-Ca2nSDgRsEZ9bi8uUbBQgYzSZfsDzAgUNWcQHYZHHX39cP-S8Du~yU4Ioy3cC~pa31Inv3RcfR9ZX1qxrBsPiDEdgtfvbO1ahNxgeTVnhYg-6n--jxqLDEI1rOpzFJD0yHfNKcjeJ5nKq5cwFBRjeAlBKGNmHioILIOcz48Woq1OQdnfthA6zDGEfnwmN~eYAAAA", // 5oo3enrz7fp77ojrfk7hjsniohsxqmhuxdhdx6ur7iwumsrjzkwq.b32.i2p, CS3
"S-cet3-RrmyWZAHunl5sK43PI8RK4YphgR7agwi2z2Cyj5hQ8k2ZSE9GW3zNtazZiSeee1alFjW7LaxjfZ936aK3T5bLwRMrgjUNeuC96I8Csl-ze63gRcVWsLsQLo66kXoZS9dEAbi4tPJdMggYvvJQDi~wLPC1ZVqai0CoVs67DmGJlBKHMabdeGGhEMKHwEMxEdEvzTnwjoWDr5zjKoyW9DCeJvIE5QrMywjsnBPo8bY5YuQCymVdx5Ib1sFuWsgnUOsh2wiGjonKZKNR-Wb7PpzXlQNJM5DYT1d6np1t0vf9TI3EHSv7D9XkmdWZDjEQkWEk5NNh-l05zvwUc2~7r-YOchKkchRPBEhvOvayxNmwAfIEg1TtPNLNoKpQk4HrGpbkG6rF6ZAM1eME8LVjDCJqXeVoyv3YoAz7WAUuvuKI7fmnXxUiePQKnAxxMAMEeeRkCw0-4CY6BUKvt~BULkc5DFLoEechkzfL0W99KiruQchWlOk0ebci5sBcAAAA" // xowpui5nxkarsg2uwjllc6wdteheytknicbbqsnbkjjwde5iq6ma.b32.i2p, CS4

};
#endif // ENABLE_I2PSAM


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
        consensus.BIP16Height = 850000;
        consensus.BIP34Height = 850000;
        consensus.BIP34Hash = uint256S("fa09d204a83a768ed5a7c8d441fa62f2043abf420cff1226c7b4329aeb9d51cf");
        consensus.BIP65Height = 850000; // bab3041e8977e0dc3eeff63fe707b92bde1dd449d8efafb248c27c8264cc311a
        consensus.BIP66Height = 850000; // 7aceee012833fa8952f8835d8b1b3ae233cd6ab08fdb27a771d2bd7bdc491894
        consensus.AIP08Height = 585555;
        consensus.AIP09Height = 850000; // TODO: Find a block in the future
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
        consensus.nMinimumChainWork = uint256S("0x00");

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
        LogPrintf("Genesis: %s", consensus.hashGenesisBlock.ToString());
        assert(consensus.hashGenesisBlock == uint256S("0x2c85519db50a40c033ccb3d4cb729414016afa537c66537f7d3d52dcd1d484a3"));
        assert(genesis.hashMerkleRoot == uint256S("0x7ce7004d764515f9b43cb9f07547c8e2e00d94c9348b3da33c8681d350f2c736"));

        // Note that of those with the service bits flag, most only support a subset of possible options
        // TODO: MEEH: Check if this works?
        vSeeds.emplace_back("seed.frank2.net");                       // Normal DNSSeed
        vSeeds.emplace_back("dnsseed03.anoncoin.net");                // Normal DNSSeed
        vSeeds.emplace_back("anc.dnsseed01.anoncoin.darkgamex.ch");   // K1773R's DNSSeed
        

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,23); // Anoncoins starts with A.
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
                {    674, uint256S("0x98045dcaca7c0e00ec9a2c18ad4f25e1222741dea993203dab80abcf0f28c28f")},
                {    703, uint256S("0x2e42a5f6d485a87c1f065adb432ac30776be91998333576232e2bc887b9be6d2")},
                {    704, uint256S("0xc9e11d1f6bde829edc92e4edc7975eadc6df4918a770533a46fee6f49ea5c86d")},
                {    705, uint256S("0x58ef03279842afa46cb47ffd2028a108126c531944e9867a7f7c757c7d6369a3")},
                {    706, uint256S("0x4f48ed9749b7ec40afe7d74037eb10862b71289694db49ad762082bf7775daa8")},
                {    739, uint256S("0x2a038a0b2c1d80caa1979c1e880daf7af39cef2aeadbf447dc6738fb411e3a05")},
                {    740, uint256S("0x85d526aeb89caffb1b9464076cb03f936cc71072a74d169f1cffc1810e72b9b8")},
                {    741, uint256S("0xbbb61c5a3a8ec054634e61e470c05e08de4774315db93ce1fa265ab4945a6cff")},
                {    742, uint256S("0xf83b1beeb9d03e2bf784504ebfc039516602d2d1593f11bb5dfc232010ebbce4")},
                {    743, uint256S("0x15cb15f7ca28d3058a6615249e8087072e9cdcb494a9bbcd81042e60fd4ac77b")},
                {    744, uint256S("0x6634539203c68085c75f6901a6406c4e0ecc8d4c0b9a1617d0baf0fb16ce4ffc")},
                {   1000, uint256S("0x00000b0e2c2b0ce41331b24dbeab9a2b13323c80008a3e6a28330c4fa04562e2")},
                {   4200, uint256S("0x0000015d22f8a48b8685657b92d7cf2886c926f4c59f800bfd9ef314ef61d577")},
                {  42000, uint256S("0x00000007c544cbf45e3975c0168a8233384bc92a075f1c6cc8126de592a898f8")},
                {  63003, uint256S("0x7b71e6664532d50e43fee1e4e10834137e2249a3ecb22a0d52a27145a5a4efa6")},
                { 420000, uint256S("0x00000000002f8e58b723820c2d5ce7f6c4699d39d216fea12b2962985cab1ab8")}
            }
        };

#ifdef ENABLE_I2PSAM
        /**
         *  See net.cpp->ThreadDNSAddressSeed() for how these are used.
         */

        i2pvSeeds.emplace_back("5oo3enrz7fp77ojrfk7hjsniohsxqmhuxdhdx6ur7iwumsrjzkwq.b32.i2p"); // Cryptoslave's seednode
        i2pvSeeds.emplace_back("xowpui5nxkarsg2uwjllc6wdteheytknicbbqsnbkjjwde5iq6ma.b32.i2p"); // Cryptoslave's seednode        
        i2pvSeeds.emplace_back("if3pj2dv3cv3ljmjy3gism45r54lvjck5moavdjiroukrxlfjfia.b32.i2p"); // Cryptoslave's seednode
        i2pvSeeds.emplace_back("xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p"); // Cryptoslave's seednode
        i2pvSeeds.emplace_back("a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p"); // K1773R's seednode
        i2pvSeeds.emplace_back("b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p"); // K1773R's seednode (dnsseed01)
        i2pvSeeds.emplace_back("7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p"); // lunokhod's seednode

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
            //CAddress addr( CService( CNetAddr( I2pDestinationSeeds[i] ), 0 ), GetDesirableServiceFlags(NODE_NETWORK | NODE_I2P) );
            //addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            //vFixedI2PSeeds.emplace_back(addr);
        }
#endif

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
        consensus.nSubsidyHalvingInterval = 420000;
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
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xa0afbded94d4be233e191525dc2d467af5c7eab3143c852c3cd549831022aad6"); //343833

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xc4;
        pchMessageStart[2] = 0xa7;
        pchMessageStart[3] = 0x47;
        nDefaultPort = 24735;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1486949366, 293345, 0x1e0ffff0, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256S("0x4966625a4b2851d9fdee139e56211a0d88575f59ed816ff5e6a63deb4e3e29a0"));
        //assert(genesis.hashMerkleRoot == uint256S("0x97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        /*vSeeds.emplace_back("testnet-seed.anoncointools.com");
        vSeeds.emplace_back("seed-b.anoncoin.loshan.co.uk");
        vSeeds.emplace_back("dnsseed-testnet.thrasher.io");*/

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,48);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x34, 0x47, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x34, 0x47, 0x83, 0x94};

        bech32_hrp = "tanc";

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

        bech32_hrp = "ranc";
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
