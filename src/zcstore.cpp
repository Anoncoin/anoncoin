#include "zcstore.h"

namespace zc = libzerocoin;


// GNOSIS TODO: allow persistent storage to a Berkeley DB file

// add a PrivateCoin to the store
void CZerocoinStore::AddCoin(zc::PrivateCoin_Ptr pprivcoin) {
    CBigNum bnPublicCoinValue(pprivcoin->getPublicCoin()->getValue());
    mapCoins[bnPublicCoinValue] = pprivcoin;
}


// check if a PrivateCoin is in the store
bool CZerocoinStore::HaveCoin(const CBigNum& bnPublicCoinValue) const
{
    bool result;
    {
        LOCK(cs_CoinStore);
        result = (mapCoins.count(bnPublicCoinValue) > 0);
    }
    return result;
}

bool CZerocoinStore::HaveCoin(const zc::PublicCoin& pubcoin) const
{
    return this->HaveCoin(pubcoin.getValue());
}



// get the PrivateCoin from the store
// throws ZerocoinStoreError if not found
zc::PrivateCoin_Ptr CZerocoinStore::GetCoin(const CBigNum& bnPublicCoinValue) const
{
    {
        LOCK(cs_CoinStore);
        ZerocoinMap::const_iterator mi = mapCoins.find(bnPublicCoinValue);
        if (mi != mapCoins.end()) {
            return mi->second;
        }
    }
    throw ZerocoinStoreError("PrivateCoin not found in this CZerocoinStore");
}

zc::PrivateCoin_Ptr CZerocoinStore::GetCoin(const zc::PublicCoin& pubcoin) const
{
    return this->GetCoin(pubcoin.getValue());
}



// gets the global instance, initializing it on first call
CZerocoinStore* GetZerocoinStore()
{
    static CZerocoinStore coinstore;
    return &coinstore;
}
