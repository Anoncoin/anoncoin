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


//CSlave 14/03/2016 Those will be deleted:
//"HNaqLnETmOcfEzbmzyh5XTQJCqxHVBk0mdvAbc2CZbIi5OxelNMREYLo1U2wGIOgNFSgfdvefOroouRpC8vsoXJ2ARYFTGH~wKd4KVv6sEdD3bVhtVYW03nuwz6KP9~otTO6QvX2uRSQNJlNGqGN7L4uNgzHuznU0jpCmpARDC4y95144iI7hWawxFnqthnzEgPNSGp5lqM7GQzDplWTDDAoF1GcJqPzVY0uoMWvLCfCNUpHw2wO0egKuAlv6Uz9IK0dQl7S0XmE9Jnt7I2ZKs4-RlX9lRf-hEHgMY9Y6k-Brl-TVwTkzmsvNPYTpbOxvkXIBru5Yi~IaGsImjhmvGT4FAj02b-0HNqGWfaiKOHl1k~O8geUeIDO2fA5oyJQmcwstMjmU24DMFeCBCB0MFL0xK8bJkUZkrzhwIY-Z2U9GqxEjt53m80-PHvUsDOGAKOpeT0fnx3vLVEJFeHS6K~G2XG24EzcmSCLwcPEFptQpxWrR4nSxya6KTPp6ZUCAAAA",
//"3bfyLFHqvqCZuAtVLIR8R4buAW3e1VQ6-S6jKrXNX5Hgq-b1fYbLqniP5MjErFzhTlu-Umk9n-E9jnLsTXJQNkZB5RkNnSMSkxPsQpFWJ8pbN-qXQf1KtJ0UYEA0zi7Eq33caToH5CtGkMvik5zOFtPKQEIbgzzu6N7tvb2huGvpBTJQdTdQSI1hjfQbpnTkZ-bsJrCWs54nu13OAi~AuSvKJtGl7sLrA1z~7YyJfA84pOLO5oMxzMJnjhARMJFTyG6RxHOeTHOlNqoLVCY7sLI2IZpJrQAJLRHTTFbUKptuj4v3W8lcZDFXa4PmK-xA6NuicFC~b6YaOMEiAL1E7w72pHTwcIxmKP1FF3U57bwDGU7126n3P8PECouG0o6QXlTsYHt~Qc4pZGy4CQtINMFcx2ik0KKxOACTu8XzJ2~7FHINgMzPnd87vNK6mfBDYZ3Np~XwEZLF1GkbYYqLVKvh8NdWiqNBt5zzR0wq3XKTxCljpIs9JIP2FNLaAh0KAAAA",
//    "y2arPA7vC36rA-SrPb2spKxvhNeCvx7ZMhdO-F9csEvWYxvx1a5NK0K2Qfo0cfwh3KrNphYudAGqYfwhi~a4xUnFtZ1-ruCtQ33i298HnnDeRJv0T1~Y9dlvlN9xQnBggH2EjVv8~I1p2W2GyGliPZ0NUzjgdmX0N1jNCP5tJ1lMUqsJrdvGi~bEmbMDOA7y6-pnyrfRC6lk7qlIuT~kJh8l-HerJ0gEzu0ByZ~YDTK4EpWaCQgW68gKIgK3pKnscfILkiZwiRHkG5kWugBeJixIO3MPOAW17HdVG43EzLAlnC1BW8vkUXKI37Ry4cfylCz-90HPfRmwom-E3ZHiEoHQFAc43l9f2~KZzmtj-7u21~e47daGnoMGjta8ADwekmd-MieVFQeJ27fsRiCQkLDTq63ONbcTqIulXGhffXZtkyoq0qo-eVl3ErS6YyVpnhvvkuP46Gh81Db4zWbu~NEQq~qb1X0JhaaiufVTY2pNgHbNVinJjyjzq2ip3dGMAAAA",
   // "UmCjD3EY6cEeCv5N9b9cRWBkU8~EwacG7nY6Tx9vPCne5fP-LIMxR~42hyrPGN4-ZXpGbns5Pu4JEvckj~mkXnJxM2-b6An2QsVYJCCqBmnqjK43HZC32s0Z2qFW5Y3agNs3eAtSK~GXy~IvQYewhSlmIOs8W~eAkLzF~C2~VzSmaWs55uBzLUINeJr7E7woPFnnrP8EjfMJHNUbmTkw1bepf3K7~BWy-LBlBaWvylTtRiAPDlqI2Q9ZZUn3wYQbHLZSGHkjdouRJ3zndcFTjwTEHzh4XQcHQK9hud7USUGnasl1IXsxizJo5lzsP5SvytkzdI07FjhpxSAYDKHn6SW4UXM5rQmAq44zHu4AzY76p3Wk9iUtwz1rFQNLqTwouZYe5hviiB1GAaWZBOhz9KsG0DzxeFRV5ou8Ps8-xlhU36mjrw3L6CYEpT6WejbNnXlIC~aUI057MAjr2sLfvVBhmycwJdSMOIT5fVkm8aTmiIM-YgpYUpInNHxH5fKEAAAA",
   // "rR~bh73tiGezgHUs82cKHX83z~Jmp7pfyTWbwjrQgzM3at-DUh7TVQnsB5FnfJrbH89P1zXTdTqUR~OjQnjlt1WZZISrBVoUNe0O8xruhUsGR77WmRbSrb1K1o6LG0eX9dZk7ZTrLa-Bj0xG3jk8IYOLOdkCjld7VzBdkTcGN7G8QWx33cJn2778i9gSGNza0egyiEd4QnCjgZlI0wYTyLz-35F~to9unN6uq8AMK9uJa8a67l~HyDl1dGC5Ye~KKRBWgD5ZCoJAJBOipjsdAZqHUTa64xX4cX6EPEVBdikAZrRQS2l-FyLcuHvXFQ~rRuOEfO0VGmhJOIzNMdvK4UfsT118kt~pPe1nI4GEQ1xWP5thcFv-WN4mtDzU4fqFivhb7hijygUhQ7zrZFCbDypzYZTPBSXjABknbcz8j0iPy9GgEHsF8Zltt~i77Vc2e8cTcNMSpxm1uvMFJiB373Lri2vGj5oB0apixV-HUj1Bdpb1HTm96PDpf963jhj~AAAA",
 //    "kvGu5BRTohvsGsNqkQW5glv-Q1Zxzwm40mFWK9g8Jte7561SmQxkKrklKX1bfuoCQ9hn1Ty3406-7xQb7bhoD9YC1mPm9hoBDNBzb4f8jn~Zry4SUY~AfCSvD1EBW4cvme9VZ1mIu3Jtj9qyv7j0gKgGyXAkX55gXT5EhRUoWspsWxao18PsB-HUTIyMP-At8GOcAeoI4GXbFSCa170o6pggoJrsXfg28Iww1VJB7MQl5IE2Q~i-WGuynKjmVUFshH9tC-vIW0nvaY-vnxNL7GNVBHVmcyi0I3nYxym~74BfdEioV-c5Q20aztYzxgvTelJu-56AMTP9GJKnn9BjVhKs2jppZLd6WFs30wGZMoK3a7Q8~kRcHKXX82pGisnBUxAbFQyOhAUOMw2wvKUeHh6-WBE8WU6zCALyLJ2532tvf8Bb~k0S7ZD96vgaMGGiR2daHcFf1e~zmVILPf27ya8bg874HZoq7WcrUsF5tLha2cMHFx1RXwMAAPlL3mYVAAAA", //ntmn7mwa6tpm7beaz35cljhggdl75537ixt4nno2i47kllcrkhpa.b32.i2p, down
//    "8w-NRnXaxoGZdyupTXX78~CmF8csc~RhJr8XR2vMbNpazKhGjWNfRjhCmwqXkkW9vwkNjovW2AAbof7PfVnMCff0sHSxMTiBNsH8cuHJS2ckBSJI3h4G4ffJLc5gflangrG1raHKrMXCw8Cn56pisx4RKokEKUYdeEPiMdyJUO5yjZW2oyk4NpaUaQCqFmcglIvNOCYzVe~LK124wjJQAJc5iME1Sg9sOHaGMPL5N2qkAm5osOg2S7cZNRdIkoNOq-ztxghrv5bDL0ybeC0sfIQzvxDiKugCrSHEHCvwkA~xOu9nhNlUvoPDyCRRi~ImeomdNoqke28di~h2JF7wBGE~3ACxxOMaa0I~c9LV3O7pRU2Xj9HDn76eMGL7YCcCU4dRByu97oqfB3E~qqmmFp8W1tgvnEAMtXFTFZPYc33ZaCaIJQD7UXcQRSRV7vjw39jhx49XFsmYV3K6~D8bN8U4sRJnKQpzOJGpOSEJWh88bII0XuA55bJsfrR4VEQ5AAAA",
//    "hL51K8bxVvqX2Z4epXReUYlWTNUAK-1XdlbBd7e796s2A3icCQRhJvGlaa6PX~tgPvos-2IJp9hFQrVa0lyKyYmpQN9X7GOErtsL-JbMQpglQsEd94jDRAsiBuvgyPZij~NNdBxKRMDvNm9s7eovzhEFTAimTSB-sgeZ4Afxx2IrNXDFM6KS8AUm8YsaMldzX9zKQDeuV0slp4ZfIAVQhZZy9zTZSmUNPnXQR7XPh5w7FkXzmKMTKSyG~layJ3AorQWqzZXmykzf4z3CE4zkzQcwc0~ZIcAg9tYvM2AdQIdgeN6ISMim6L8q6ku6abuONkyw-NJTi3NopeGHZva21Tc3uHetsKoW434N24HBVUtIjJVjGsbZ7xBfz2xM5kyhPl6SlD-RJarCw47Rovmfc9Piq6q3S~Zw-rvRl-xDMJzwraIYNjAROouDwjI9Bqnguq9DH5uFBxf4uN69X7T~yWTAjdvelZKp6BGe~HGo7bNQjBmymhlH4erCKZEXOaxDAAAA"
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
                {  1000, uint256S("0x00000b0e2c2b0ce41331b24dbeab9a2b13323c80008a3e6a28330c4fa04562e2")},
                {  4200, uint256S("0x0000015d22f8a48b8685657b92d7cf2886c926f4c59f800bfd9ef314ef61d577")},
                { 42000, uint256S("0x00000007c544cbf45e3975c0168a8233384bc92a075f1c6cc8126de592a898f8")},
                {420000, uint256S("0x00000000002f8e58b723820c2d5ce7f6c4699d39d216fea12b2962985cab1ab8")}
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
        assert(consensus.hashGenesisBlock == uint256S("0x4966625a4b2851d9fdee139e56211a0d88575f59ed816ff5e6a63deb4e3e29a0"));
        assert(genesis.hashMerkleRoot == uint256S("0x97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9"));

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
