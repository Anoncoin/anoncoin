#include "zcstore.h"

using libzerocoin::PublicCoin;
using libzerocoin::PrivateCoin;


// GNOSIS TODO: allow persistent storage to a Berkeley DB file

// add a copy of the given PrivateCoin to the store
void CPrivateCoinStore::AddCoin(const PrivateCoin& privcoin) {
    PrivateCoin* pprivcoin = new PrivateCoin(privcoin);
    CBigNum bnPublicCoinValue(pprivcoin->getPublicCoin().getValue());
    {
        LOCK(cs_CoinStore);
        if (mapCoins.count(bnPublicCoinValue) == 0)
            mapCoins[bnPublicCoinValue] = pprivcoin;
    }
}


// check if a PrivateCoin is in the store
bool CPrivateCoinStore::HaveCoin(const CBigNum& bnPublicCoinValue) const
{
    bool result;
    {
        LOCK(cs_CoinStore);
        result = (mapCoins.count(bnPublicCoinValue) > 0);
    }
    return result;
}

bool CPrivateCoinStore::HaveCoin(const PublicCoin& pubcoin) const
{
    return this->HaveCoin(pubcoin.getValue());
}



// get the PrivateCoin from the store
// throws ZerocoinStoreError if not found
const PrivateCoin& CPrivateCoinStore::GetCoin(const CBigNum& bnPublicCoinValue) const
{
    {
        LOCK(cs_CoinStore);
        PrivateCoinMap::const_iterator mi = mapCoins.find(bnPublicCoinValue);
        if (mi != mapCoins.end()) {
            return *mi->second;
        }
    }
    throw ZerocoinStoreError("PrivateCoin not found in this CPrivateCoinStore");
}

const PrivateCoin& CPrivateCoinStore::GetCoin(const PublicCoin& pubcoin) const
{
    return this->GetCoin(pubcoin.getValue());
}
