// Copyright (c) 2014 The Anoncoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zc.h"

using libzerocoin::Accumulator;
using libzerocoin::AccumulatorWitness;
using libzerocoin::Params;
using libzerocoin::PublicCoin;
using libzerocoin::PrivateCoin;
using libzerocoin::CoinDenomination;


// ensure that Zerocoin parameters are initialized exactly once, and before first use.
Params* GetZerocoinParams()
{
    static Params params;
    return &params;
}


// GNOSIS TODO: allow persistent storage to a Berkeley DB file

// add a copy of the given PrivateCoin to the store
void CWalletCoinStore::AddCoin(const PrivateCoin& privcoin) {
    PrivateCoin* pprivcoin = new PrivateCoin(privcoin);
    CBigNum bnPublicCoinValue(pprivcoin->getPublicCoin().getValue());
    {
        LOCK(cs_CoinStore);
        if (mapCoins.count(bnPublicCoinValue) == 0)
            mapCoins[bnPublicCoinValue] = pprivcoin;
    }
}


// check if a PrivateCoin is in the store
bool CWalletCoinStore::HaveCoin(const CBigNum& bnPublicCoinValue) const
{
    bool result;
    {
        LOCK(cs_CoinStore);
        result = (mapCoins.count(bnPublicCoinValue) > 0);
    }
    return result;
}

bool CWalletCoinStore::HaveCoin(const PublicCoin& pubcoin) const
{
    return this->HaveCoin(pubcoin.getValue());
}



// get the PrivateCoin from the store
// throws ZerocoinStoreError if not found
const PrivateCoin& CWalletCoinStore::GetCoin(const CBigNum& bnPublicCoinValue) const
{
    {
        LOCK(cs_CoinStore);
        PrivateCoinMap::const_iterator mi = mapCoins.find(bnPublicCoinValue);
        if (mi != mapCoins.end()) {
            return *mi->second;
        }
    }
    throw ZerocoinStoreError("PrivateCoin not found in this CWalletCoinStore");
}

const PrivateCoin& CWalletCoinStore::GetCoin(const PublicCoin& pubcoin) const
{
    return this->GetCoin(pubcoin.getValue());
}




// contains a PrivateCoin along with metadata. Stored in zerocoin_wallet.dat


// these change the status by updating what is known
// throw runtime_error if the values are being supplied out of order
void CWalletCoin::SetMintOutputAndDenomination(COutPoint outputMint, CoinDenomination denom)
{
    if (nStatus > ZCWST_MINTED_NOT_IN_BLOCK)
        throw std::runtime_error("cannot set CWalletCoin mint output or denomination once mint txn in block"); // or can we (txn malleability...)?
    nStatus = ZCWST_MINTED_NOT_IN_BLOCK;
    this->outputMint_hash = outputMint.hash;
    this->outputMint_vout = outputMint.n;
    coin.setDenomination(denom);
}


// hashBlock should be set to 0 if the new fork does not contain our ZC mint txn
void CWalletCoin::SetMintedBlock(uint256 hashBlock)
{
    if (nStatus < ZCWST_MINTED_NOT_IN_BLOCK)
        throw std::runtime_error("cannot set CWalletCoin mint block hash before a mint txn is specified");
    if (nStatus > ZCWST_MINTED_IN_BLOCK)
    {
        // big problem! we have a deep fork!
        // we assumed that our ZC mint txn was deep enough to not be reverted by a fork, and we created a spend
        // to fix this, we need to undo all of our spend-related behavior
        throw std::runtime_error("OMGWTF"); // GNOSIS TODO
        inputSpend_hash = 0;  // CInPoint::SetNull()
        inputSpend_vin = -1;
        hashSpendBlock = 0;
    }
    nStatus = ZCWST_MINTED_IN_BLOCK;
    hashMintBlock = hashBlock;

    std::abort();  // the following is a work in progress
    /*
    //XXX GNOSIS TODO: WRONG!!! we may be able to salvage some mapWitnesses
    mapWitnesses.clear();
    const Accumulator& chkptPrevBlock = GetParentBlockCheckpoint(hashBlock, coin.getDenomination());
    AccumulatorWitness w(GetZerocoinParams(), chkptPrevBlock, pubcoin);
    mapWitnesses[hashBlock] = w;
    */
}

void CWalletCoin::SetSpendInput(CInPoint inputSpend)
{
    if (nStatus < ZCWST_MINTED_IN_BLOCK)
        throw std::runtime_error("cannot set CWalletCoin spend input until coin has been minted");
    if (nStatus >= ZCWST_SPENT_IN_BLOCK)
    {
        printf("WARNING: SetSpendInput has already been called on this CWalletCoin\n");
        hashSpendBlock = 0;
    }
    nStatus = ZCWST_SPENT_NOT_IN_BLOCK;
    this->inputSpend_hash = inputSpend.ptx->GetHash();
    this->inputSpend_vin = inputSpend.n;
}

// hashBlock should be set to 0 if the new fork does not contain our ZC spend txn
void CWalletCoin::SetSpentBlock(uint256 hashBlock)
{
    if (nStatus < ZCWST_SPENT_NOT_IN_BLOCK)
        throw std::runtime_error("cannot set CWalletCoin spent block hash before a spend txn was specified");
    if (nStatus == ZCWST_SPENT_IN_BLOCK)
        printf("WARNING: SetSpentBlock has already been called on this CWalletCoin\n");
    nStatus = ZCWST_SPENT_IN_BLOCK;
    hashSpendBlock = hashBlock;
}


//GetParentBlockCheckpoint // GNOSIS TODO
