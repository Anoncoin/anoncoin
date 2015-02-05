// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2015 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
// Anoncoin-config.h has been loaded...

#include "assert.h"
#include "core.h"
#include "protocol.h"
#include "util.h"

#include <boost/assign/list_of.hpp>

using namespace boost::assign;

#ifdef ENABLE_I2PSAM
static const string I2pSeedAddresses[] = {
    "8w-NRnXaxoGZdyupTXX78~CmF8csc~RhJr8XR2vMbNpazKhGjWNfRjhCmwqXkkW9vwkNjovW2AAbof7PfVnMCff0sHSxMTiBNsH8cuHJS2ckBSJI3h4G4ffJLc5gflangrG1raHKrMXCw8Cn56pisx4RKokEKUYdeEPiMdyJUO5yjZW2oyk4NpaUaQCqFmcglIvNOCYzVe~LK124wjJQAJc5iME1Sg9sOHaGMPL5N2qkAm5osOg2S7cZNRdIkoNOq-ztxghrv5bDL0ybeC0sfIQzvxDiKugCrSHEHCvwkA~xOu9nhNlUvoPDyCRRi~ImeomdNoqke28di~h2JF7wBGE~3ACxxOMaa0I~c9LV3O7pRU2Xj9HDn76eMGL7YCcCU4dRByu97oqfB3E~qqmmFp8W1tgvnEAMtXFTFZPYc33ZaCaIJQD7UXcQRSRV7vjw39jhx49XFsmYV3K6~D8bN8U4sRJnKQpzOJGpOSEJWh88bII0XuA55bJsfrR4VEQ5AAAA",
    "hL51K8bxVvqX2Z4epXReUYlWTNUAK-1XdlbBd7e796s2A3icCQRhJvGlaa6PX~tgPvos-2IJp9hFQrVa0lyKyYmpQN9X7GOErtsL-JbMQpglQsEd94jDRAsiBuvgyPZij~NNdBxKRMDvNm9s7eovzhEFTAimTSB-sgeZ4Afxx2IrNXDFM6KS8AUm8YsaMldzX9zKQDeuV0slp4ZfIAVQhZZy9zTZSmUNPnXQR7XPh5w7FkXzmKMTKSyG~layJ3AorQWqzZXmykzf4z3CE4zkzQcwc0~ZIcAg9tYvM2AdQIdgeN6ISMim6L8q6ku6abuONkyw-NJTi3NopeGHZva21Tc3uHetsKoW434N24HBVUtIjJVjGsbZ7xBfz2xM5kyhPl6SlD-RJarCw47Rovmfc9Piq6q3S~Zw-rvRl-xDMJzwraIYNjAROouDwjI9Bqnguq9DH5uFBxf4uN69X7T~yWTAjdvelZKp6BGe~HGo7bNQjBmymhlH4erCKZEXOaxDAAAA"
};
#endif // ENABLE_I2PSAM

//
// Main network
//
unsigned int pnSeed[] =
{
    0x0
};

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";

        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xca;
        pchMessageStart[2] = 0xba;
        pchMessageStart[3] = 0xda;
        vAlertPubKey = ParseHex("04b2941a448ab9860beb73fa2f600c09bf9fe4d18d5ff0b3957bf94c6d177d61f88660d7c0dd9adef984080ddea03c898039759f66c2011c111c4394692f814962");
        nDefaultPort = 9377;
        nRPCPort = 9376;

        // ToDo: These proof of work limits need to be checked, PRIME's value hasn't been looked up yet.
        // GR NOTE to Team: 9.99 coders removed CBigNum, stop using.  Port to the new coding convention, with what your working on now....
        // ...had that figured out once, think our new type is CScriptNum(), need to look it up myself.
        bnProofOfWorkLimit[SCRYPT_ANC] = CBigNum().SetCompact(0x1e0ffff0); // As defined in Anoncoin 8.6....
        bnProofOfWorkLimit[SHA256D_BTC] = CBigNum(~uint256(0) >> 32);      /* Bitcoin difficulty is very large (taken from 9.99 code 12/2014 */
        bnProofOfWorkLimit[PRIME_XPM] = CBigNum(~uint256(0) >> 20);

// Genesis:
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
        const char* pszTimestamp = "02/Jun/2013:  The Universe, we're all one. But really, fuck the Central banks. - Anonymous 420";
        CTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << 0x0 << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1370190760;
        genesis.nBits    = 0x1e0ffff0;
        genesis.nNonce   = 347089008;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x2c85519db50a40c033ccb3d4cb729414016afa537c66537f7d3d52dcd1d484a3"));
        assert(genesis.hashMerkleRoot == uint256("0x7ce7004d764515f9b43cb9f07547c8e2e00d94c9348b3da33c8681d350f2c736"));

// ToDo: Named domains can NOT go into fixed, unless converted to ip addresses. Is that what is wanted here, on these first 2 entries? Or....?
//        vSeeds.push_back(CDNSSeedData("coinpool.in", "anoncoin.dnsseed.coinpool.in"));                              // Normal Seednode, NO DNS-SEED!
//        vSeeds.push_back(CDNSSeedData("anoncoin.net", "dnsseed01.anoncoin.net"));                                   // Normal Seednode, NO DNS-SEED!
        vSeeds.push_back(CDNSSeedData("anoncoin.darkgamex.ch", "anc.dnsseed01.anoncoin.darkgamex.ch"));             // K1773R's DNSSeed
        vSeeds.push_back(CDNSSeedData("www.virtual-currency.com", "anc.dnsseed01.anoncoin.virtual-currency.com"));  // keystroke's DNSSeed

#ifdef ENABLE_I2PSAM
/**
 * ToDo: Check me.  Brought all the seed data from 0.8.5.6 'master' net.cpp file, and rebuilt it for use in chainparams...
 *       See net.cpp->ThreadDNSAddressSeed() for how these are used.  Need to revisit testnet considerations.
 */
// As of 12/30/2014, these following entries were not working.  Renewing tests for awhile, or removed...
        i2pvSeeds.push_back(CDNSSeedData("d46o5wddsdrvg2ywnu4o57zeloayt7oyg56adz63xukordgfommq.b32.i2p", "d46o5wddsdrvg2ywnu4o57zeloayt7oyg56adz63xukordgfommq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("u2om3hgjpghqfi7yid75xdmvzlgjybstzp6mtmaxse4aztm23rwq.b32.i2p", "u2om3hgjpghqfi7yid75xdmvzlgjybstzp6mtmaxse4aztm23rwq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("htigbyeisbqizn63ftqw7ggfwfeolwkb3zfxwmyffygbilwqsswq.b32.i2p", "htigbyeisbqizn63ftqw7ggfwfeolwkb3zfxwmyffygbilwqsswq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("st4eyxcp73zzbpatgt26pt3rlfwb7g5ywedol65baalgpnhvzqpa.b32.i2p", "st4eyxcp73zzbpatgt26pt3rlfwb7g5ywedol65baalgpnhvzqpa.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("qgmxpnpujddsd5ez67p4ognqsvo64tnzdbzesezdbtb3atyoxcpq.b32.i2p", "qgmxpnpujddsd5ez67p4ognqsvo64tnzdbzesezdbtb3atyoxcpq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p", "7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p")); // lunokhod's seednode
        i2pvSeeds.push_back(CDNSSeedData("ypwvq7jcu3uwyg4ufjqt4a26ca6pxdcnshv6q2okmmjsof5dxzkq.b32.i2p", "ypwvq7jcu3uwyg4ufjqt4a26ca6pxdcnshv6q2okmmjsof5dxzkq.b32.i2p")); // keystroke's seednode

// Have been working well since testing the v0.9.4 clients
        i2pvSeeds.push_back(CDNSSeedData("a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p", "a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p")); // K1773R's seednode
        i2pvSeeds.push_back(CDNSSeedData("b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p", "b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p")); // K1773R's seednode (dnsseed01)
        i2pvSeeds.push_back(CDNSSeedData("72vaef5cmmlcilgvpeltcp77gutsnyic2l5khsgz7kyivla5lwjq.b32.i2p", "72vaef5cmmlcilgvpeltcp77gutsnyic2l5khsgz7kyivla5lwjq.b32.i2p")); // riddler's seednode
        i2pvSeeds.push_back(CDNSSeedData("qc37luxnbh3hkihxfl2e7nwosebh5sbfvpvjqwn7c3g5kqftb5qq.b32.i2p", "qc37luxnbh3hkihxfl2e7nwosebh5sbfvpvjqwn7c3g5kqftb5qq.b32.i2p")); // psi's seednode
        i2pvSeeds.push_back(CDNSSeedData("xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p", "xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p")); // Cryptoslave's seednode
#endif // ENABLE_I2PSAM

        base58Prefixes[PUBKEY_ADDRESS] = list_of(23);           // Anoncoin addresses start with A
        base58Prefixes[SCRIPT_ADDRESS] = list_of(5);
        base58Prefixes[SECRET_KEY] =     list_of(151);          // Anoncoin secret keys are the Public Key + 128
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x88)(0xAD)(0xE4);

        // Convert the pnSeeds array into usable address objects.
        // ToDo: Generate some usable ip (IPv4) addresses for the pnSeed array  See: contrib/seeds/ for details...
        // OrNot: ...as we migrate to I2P for everything, this becomes less of feature we even care about.
        for (unsigned int i = 0; i < ARRAYLEN(pnSeed); i++)
        {
            // It'll only connect to one or two seed nodes because once it connects,
            // it'll get a pile of addresses with newer timestamps.
            // Seed nodes are given a random 'last seen time' of between one and two
            // weeks ago.
            const int64_t nOneWeek = 7*24*60*60;
            struct in_addr ip;
            memcpy(&ip, &pnSeed[i], sizeof(ip));
            CAddress addr(CService(ip, GetDefaultPort()));
            addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            vFixedSeeds.push_back(addr);
        }
#ifdef ENABLE_I2PSAM
        // As of 12/26/2014, there are NO entries from above clearnet pnSeed array, only using I2P for fixed seeding...
        for (unsigned int i = 0; i < ARRAYLEN( I2pSeedAddresses ); i++ ) {
            const int64_t nOneWeek = 7*24*60*60;
            CService aServiceSeed( I2pSeedAddresses[i] );
            CAddress addr( aServiceSeed );
            addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            vFixedSeeds.push_back(addr);
        }
#endif
    }

    virtual const CBlock& GenesisBlock() const { return genesis; }
    virtual Network NetworkID() const { return CChainParams::MAIN; }

    virtual const vector<CAddress>& FixedSeeds() const { return vFixedSeeds; }
protected:
    CBlock genesis;
    vector<CAddress> vFixedSeeds;
};
static CMainParams mainParams;


//
// Testnet (v4)
//
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        strNetworkID = "testnet4";

        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xc4;
        pchMessageStart[2] = 0xb7;
        pchMessageStart[3] = 0xd4;
        // GR Note: 1/26/2015 - New secp256k1 key values generated for testnet....
        vAlertPubKey = ParseHex("0442ccd085e52f7b74ee594826e36e417706af91ff7e7236a430b2dd16fe9f1a8132d0718e0bf5a3b7105354bf5bf954330097b21824c26c466836df9538f3d33e");
        // Private key for alert generation, anyone on the development and test team can use it:
        // "3082011302010104204b164c9765248427d1b13b9dc4f11107629485f0c61de070d89ebae308822e25a081a53081a2020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a1440342000442ccd085e52f7b74ee594826e36e417706af91ff7e7236a430b2dd16fe9f1a8132d0718e0bf5a3b7105354bf5bf954330097b21824c26c466836df9538f3d33e"
        nDefaultPort = 19377;
        nRPCPort = 19376;
        strDataDir = "testnet4";

        // Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1317798646;
        genesis.nNonce = 385270584;
        hashGenesisBlock = genesis.GetHash();
        // ToDo: This assertion fails @ runtime, need to do the math:
        // assert(hashGenesisBlock == uint256("0xf5ae71e26c74beacc88382716aced69cddf3dffff24f384e1808905e0188f68f"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("anoncoin.net", "dnsseed01.anoncoin.net"));

        base58Prefixes[PUBKEY_ADDRESS] = list_of(111);      // Anoncoin v8 compatible testnet Public keys use this value
        base58Prefixes[SCRIPT_ADDRESS] = list_of(196);
        base58Prefixes[SECRET_KEY]     = list_of(239);      // Anoncoin testnet secret keys are the Public Key + 128
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x35)(0x87)(0xCF);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x35)(0x83)(0x94);
    }
    virtual Network NetworkID() const { return CChainParams::TESTNET; }
};
static CTestNetParams testNetParams;


//
// Regression test
//
class CRegTestParams : public CTestNetParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;

        // ToDo: Proof of work limits, for regression testing are very small, more than likely these should work
        bnProofOfWorkLimit[SCRYPT_ANC] = CBigNum(~uint256(0) >> 1);
        bnProofOfWorkLimit[SHA256D_BTC] = CBigNum(~uint256(0) >> 1);
        bnProofOfWorkLimit[PRIME_XPM] = CBigNum(~uint256(0) >> 1);

        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 2;
        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 19444;
        nRPCPort = 19443;
        strDataDir = "regtest";
        // ToDo: This assertion fails @ runtime, need to do the math:
        // assert(hashGenesisBlock == uint256("0x9372df8b4c0144d2238b73d65ce81b5eb37ec416c23fc29307b89de4b0493cf8"));

        vSeeds.clear();  // Regtest mode doesn't have any DNS seeds.
    }

    virtual bool RequireRPCPassword() const { return false; }
    virtual Network NetworkID() const { return CChainParams::REGTEST; }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = &mainParams;

const CChainParams &Params() {
    return *pCurrentParams;
}

void SelectParams(CChainParams::Network network) {
    switch (network) {
        case CChainParams::MAIN:
            pCurrentParams = &mainParams;
            break;
        case CChainParams::TESTNET:
            pCurrentParams = &testNetParams;
            break;
        case CChainParams::REGTEST:
            pCurrentParams = &regTestParams;
            break;
        default:
            assert(false && "Unimplemented network");
            return;
    }
}

bool SelectParamsFromCommandLine() {
    bool fRegTest = GetBoolArg("-regtest", false);
    bool fTestNet = GetBoolArg("-testnet", false);

    if (fTestNet && fRegTest) {
        return false;
    }

    if (fRegTest) {
        SelectParams(CChainParams::REGTEST);
    } else if (fTestNet) {
        SelectParams(CChainParams::TESTNET);
    } else {
        SelectParams(CChainParams::MAIN);
    }
    return true;
}
