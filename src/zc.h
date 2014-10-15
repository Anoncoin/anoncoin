#ifndef ANONCOIN_ZC_H_
#define ANONCOIN_ZC_H_

#include <map>
#include <boost/foreach.hpp>
#include "main.h"    // COutPoint
#include "sync.h"    // CCriticalSection
#include "serialize.h"
#include "Zerocoin.h"


class CWalletCoin;
class CInPoint;


// contains static Params that is initialized on first use
libzerocoin::Params* GetZerocoinParams();



typedef std::map<CBigNum, libzerocoin::PrivateCoin*> PrivateCoinMap;

class ZerocoinStoreError: public std::runtime_error
{
public:
    explicit ZerocoinStoreError(const std::string& str) : std::runtime_error(str) {}
};


// this is intended to have a similar interface to CKeyStore
class CWalletCoinStore
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
    // IMPORTANT: the PrivateCoin referred to is owned by this CWalletCoinStore!
    const libzerocoin::PrivateCoin& GetCoin(const libzerocoin::PublicCoin& pubcoin) const;
    const libzerocoin::PrivateCoin& GetCoin(const CBigNum& bnPublicCoinValue) const;
    // TODO? GetCoins()

    ~CWalletCoinStore()
    {
        BOOST_FOREACH(PrivateCoinMap::value_type& item, mapCoins) {
            delete item.second;
        }
    }

private:
    PrivateCoinMap mapCoins;
};

// contains a PrivateCoin along with metadata. Stored in zerocoin_wallet.dat
class CWalletCoin
{
public:
    enum Status
    {
        // initial state - the PrivateCoin was generated, but it has no denomination, nor other metadata
        // have: private coin (no denomination)
        ZCWST_NOT_MINTED = 0,

        // this PrivateCoin has been put into a ZC mint transaction, but not yet in block
        // it may or may not have been broadcast
        // have: private coin (with denom.), mint COutPoint
        ZCWST_MINTED_NOT_IN_BLOCK,

        // ZC mint txn is in a block
        // have: private coin, mint COutPoint, mint block hash, witness map
        ZCWST_MINTED_IN_BLOCK,

        // coin has been spent, but the spend is not yet in a block
        // have private coin, mint COutPoint, mint block hash, witness map, spend CInPoint
        ZCWST_SPENT_NOT_IN_BLOCK,

        // final state - coin spend is in a block
        // GNOSIS TODO: but what about another fork putting spend txn back in mempool?
        // have private coin, mint COutPoint, mint block hash, witness map, spend CInPoint, spend block hash
        ZCWST_SPENT_IN_BLOCK
    };

    template<typename Stream>
    CWalletCoin(Stream& strm)
    {
        strm >> *this;
    }

    // generates a new PrivateCoin (CPU intensive!)
    CWalletCoin()
        : nStatus(ZCWST_NOT_MINTED),
          coin(GetZerocoinParams()),
          outputMint_hash(0),     // COutPoint::SetNull()
          outputMint_vout(-1),
          hashMintBlock(0),
          inputSpend_hash(0),     // CInPoint::SetNull()
          inputSpend_vin(-1),
          hashSpendBlock(0)
    {
        mapWitnesses.clear();
    }

    // getters
    const libzerocoin::PrivateCoin& GetPrivateCoin() const { return coin; }
    //XXX

    // status
    Status GetStatus() const { return nStatus; }

    // these change the status by updating what is known
    // throw runtime_error if the values are being supplied out of order
    void SetMintOutputAndDenomination(COutPoint outputMint, libzerocoin::CoinDenomination denom);

    void SetMintedBlock(uint256 hashBlock);

    void SetSpendInput(CInPoint inputSpend);

    void SetSpentBlock(uint256 hashBlock);

    std::string ToString() const
    {
        return strprintf("CWalletCoin(status=%d, XXX)", static_cast<int>(nStatus)); //XXX GNOSIS TODO
    }

    IMPLEMENT_SERIALIZE
    (
        CWalletCoin& me = *const_cast<CWalletCoin*>(this);

        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nStatus); // GNOSIS TODO: fix warning about ambiguity
        READWRITE(coin);
        // as the status increases to later stages, more is known and so more is serialized here
        if (nStatus >= ZCWST_MINTED_NOT_IN_BLOCK)
        {
            // COutPoint
            READWRITE(outputMint_hash);
            READWRITE(outputMint_vout);
        }
        if (nStatus >= ZCWST_MINTED_IN_BLOCK)
        {
            READWRITE(hashMintBlock);
            READWRITE(mapWitnesses);
        }
        if (nStatus >= ZCWST_SPENT_NOT_IN_BLOCK)
        {
            // CInPoint
            READWRITE(inputSpend_hash);
            READWRITE(inputSpend_vin);
        }
        if (nStatus >= ZCWST_SPENT_IN_BLOCK)
            READWRITE(hashSpendBlock);
    )

private:
    Status nStatus;                 // describes what state the coin is in and what is present
    libzerocoin::PrivateCoin coin;  // the coin itself (serial, randomness, public coin, and optional denom.)
    uint256 outputMint_hash;        // outputMint - points to transaction and vout of mint, if not null
    unsigned int outputMint_vout;
    uint256 hashMintBlock;          // points to mint block, if not null
    std::map<uint256,libzerocoin::AccumulatorWitness> mapWitnesses;
    uint256 inputSpend_hash;            // inputSpend - points to transaction and vin of spend, if not null
    unsigned int inputSpend_vin;
    uint256 hashSpendBlock;         // points to spend block, if not null


};

//GetParentBlockCheckpoint

#endif /* #ifndef ANONCOIN_ZC_H */
