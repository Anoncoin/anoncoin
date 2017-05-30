// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ANONCOIN_CHAIN_PARAMS_H
#define ANONCOIN_CHAIN_PARAMS_H

// Many builder specific things set in the config file, ENABLE_WALLET is a good example.
// Don't forget to include it this way in your source or header files.  GR Note: This
// method is now preferred, with a note in the source file about how the builder config
// file has now been loaded.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "chainparamsbase.h"
#include "protocol.h"
#include "uint256.h"

#include <vector>

using namespace std;

typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

class CAddress;
class CBlock;

// DNS seeds
// Each pair gives a source name and a seed name.
// The first name is used as information source for addrman.
// The second name should resolve to a list of seed addresses.
struct CDNSSeedData {
    string name, host;
    CDNSSeedData(const string &strName, const string &strHost) : name(strName), host(strHost) {}
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Anoncoin system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
        EXT_SECRET_KEY,

        MAX_BASE58_TYPES
    };

    enum MinedWithAlgo {
        ALGO_SCRYPT,             // Anoncoin native is this, always needs to be the default, and compatible with blocks on the chain from genesis onward...
        ALGO_SHA256D,

        MAX_ALGO_TYPES
    };

    const uint256& HashGenesisBlock() const { return hashGenesisBlock; }
    const MessageStartChars& MessageStart() const { return pchMessageStart; }
    const vector<unsigned char>& AlertKey() const { return vAlertPubKey; }
    const vector<unsigned char>& OldAlertKey() const { return vOldAlertPubKey; }
    int GetDefaultPort() const { return nDefaultPort; }
    const uint256& ProofOfWorkLimit( MinedWithAlgo mwa = ALGO_SCRYPT ) const { return bnProofOfWorkLimit[ mwa ]; }
    virtual const CBlock& GenesisBlock() const = 0;
    virtual bool RequireRPCPassword() const { return true; }
    const string& DataDir() const { return strDataDir; }
    CBaseChainParams::Network GetNetworkID() const { return networkID; }
    std::string NetworkIDString() const { return strNetworkID; }
    const vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
#ifdef ENABLE_I2PSAM
    const vector<CDNSSeedData>& i2pDNSSeeds() const { return i2pvSeeds; }
#endif
    const std::vector<unsigned char> &Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    virtual const vector<CAddress>& FixedSeeds() const = 0;
    virtual const vector<CAddress>& FixedI2PSeeds() const = 0;
protected:
    CChainParams() {}

    uint256 hashGenesisBlock;
    MessageStartChars pchMessageStart;
    // Raw pub key bytes for the broadcast alert signing key.
    vector<unsigned char> vAlertPubKey;
    vector<unsigned char> vOldAlertPubKey;
    int nDefaultPort;
    uint256 bnProofOfWorkLimit[ MAX_ALGO_TYPES ];
    string strDataDir;
    vector<CDNSSeedData> vSeeds;
#ifdef ENABLE_I2PSAM
    vector<CDNSSeedData> i2pvSeeds;
#endif
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    CBaseChainParams::Network networkID;
    std::string strNetworkID;
};

/**
 * Return the currently selected parameters. This won't change after app startup
 * outside of the unit tests.
 */
extern const CChainParams &Params();

/** Return parameters for the given network. */
extern CChainParams &Params(CBaseChainParams::Network network);

/** Sets the params returned by Params() to those for the given network. */
extern void SelectParams(CBaseChainParams::Network network);

/**
 * Looks for -regtest or -testnet and then calls SelectParams as appropriate.
 * Returns false if an invalid combination is given.
 */
extern bool SelectParamsFromCommandLine();

extern bool TestNet();
extern bool RegTest();
extern bool isMainNetwork();

#endif // header guard

