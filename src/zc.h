#ifndef ANONCOIN_ZC_H_
#define ANONCOIN_ZC_H_

#include <map>
#include <boost/foreach.hpp>
#include "sync.h"    // CCriticalSection
#include "Zerocoin.h"



// contains static Params that is initialized on first use
libzerocoin::Params* GetZerocoinParams();



typedef std::map<CBigNum, libzerocoin::PrivateCoin*> PrivateCoinMap;

class ZerocoinStoreError: public std::runtime_error
{
public:
    explicit ZerocoinStoreError(const std::string& str) : std::runtime_error(str) {}
};


// this is intended to have a similar interface to CKeyStore
class CPrivateCoinStore
{
public:
    mutable CCriticalSection cs_CoinStore;

    // TODO? NewCoin

    // add a PrivateCoin to the store
    void AddCoin(const libzerocoin::PrivateCoin& privcoin);

    // Check whether a PrivateCoin corresponding to the given PublicCoin is present in the store.
    bool HaveCoin(const libzerocoin::PublicCoin& pubcoin) const;
    bool HaveCoin(const CBigNum& bnPublicCoinValue) const;

    // Get a PrivateCoin corresponding to the given PublicCoin from the store.
    // throws ZerocoinStoreError if not found
    // IMPORTANT: the PrivateCoin referred to is owned by this CPrivateCoinStore!
    const libzerocoin::PrivateCoin& GetCoin(const libzerocoin::PublicCoin& pubcoin) const;
    const libzerocoin::PrivateCoin& GetCoin(const CBigNum& bnPublicCoinValue) const;
    // TODO? GetCoins()

    ~CPrivateCoinStore()
    {
        BOOST_FOREACH(PrivateCoinMap::value_type& item, mapCoins) {
            delete item.second;
        }
    }

private:
    PrivateCoinMap mapCoins;
};

#endif /* #ifndef ANONCOIN_ZC_H */
