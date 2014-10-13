// Copyright (c) 2014 The Anoncoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zc.h"

using libzerocoin::Params;
using libzerocoin::PublicCoin;
using libzerocoin::PrivateCoin;


// ensure that Zerocoin parameters are initialized exactly once, and before first use.
Params* GetZerocoinParams()
{
    static Params params;
    return &params;
}


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
