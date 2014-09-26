#ifndef ANONCOIN_ZCSTORE_H_
#define ANONCOIN_ZCSTORE_H_

#include <map>
#include "sync.h"    // CCriticalSection
#include "Zerocoin.h"


typedef std::map<CBigNum, libzerocoin::PrivateCoin_Ptr> ZerocoinMap;

class ZerocoinStoreError: public std::runtime_error
{
public:
    explicit ZerocoinStoreError(const std::string& str) : std::runtime_error(str) {}
};


// this is intended to have a similar interface to CKeyStore
class CZerocoinStore
{
protected:
    mutable CCriticalSection cs_CoinStore;

public:
    // add a PrivateCoin to the store
    void AddCoin(libzerocoin::PrivateCoin_Ptr pprivcoin);

    // Check whether a PrivateCoin corresponding to the given PublicCoin is present in the store.
    bool HaveCoin(const libzerocoin::PublicCoin& pubcoin) const;
    bool HaveCoin(const CBigNum& bnPublicCoinValue) const;

    // Get a PrivateCoin corresponding to the given PublicCoin from the store.
    // throws ZerocoinStoreError if not found
    libzerocoin::PrivateCoin_Ptr GetCoin(const libzerocoin::PublicCoin& pubcoin) const;
    libzerocoin::PrivateCoin_Ptr GetCoin(const CBigNum& bnPublicCoinValue) const;
    // TODO? GetCoins()

private:
    ZerocoinMap mapCoins;
};

// gets the global instance, initializing it on first call
CZerocoinStore* GetZerocoinStore();

#endif /* ifndef ANONCOIN_ZCSTORE_H_ */

