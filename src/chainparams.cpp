// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2015 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
// Anoncoin-config.h has been loaded...

#include "block.h"
#include "amount.h"
#include "protocol.h"
#include "random.h"
#include "timedata.h"
// #include "util.h"

#include <assert.h>

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
// As of March 30, 2015
static const string I2pDestinationSeeds[] = {
    "HNaqLnETmOcfEzbmzyh5XTQJCqxHVBk0mdvAbc2CZbIi5OxelNMREYLo1U2wGIOgNFSgfdvefOroouRpC8vsoXJ2ARYFTGH~wKd4KVv6sEdD3bVhtVYW03nuwz6KP9~otTO6QvX2uRSQNJlNGqGN7L4uNgzHuznU0jpCmpARDC4y95144iI7hWawxFnqthnzEgPNSGp5lqM7GQzDplWTDDAoF1GcJqPzVY0uoMWvLCfCNUpHw2wO0egKuAlv6Uz9IK0dQl7S0XmE9Jnt7I2ZKs4-RlX9lRf-hEHgMY9Y6k-Brl-TVwTkzmsvNPYTpbOxvkXIBru5Yi~IaGsImjhmvGT4FAj02b-0HNqGWfaiKOHl1k~O8geUeIDO2fA5oyJQmcwstMjmU24DMFeCBCB0MFL0xK8bJkUZkrzhwIY-Z2U9GqxEjt53m80-PHvUsDOGAKOpeT0fnx3vLVEJFeHS6K~G2XG24EzcmSCLwcPEFptQpxWrR4nSxya6KTPp6ZUCAAAA",
    "3bfyLFHqvqCZuAtVLIR8R4buAW3e1VQ6-S6jKrXNX5Hgq-b1fYbLqniP5MjErFzhTlu-Umk9n-E9jnLsTXJQNkZB5RkNnSMSkxPsQpFWJ8pbN-qXQf1KtJ0UYEA0zi7Eq33caToH5CtGkMvik5zOFtPKQEIbgzzu6N7tvb2huGvpBTJQdTdQSI1hjfQbpnTkZ-bsJrCWs54nu13OAi~AuSvKJtGl7sLrA1z~7YyJfA84pOLO5oMxzMJnjhARMJFTyG6RxHOeTHOlNqoLVCY7sLI2IZpJrQAJLRHTTFbUKptuj4v3W8lcZDFXa4PmK-xA6NuicFC~b6YaOMEiAL1E7w72pHTwcIxmKP1FF3U57bwDGU7126n3P8PECouG0o6QXlTsYHt~Qc4pZGy4CQtINMFcx2ik0KKxOACTu8XzJ2~7FHINgMzPnd87vNK6mfBDYZ3Np~XwEZLF1GkbYYqLVKvh8NdWiqNBt5zzR0wq3XKTxCljpIs9JIP2FNLaAh0KAAAA",
    "y2arPA7vC36rA-SrPb2spKxvhNeCvx7ZMhdO-F9csEvWYxvx1a5NK0K2Qfo0cfwh3KrNphYudAGqYfwhi~a4xUnFtZ1-ruCtQ33i298HnnDeRJv0T1~Y9dlvlN9xQnBggH2EjVv8~I1p2W2GyGliPZ0NUzjgdmX0N1jNCP5tJ1lMUqsJrdvGi~bEmbMDOA7y6-pnyrfRC6lk7qlIuT~kJh8l-HerJ0gEzu0ByZ~YDTK4EpWaCQgW68gKIgK3pKnscfILkiZwiRHkG5kWugBeJixIO3MPOAW17HdVG43EzLAlnC1BW8vkUXKI37Ry4cfylCz-90HPfRmwom-E3ZHiEoHQFAc43l9f2~KZzmtj-7u21~e47daGnoMGjta8ADwekmd-MieVFQeJ27fsRiCQkLDTq63ONbcTqIulXGhffXZtkyoq0qo-eVl3ErS6YyVpnhvvkuP46Gh81Db4zWbu~NEQq~qb1X0JhaaiufVTY2pNgHbNVinJjyjzq2ip3dGMAAAA",
    "d-Puxd~OdmFGywsIE4cIpbCfZLId8xWJEeelq18VgnZykEYiPS0K0C9w7eb2UMfeqamYejmODi9ITePyZpyOT3vRgKR~I88AiQbiH1YjiR17-R7KDRJQXmbKfOqMpEMW7JUh-hZBI87PNkC4TTv65lSpKyt8Kz~SaX21jVEMX34klJg7ZI48JIaSvRXWFPCZoyadKOryjPIS-tqVQyX4yEbVcypLfouJ02uRYvo2SAyluDBpZ71Rn5FpnlDKkLx0srh5k6CJYMz7vozNYUub9xvKEZK0SLcKsLzy~FRyuHEbOHUZ76~ibvxUUWORt4hHdT3O-WDre17pMDnuEG8YDGHPotX-Y7CcfH-fXRacVLwhK4Y5k9BvdmiuaZZoU3GxOIGxyMGeK5MJhinNBNdZpyEvUnFPpfu7R33offpE50vKmnwim9YZVa76VG3xlRpYWu9Gj2IAfzR-3JSTvtxFPqMDHR2-D-MOeOp98Qe0CtHcAvx3kHhuCJW4gbwOJa2TAAAA",
    "UmCjD3EY6cEeCv5N9b9cRWBkU8~EwacG7nY6Tx9vPCne5fP-LIMxR~42hyrPGN4-ZXpGbns5Pu4JEvckj~mkXnJxM2-b6An2QsVYJCCqBmnqjK43HZC32s0Z2qFW5Y3agNs3eAtSK~GXy~IvQYewhSlmIOs8W~eAkLzF~C2~VzSmaWs55uBzLUINeJr7E7woPFnnrP8EjfMJHNUbmTkw1bepf3K7~BWy-LBlBaWvylTtRiAPDlqI2Q9ZZUn3wYQbHLZSGHkjdouRJ3zndcFTjwTEHzh4XQcHQK9hud7USUGnasl1IXsxizJo5lzsP5SvytkzdI07FjhpxSAYDKHn6SW4UXM5rQmAq44zHu4AzY76p3Wk9iUtwz1rFQNLqTwouZYe5hviiB1GAaWZBOhz9KsG0DzxeFRV5ou8Ps8-xlhU36mjrw3L6CYEpT6WejbNnXlIC~aUI057MAjr2sLfvVBhmycwJdSMOIT5fVkm8aTmiIM-YgpYUpInNHxH5fKEAAAA",
    "rR~bh73tiGezgHUs82cKHX83z~Jmp7pfyTWbwjrQgzM3at-DUh7TVQnsB5FnfJrbH89P1zXTdTqUR~OjQnjlt1WZZISrBVoUNe0O8xruhUsGR77WmRbSrb1K1o6LG0eX9dZk7ZTrLa-Bj0xG3jk8IYOLOdkCjld7VzBdkTcGN7G8QWx33cJn2778i9gSGNza0egyiEd4QnCjgZlI0wYTyLz-35F~to9unN6uq8AMK9uJa8a67l~HyDl1dGC5Ye~KKRBWgD5ZCoJAJBOipjsdAZqHUTa64xX4cX6EPEVBdikAZrRQS2l-FyLcuHvXFQ~rRuOEfO0VGmhJOIzNMdvK4UfsT118kt~pPe1nI4GEQ1xWP5thcFv-WN4mtDzU4fqFivhb7hijygUhQ7zrZFCbDypzYZTPBSXjABknbcz8j0iPy9GgEHsF8Zltt~i77Vc2e8cTcNMSpxm1uvMFJiB373Lri2vGj5oB0apixV-HUj1Bdpb1HTm96PDpf963jhj~AAAA",
    "6Njy3LSSCG4KrIeEubXoyWIw~FIKIVesPuzWjNTyNxwmvK65udJCmvTuQyrkPNkARZDNE4-pL4aAI4EhOsWzE3LkSOsvQTG4yhzRE7r97Fg0x~nE50SnrqWoJrsGHUcJtq-wL77WSzdcMD-aA5z4fROKwf3kvXCiOcRu9V9VdOlPuiynl838zOIOqApIFUfKzNcKMvEGfa9D998dL2FI-QnNgjLTcYn1YIK7muNSrhBgDJIfZPZRi21K-Q4fUSXRyutx6pP~ipLmsCJZOFrf-y0GThoSLy2DGNJ77s~UB~ANyuglLFvqDWqKp8KOuU4GTpmEfLPrurKr2kXc3jbqmw3YLVonJev-wcH2boS9WLcODtyytzQLBfz83bMACI-YbBQESjD1dZAAR3zDztKzdQcxzbyMGuoJe-6~QrEy~35TPL2TKIr2BjTWNwt3HQ65NFHLsA7HneiLf0Ntr~wcewxK7bRY3~qI7J7oUsf-QhyjkKqHJ6ohmAMTY1U3CT3ZAAAA",
    "kvGu5BRTohvsGsNqkQW5glv-Q1Zxzwm40mFWK9g8Jte7561SmQxkKrklKX1bfuoCQ9hn1Ty3406-7xQb7bhoD9YC1mPm9hoBDNBzb4f8jn~Zry4SUY~AfCSvD1EBW4cvme9VZ1mIu3Jtj9qyv7j0gKgGyXAkX55gXT5EhRUoWspsWxao18PsB-HUTIyMP-At8GOcAeoI4GXbFSCa170o6pggoJrsXfg28Iww1VJB7MQl5IE2Q~i-WGuynKjmVUFshH9tC-vIW0nvaY-vnxNL7GNVBHVmcyi0I3nYxym~74BfdEioV-c5Q20aztYzxgvTelJu-56AMTP9GJKnn9BjVhKs2jppZLd6WFs30wGZMoK3a7Q8~kRcHKXX82pGisnBUxAbFQyOhAUOMw2wvKUeHh6-WBE8WU6zCALyLJ2532tvf8Bb~k0S7ZD96vgaMGGiR2daHcFf1e~zmVILPf27ya8bg874HZoq7WcrUsF5tLha2cMHFx1RXwMAAPlL3mYVAAAA"
//    "8w-NRnXaxoGZdyupTXX78~CmF8csc~RhJr8XR2vMbNpazKhGjWNfRjhCmwqXkkW9vwkNjovW2AAbof7PfVnMCff0sHSxMTiBNsH8cuHJS2ckBSJI3h4G4ffJLc5gflangrG1raHKrMXCw8Cn56pisx4RKokEKUYdeEPiMdyJUO5yjZW2oyk4NpaUaQCqFmcglIvNOCYzVe~LK124wjJQAJc5iME1Sg9sOHaGMPL5N2qkAm5osOg2S7cZNRdIkoNOq-ztxghrv5bDL0ybeC0sfIQzvxDiKugCrSHEHCvwkA~xOu9nhNlUvoPDyCRRi~ImeomdNoqke28di~h2JF7wBGE~3ACxxOMaa0I~c9LV3O7pRU2Xj9HDn76eMGL7YCcCU4dRByu97oqfB3E~qqmmFp8W1tgvnEAMtXFTFZPYc33ZaCaIJQD7UXcQRSRV7vjw39jhx49XFsmYV3K6~D8bN8U4sRJnKQpzOJGpOSEJWh88bII0XuA55bJsfrR4VEQ5AAAA",
//    "hL51K8bxVvqX2Z4epXReUYlWTNUAK-1XdlbBd7e796s2A3icCQRhJvGlaa6PX~tgPvos-2IJp9hFQrVa0lyKyYmpQN9X7GOErtsL-JbMQpglQsEd94jDRAsiBuvgyPZij~NNdBxKRMDvNm9s7eovzhEFTAimTSB-sgeZ4Afxx2IrNXDFM6KS8AUm8YsaMldzX9zKQDeuV0slp4ZfIAVQhZZy9zTZSmUNPnXQR7XPh5w7FkXzmKMTKSyG~layJ3AorQWqzZXmykzf4z3CE4zkzQcwc0~ZIcAg9tYvM2AdQIdgeN6ISMim6L8q6ku6abuONkyw-NJTi3NopeGHZva21Tc3uHetsKoW434N24HBVUtIjJVjGsbZ7xBfz2xM5kyhPl6SlD-RJarCw47Rovmfc9Piq6q3S~Zw-rvRl-xDMJzwraIYNjAROouDwjI9Bqnguq9DH5uFBxf4uN69X7T~yWTAjdvelZKp6BGe~HGo7bNQjBmymhlH4erCKZEXOaxDAAAA"
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

        // As of April '15, SCRYPT is all that matters, future mining a SHA256D algo may soon be supported as well, set those limits here.
        bnProofOfWorkLimit[ALGO_SCRYPT] = uint256().SetCompact(0x1e0ffff0);  // As defined in Anoncoin 8.6....
        bnProofOfWorkLimit[ALGO_SHA256D] = ~uint256(0) >> 32;       // ToDo: set SHA256D min work

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
        CMutableTransaction txNew;
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
         *  See net.cpp->ThreadDNSAddressSeed() for how these are used.
         */
        i2pvSeeds.push_back(CDNSSeedData("7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p", "7zbwzykhyjcmmessswamkxfyya7hioiy2oq7voaw27625qwruqia.b32.i2p")); // lunokhod's seednode
        i2pvSeeds.push_back(CDNSSeedData("b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p", "b7ziruwpk7g2e44xyomnc2nu5tx7bc2f2ai4dzi66uxm3bc3qttq.b32.i2p")); // K1773R's seednode (dnsseed01)
        i2pvSeeds.push_back(CDNSSeedData("ypwvq7jcu3uwyg4ufjqt4a26ca6pxdcnshv6q2okmmjsof5dxzkq.b32.i2p", "ypwvq7jcu3uwyg4ufjqt4a26ca6pxdcnshv6q2okmmjsof5dxzkq.b32.i2p")); // keystroke's seednode

        // As of March 30, 2015 these entries were not working.
//        i2pvSeeds.push_back(CDNSSeedData("d46o5wddsdrvg2ywnu4o57zeloayt7oyg56adz63xukordgfommq.b32.i2p", "d46o5wddsdrvg2ywnu4o57zeloayt7oyg56adz63xukordgfommq.b32.i2p"));
//        i2pvSeeds.push_back(CDNSSeedData("u2om3hgjpghqfi7yid75xdmvzlgjybstzp6mtmaxse4aztm23rwq.b32.i2p", "u2om3hgjpghqfi7yid75xdmvzlgjybstzp6mtmaxse4aztm23rwq.b32.i2p"));
//        i2pvSeeds.push_back(CDNSSeedData("htigbyeisbqizn63ftqw7ggfwfeolwkb3zfxwmyffygbilwqsswq.b32.i2p", "htigbyeisbqizn63ftqw7ggfwfeolwkb3zfxwmyffygbilwqsswq.b32.i2p"));
//        i2pvSeeds.push_back(CDNSSeedData("st4eyxcp73zzbpatgt26pt3rlfwb7g5ywedol65baalgpnhvzqpa.b32.i2p", "st4eyxcp73zzbpatgt26pt3rlfwb7g5ywedol65baalgpnhvzqpa.b32.i2p"));
//        i2pvSeeds.push_back(CDNSSeedData("qgmxpnpujddsd5ez67p4ognqsvo64tnzdbzesezdbtb3atyoxcpq.b32.i2p", "qgmxpnpujddsd5ez67p4ognqsvo64tnzdbzesezdbtb3atyoxcpq.b32.i2p"));
//        i2pvSeeds.push_back(CDNSSeedData("a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p", "a4gii55rnvv22qm2ojre2n67bzms5utr4k3ckafwjdoym2cqmv2q.b32.i2p")); // K1773R's seednode
//        i2pvSeeds.push_back(CDNSSeedData("72vaef5cmmlcilgvpeltcp77gutsnyic2l5khsgz7kyivla5lwjq.b32.i2p", "72vaef5cmmlcilgvpeltcp77gutsnyic2l5khsgz7kyivla5lwjq.b32.i2p")); // riddler's seednode
//        i2pvSeeds.push_back(CDNSSeedData("qc37luxnbh3hkihxfl2e7nwosebh5sbfvpvjqwn7c3g5kqftb5qq.b32.i2p", "qc37luxnbh3hkihxfl2e7nwosebh5sbfvpvjqwn7c3g5kqftb5qq.b32.i2p")); // psi's seednode
//        i2pvSeeds.push_back(CDNSSeedData("xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p", "xynjl64xlviqhkjl2fbvupj7y3wct46jtayoxm2ksba6tqzo6tsa.b32.i2p")); // Cryptoslave's seednode
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

#ifdef ENABLE_I2PSAM
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
        bnProofOfWorkLimit[ALGO_SCRYPT] = ~uint256(0) >> 17;
        bnProofOfWorkLimit[ALGO_SHA256D] = ~uint256(0) >> 20;

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
        bnProofOfWorkLimit[ALGO_SCRYPT] = ~uint256(0) >> 10;
        bnProofOfWorkLimit[ALGO_SHA256D] = ~uint256(0) >> 10;

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
