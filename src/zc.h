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



typedef std::map<uint256, CWalletCoin*> WalletCoinMap;

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

    // add a CWalletCoin to the store
    void AddCoin(CWalletCoin* pwzc);

    // Check whether a CWalletCoin corresponding to the given PublicCoin hash is present in the store.
    bool HaveCoin(uint256 hashPubCoin) const;

    // Get a CWalletCoin corresponding to the given PublicCoin hash from the store.
    // throws ZerocoinStoreError if not found
    // IMPORTANT: the CWalletCoin referred to is owned by this CWalletCoinStore!
    CWalletCoin& GetCoin(uint256 hashPubCoin);
    // TODO? GetCoins()

    ~CWalletCoinStore();

private:
    WalletCoinMap mapCoins;
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
    libzerocoin::CoinDenomination GetDenomination() const
    {
        if (nStatus < ZCWST_MINTED_NOT_IN_BLOCK)
            throw std::runtime_error("cannot get denomination from CWalletCoin not yet minted");
        return coin.getDenomination();
    }

    uint256 GetPublicCoinHash() const
    {
        uint256 hashPubCoin;
        Bignum bnPubVal = coin.getPublicCoin().getValue();
        CHashWriter h(SER_GETHASH, 0);
        h << bnPubVal;
        hashPubCoin = h.GetHash();
        return hashPubCoin;
    }

    uint256 GetMintOutputHash() const
    {
        if (nStatus < ZCWST_MINTED_NOT_IN_BLOCK)
            throw std::runtime_error("cannot get mint txn hash from CWalletCoin not yet minted");
        return outputMint_hash;
    }

    int GetMintOutputVout() const
    {
        if (nStatus < ZCWST_MINTED_NOT_IN_BLOCK)
            throw std::runtime_error("cannot get mint txn vout from CWalletCoin not yet minted");
        return outputMint_vout;
    }

    uint256 GetMintedBlock() const
    {
        if (nStatus < ZCWST_MINTED_IN_BLOCK)
            throw std::runtime_error("CWalletCoin: cannot get block hash containing mint txn because not in block yet");
        return hashMintBlock;
    }

    uint256 GetSpendInputHash() const
    {
        if (nStatus < ZCWST_SPENT_NOT_IN_BLOCK)
            throw std::runtime_error("CWalletCoin: cannot get spend input hash before spending");
        return inputSpend_hash;
    }

    int GetSpendInputVin() const
    {
        if (nStatus < ZCWST_SPENT_NOT_IN_BLOCK)
            throw std::runtime_error("CWalletCoin: cannot get spend input vin before spending");
        return inputSpend_vin;
    }

    uint256 GetSpentBlock() const
    {
        if (nStatus < ZCWST_SPENT_IN_BLOCK)
            throw std::runtime_error("CWalletCoin: cannot get hash of block containing spend tx until tx gets in a block");
        return hashSpendBlock;
    }


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
        // nStatus
        if (fRead)
        {
            // deserializing
            int _nStatus;
            READWRITE(_nStatus);
            me.nStatus = static_cast<CWalletCoin::Status>(_nStatus);
        }
        else
        {
            // serializing or getting size
            int _nStatus = static_cast<int>(nStatus);
            READWRITE(_nStatus);
        }
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
            // GNOSIS TODO: set params for each!
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
