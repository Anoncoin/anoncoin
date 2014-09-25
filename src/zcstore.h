#ifndef ANONCOIN_ZCSTORE_H_
#define ANONCOIN_ZCSTORE_H_

#include <map>
#include "sync.h"    // CCriticalSection
#include "Zerocoin.h"


typedef std::map<CBigNum, libzerocoin::PrivateCoin_Ptr> ZerocoinMap;


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
    libzerocoin::PrivateCoin_Ptr GetCoin() const;
    // TODO? GetCoins()

private:
    ZerocoinMap mapCoins;
};

// gets the global instance, initializing it on first call
CZerocoinStore* GetZerocoinStore();

#endif /* ifndef ANONCOIN_ZCSTORE_H_ */

