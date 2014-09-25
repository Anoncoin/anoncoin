#include "zcstore.h"

namespace zc = libzerocoin;



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

// XXX TODO


// gets the global instance, initializing it on first call
CZerocoinStore* GetZerocoinStore()
{
    static CZerocoinStore coinstore;
    return &coinstore;
}
