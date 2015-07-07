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

//
// Main network clearnet fixed seeds, atm none defined
//
unsigned int pnSeed[] =
{
    0x0
};

#ifdef ENABLE_I2PSAM
static const string I2pSeedAddresses[] = {
    "8w-NRnXaxoGZdyupTXX78~CmF8csc~RhJr8XR2vMbNpazKhGjWNfRjhCmwqXkkW9vwkNjovW2AAbof7PfVnMCff0sHSxMTiBNsH8cuHJS2ckBSJI3h4G4ffJLc5gflangrG1raHKrMXCw8Cn56pisx4RKokEKUYdeEPiMdyJUO5yjZW2oyk4NpaUaQCqFmcglIvNOCYzVe~LK124wjJQAJc5iME1Sg9sOHaGMPL5N2qkAm5osOg2S7cZNRdIkoNOq-ztxghrv5bDL0ybeC0sfIQzvxDiKugCrSHEHCvwkA~xOu9nhNlUvoPDyCRRi~ImeomdNoqke28di~h2JF7wBGE~3ACxxOMaa0I~c9LV3O7pRU2Xj9HDn76eMGL7YCcCU4dRByu97oqfB3E~qqmmFp8W1tgvnEAMtXFTFZPYc33ZaCaIJQD7UXcQRSRV7vjw39jhx49XFsmYV3K6~D8bN8U4sRJnKQpzOJGpOSEJWh88bII0XuA55bJsfrR4VEQ5AAAA",
    "hL51K8bxVvqX2Z4epXReUYlWTNUAK-1XdlbBd7e796s2A3icCQRhJvGlaa6PX~tgPvos-2IJp9hFQrVa0lyKyYmpQN9X7GOErtsL-JbMQpglQsEd94jDRAsiBuvgyPZij~NNdBxKRMDvNm9s7eovzhEFTAimTSB-sgeZ4Afxx2IrNXDFM6KS8AUm8YsaMldzX9zKQDeuV0slp4ZfIAVQhZZy9zTZSmUNPnXQR7XPh5w7FkXzmKMTKSyG~layJ3AorQWqzZXmykzf4z3CE4zkzQcwc0~ZIcAg9tYvM2AdQIdgeN6ISMim6L8q6ku6abuONkyw-NJTi3NopeGHZva21Tc3uHetsKoW434N24HBVUtIjJVjGsbZ7xBfz2xM5kyhPl6SlD-RJarCw47Rovmfc9Piq6q3S~Zw-rvRl-xDMJzwraIYNjAROouDwjI9Bqnguq9DH5uFBxf4uN69X7T~yWTAjdvelZKp6BGe~HGo7bNQjBmymhlH4erCKZEXOaxDAAAA"
};
#endif // ENABLE_I2PSAM

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

        /** \brief
         * Starting with Anoncoin v0.9.4.5, the following vAlertPubKey ECDSA (Elliptical Curve DSA) value will be used as the public key
         * for generating new alerts on the Anoncoin Network. The private key is not available, nor is it in the hands of
         * just one individual. This change will however allow the current development team to generate Alert messages as required.
         * As of 3/3/2015  Holders of the new private key are: K1773R, Lunokhod, Cryptoslave and myself GroundRod.
         * If an alert is necessary, it can be used to quickly inform all the nodes of an important development or software upgrade.
         *
         */
        vAlertPubKey = ParseHex("04c6db35c11724e526f6725cc5bd5293b4bc9382397856e1bcef7111fb44ce357fd12442b34c496d937a348c1dca1e36ae0c0e128905eb3d301433887e8f0b4536");

        // Effective 3/3/2015, the following Alert key is no longer being used & has been replaced.
        // One last build of the v8 client is being prepared to support the new Alert key above, that is version 0.8.5.7.
        vOldAlertPubKey = ParseHex("04b2941a448ab9860beb73fa2f600c09bf9fe4d18d5ff0b3957bf94c6d177d61f88660d7c0dd9adef984080ddea03c898039759f66c2011c111c4394692f814962");

        nDefaultPort = 9377;

        // As of March '15, SCRYPT is all that matters, future mining with a SHA256D algo may soon be supported as well, set those limits here.
        bnProofOfWorkLimit[ALGO_SCRYPT] = CBigNum().SetCompact(0x1e0ffff0);  // As defined in Anoncoin 8.6....
        bnProofOfWorkLimit[ALGO_SHA256D] = CBigNum(~uint256(0) >> 32);       // ToDo: Bitcoin difficulty is very high (taken from 9.99 code 12/2014)

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
        // printf("Mainnet Genesis Hash: %s, nBits: %08x, bnLimt: %08x \n", hashGenesisBlock.ToString().c_str(), genesis.nBits, bnProofOfWorkLimit[ALGO_SCRYPT].GetCompact());
        assert(hashGenesisBlock == uint256("0x2c85519db50a40c033ccb3d4cb729414016afa537c66537f7d3d52dcd1d484a3"));
        assert(genesis.hashMerkleRoot == uint256("0x7ce7004d764515f9b43cb9f07547c8e2e00d94c9348b3da33c8681d350f2c736"));

        // vSeeds.push_back(CDNSSeedData("coinpool.in", "anoncoin.dnsseed.coinpool.in"));                           // Normal Seednode, NO DNS-SEED!
        // vSeeds.push_back(CDNSSeedData("anoncoin.net", "dnsseed01.anoncoin.net"));                                // Normal Seednode, NO DNS-SEED!
        vSeeds.push_back(CDNSSeedData("anoncoin.darkgamex.ch", "anc.dnsseed01.anoncoin.darkgamex.ch"));             // K1773R's DNSSeed
        vSeeds.push_back(CDNSSeedData("www.virtual-currency.com", "anc.dnsseed01.anoncoin.virtual-currency.com"));  // keystroke's DNSSeed

#ifdef ENABLE_I2PSAM
        /**
         * ToDo: Brought all the seed data from 0.8.5.6 'master' net.cpp file, and rebuilt it for use here in chainparams...
         *       See net.cpp->ThreadDNSAddressSeed() for how these are used.
         */
        // As of 12/30/2014, these following entries were not working.  Renewing tests for awhile, or they need to be removed or updated...
        i2pvSeeds.push_back(CDNSSeedData("d46o5wddsdrvg2ywnu4o57zeloayt7oyg56adz63xukordgfommq.b32.i2p", "d46o5wddsdrvg2ywnu4o57zeloayt7oyg56adz63xukordgfommq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("u2om3hgjpghqfi7yid75xdmvzlgjybstzp6mtmaxse4aztm23rwq.b32.i2p", "u2om3hgjpghqfi7yid75xdmvzlgjybstzp6mtmaxse4aztm23rwq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("htigbyeisbqizn63ftqw7ggfwfeolwkb3zfxwmyffygbilwqsswq.b32.i2p", "htigbyeisbqizn63ftqw7ggfwfeolwkb3zfxwmyffygbilwqsswq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("st4eyxcp73zzbpatgt26pt3rlfwb7g5ywedol65baalgpnhvzqpa.b32.i2p", "st4eyxcp73zzbpatgt26pt3rlfwb7g5ywedol65baalgpnhvzqpa.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("qgmxpnpujddsd5ez67p4ognqsvo64tnzdbzesezdbtb3atyoxcpq.b32.i2p", "qgmxpnpujddsd5ez67p4ognqsvo64tnzdbzesezdbtb3atyoxcpq.b32.i2p"));
        i2pvSeeds.push_back(CDNSSeedData("7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p", "7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p")); // lunokhod's seednode
        i2pvSeeds.push_back(CDNSSeedData("ypwvq7jcu3uwyg4ufjqt4a26ca6pxdcnshv6q2okmmjsof5dxzkq.b32.i2p", "ypwvq7jcu3uwyg4ufjqt4a26ca6pxdcnshv6q2okmmjsof5dxzkq.b32.i2p")); // keystroke's seednode

        // Has been working well with tests in late '14 and early '15 using the v0.9.4 clients
        i2pvSeeds.push_back(CDNSSeedData("a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p", "a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p")); // K1773R's seednode
        i2pvSeeds.push_back(CDNSSeedData("b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p", "b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p")); // K1773R's seednode (dnsseed01)
        i2pvSeeds.push_back(CDNSSeedData("72vaef5cmmlcilgvpeltcp77gutsnyic2l5khsgz7kyivla5lwjq.b32.i2p", "72vaef5cmmlcilgvpeltcp77gutsnyic2l5khsgz7kyivla5lwjq.b32.i2p")); // riddler's seednode
        i2pvSeeds.push_back(CDNSSeedData("qc37luxnbh3hkihxfl2e7nwosebh5sbfvpvjqwn7c3g5kqftb5qq.b32.i2p", "qc37luxnbh3hkihxfl2e7nwosebh5sbfvpvjqwn7c3g5kqftb5qq.b32.i2p")); // psi's seednode
        i2pvSeeds.push_back(CDNSSeedData("xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p", "xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p")); // Cryptoslave's seednode
#endif // ENABLE_I2PSAM

        // Because the prefix bytes are attached to a base256 encoding (ie, binary) of known width,  they affect the leading cinquantoctal digit of the corresponding base58
        // representation of the same number.  The 23 below, is why mainnet pay-to-pubkey txouts always start with the letter A
        base58Prefixes[PUBKEY_ADDRESS] = list_of(23);           // the pay-to-pubkey prefix byte
        base58Prefixes[SCRIPT_ADDRESS] = list_of(5);            // the pay-to-script-hash prefix byte
        base58Prefixes[SECRET_KEY] =     list_of(151);          // Anoncoin secret keys are the Public Key + 128
        // What we have here are the same as Bitcoin values, and explained below as follows:
        // The four-byte prefixes correspond to cinquantoctal prefixes 'xpub' and 'xprv' on mainnet and 'tpub' and 'tprv' on testnet.
        // Somebody with far too much time to invest in encoding transformations figured out what range of numeric values would start
        // with 'xpub' and 'xpriv' etc when converted to base58 from a binary number 4 bytes wider than a key value, then exactly which
        // four bytes to prefix to ensure that the base256 representation always yields a number in that range.
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x88)(0xAD)(0xE4);

        // Convert the pnSeeds array into usable address objects.
        // ToDo: Generate some usable ip (IPv4) addresses for the pnSeed array  See: contrib/seeds/ for details...
        // Or Not: ...as we migrate to I2P for everything, this becomes less of a feature we even care about.
        // WARNING - Because there are no clearnet seeds, there is a line of code to not load fixed entries unless i2p is enabled.
        for (unsigned int i = 0; i < ARRAYLEN(pnSeed); i++)
        {
            // Only connect to one or two seed nodes because once it connects,
            // it will get a pile of addresses with newer timestamps. So Seed
            // nodes are given a random 'last seen time' of between one and two weeks ago.
            const int64_t nOneWeek = 7*24*60*60;
            struct in_addr ip;
            memcpy(&ip, &pnSeed[i], sizeof(ip));
            CAddress addr(CService(ip, GetDefaultPort()));
            addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            vFixedSeeds.push_back(addr);
        }
#ifdef ENABLE_I2PSAM
        // As of 12/26/2014, there are NO entries for the above clearnet pnSeed array,
        // only using I2P for fixed seeding now, the strings are base64 encoded I2P
        // Destination addresses, and we set the port to 0.
        for (unsigned int i = 0; i < ARRAYLEN( I2pSeedAddresses ); i++ ) {
            const int64_t nOneWeek = 7*24*60*60;
            // Fixed seed nodes get our standard services bits set, this is after creating a CService obj with port 0,
            // and a CNetAddr obj, where setspecial is called given the I2P Destination string so it is setup correctly
            // with our new GarlicCat value for routing...
            CAddress addr( CService( CNetAddr( I2pSeedAddresses[i] ), 0 ), NODE_NETWORK | NODE_BLOOM | NODE_I2P );
            addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            vFixedSeeds.push_back(addr);
        }
#endif
    }

    virtual const CBlock& GenesisBlock() const { return genesis; }
    virtual const vector<CAddress>& FixedSeeds() const { return vFixedSeeds; }
protected:
    CBlock genesis;
    vector<CAddress> vFixedSeeds;
};
// This initializes the 1st CMainParams object, used as the primary network type, under normal operating modes.
static CMainParams mainParams;


//
// Testnet.
//
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        // These values have been set to the same as the v0.8.5.6 client had, so testing should be possible with that client, although maynot be required.
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xc4;
        pchMessageStart[2] = 0xa7;
        pchMessageStart[3] = 0x4b;

        strNetworkID = "testnet";

        // GR Note: 1/26/2015 - New secp256k1 key values generated for testnet....
        vAlertPubKey = ParseHex("0442ccd085e52f7b74ee594826e36e417706af91ff7e7236a430b2dd16fe9f1a8132d0718e0bf5a3b7105354bf5bf954330097b21824c26c466836df9538f3d33e");
        // Private key for alert generation, anyone on the development and test team can use it:
        // "3082011302010104204b164c9765248427d1b13b9dc4f11107629485f0c61de070d89ebae308822e25a081a53081a2020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704410479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a1440342000442ccd085e52f7b74ee594826e36e417706af91ff7e7236a430b2dd16fe9f1a8132d0718e0bf5a3b7105354bf5bf954330097b21824c26c466836df9538f3d33e"

        nDefaultPort = 19377;

        // ToDo: Proof of work limits for testnet.  Adjust as needed...
        bnProofOfWorkLimit[ALGO_SCRYPT] = CBigNum(~uint256(0) >> 17);
        bnProofOfWorkLimit[ALGO_SHA256D] = CBigNum(~uint256(0) >> 20);

        // Modify the testnet genesis block so the timestamp is valid for a later start.
        // These values have been set to the same as the v0.8.5.6 client had, so testing should be possible with that client, although maynot be required.
        genesis.nTime = 1373625296;
        genesis.nNonce = 346280655;
        hashGenesisBlock = genesis.GetHash();

        // This Genesis block hash matches the v0.8.5.6 client builds
        // printf("Testnet Genesis Hash: %s, nBits: %08x, bnLimt: %08x \n", hashGenesisBlock.ToString().c_str(), genesis.nBits, bnProofOfWorkLimit[ALGO_SCRYPT].GetCompact());
        assert(hashGenesisBlock == uint256("0x66d320494074f837363642a0c848ead1dbbbc9f7b854f8cda1f3eabbf08eb48c"));

        // During intialization, the DNS and Fixed seed node vectors are filled up as the class is derived from CMainParams, so has a copy of those values.
        // Dump those here, and add any that might really be useful for testnet...
        vFixedSeeds.clear();
        vSeeds.clear();
#ifdef ENABLE_I2PSAM
        i2pvSeeds.clear();
#endif
        // vSeeds.push_back(CDNSSeedData("anoncoin.net", "dnsseed01.anoncoin.net"));

        base58Prefixes[PUBKEY_ADDRESS] = list_of(111);      // Anoncoin v8 compatible testnet Public keys use this value
        base58Prefixes[SCRIPT_ADDRESS] = list_of(196);
        base58Prefixes[SECRET_KEY]     = list_of(239);      // Anoncoin testnet secret keys start with the same prefix as the Public Key + 128
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x35)(0x87)(0xCF);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x35)(0x83)(0x94);
    }
};
// Keep in mind, this is a class derived from CMainParams which is derived from CChainParams.  If your trying to debug startup code
// you will see, the CMainParams object created twice, first from the creation of the static variable mainParams (above), then the
// first CTestNetParams code get executed when this variable is created and initialized...
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
        bnProofOfWorkLimit[ALGO_SCRYPT] = CBigNum(~uint256(0) >> 1);
        bnProofOfWorkLimit[ALGO_SHA256D] = CBigNum(~uint256(0) >> 1);

        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 2;
        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 19444;

        // printf("RegTest Genesis Hash: %s, nBits: %08x, bnLimt: %08x \n", hashGenesisBlock.ToString().c_str(), genesis.nBits, bnProofOfWorkLimit[ALGO_SCRYPT].GetCompact());
        assert(hashGenesisBlock == uint256("0x03ab995e27af2435ad33284ccb89095e6abe47d0846a4e8c34a3d0fc2d167ceb"));

        vSeeds.clear();  // Regtest mode doesn't have any DNS seeds.
    }

    virtual bool RequireRPCPassword() const { return false; }
};

// Keep in mind, this is a class derived from CTestNetParams which is derived from CMainParams which is derived from CChainParams.
// If your trying to debug startup code you will see the CMainParams object created, then the CTestNetParams code get executed again
// then finally when this variable is created and initialized you'll have the one being used for Regression testing...
static CRegTestParams regTestParams;

// This pointer is now set in SelectParams, it was defaulting to mainParams, making debugging problems difficult,
// now if any call is made before that, say to query the parameters via Param(), an assertion will fail there too...
static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(CBaseChainParams::Network network) {
    switch (network) {
        case CBaseChainParams::MAIN:
            return mainParams;
        case CBaseChainParams::TESTNET:
            return testNetParams;
        case CBaseChainParams::REGTEST:
            return regTestParams;
        default:
            assert(false && "Unimplemented network");
            return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine() {
    bool fRegTest = GetBoolArg("-regtest", false);
    bool fTestNet = GetBoolArg("-testnet", false);

    if (fTestNet && fRegTest) {
        return false;
    }

    if (fRegTest) {
        SelectParams(CBaseChainParams::REGTEST);
    } else if (fTestNet) {
        SelectParams(CBaseChainParams::TESTNET);
    } else {
        SelectParams(CBaseChainParams::MAIN);
    }
    return true;
}
