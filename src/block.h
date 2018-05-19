// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_BLOCK_H
#define ANONCOIN_BLOCK_H

#ifndef HARDFORK_BLOCK3
#define HARDFORK_BLOCK3 850000
#endif

#include "Gost3411.h"
#include "pow.h"
#include "transaction.h"
#include "serialize.h"
#include "uint256.h"

#include <boost/unordered_map.hpp>

/** The maximum allowed size for a serialized block, in bytes (network rule) */
extern const uint32_t MAX_BLOCK_SIZE;

/**
 * Because Block headers for Anoncoin are made up of meaningless SHA256 double hashes, we introduce a new solution
 * to the fact block hashes != block proof-of-work
 * Legacy support of old blocks running under v10 requires this, as does every block mined today
 */
class uintFakeHash : public uint256
{
public:
    uintFakeHash() {}
    uintFakeHash(uint64_t b) : uint256(b) {}
    uintFakeHash( const uint256& hashin ) : uint256( hashin) {}
    // uintFakeHash( uint256& hashin ) : uint256( hashin) {}
    uint256 GetRealHash() const;
    void SetRealHash( const uint256& realHash );
};

struct BlockHashCorrector
{
    size_t operator()(const uint256& fakehash) const { return fakehash.GetLow64(); }
};

typedef boost::unordered_map<uintFakeHash, uint256, BlockHashCorrector> BlockHashCorrectionMap;
extern BlockHashCorrectionMap mapBlockHashCrossReference;

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
private:
    mutable bool fCalcScrypt;
    mutable bool fCalcSha256d;
    mutable uint256 therealHash;
    mutable uint256 gost3411Hash;
    mutable uintFakeHash sha256dHash;

public:
    // header
    static const int32_t CURRENT_VERSION=2;
    int32_t nVersion;
    uintFakeHash hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        //! If (when) we read this header make sure we re-calculate the hashes when they are asked for.
        if (ser_action.ForRead()) {
            fCalcScrypt = false;
            fCalcSha256d = false;
        }
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    }

    void SetNull()
    {
        nVersion = CBlockHeader::CURRENT_VERSION;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        fCalcScrypt = false;
        fCalcSha256d = false;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uintFakeHash CalcSha256dHash( const bool fForceUpdate = false ) const;
    uint256 GetHash( const bool fForceUpdate = false ) const;
    uint256 GetGost3411Hash() const;

    inline uint256 GetPoWHash(uint32_t nHeight, const bool fForceUpdate = false) const
    {
        return GetPoWHash();
    }

    inline uint256 GetPoWHash(uint32_t nHeight) const
    {
        if (nHeight < HARDFORK_BLOCK3)
            return GetHash();
        return GetGost3411Hash();
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransaction> vtx;

    // memory only
    mutable std::vector<uint256> vMerkleTree;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CBlockHeader*)this);
        READWRITE(vtx);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        vMerkleTree.clear();
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    // Build the in-memory merkle tree for this block and return the merkle root.
    // If non-NULL, *mutated is set to whether mutation was detected in the merkle
    // tree (a duplication of transactions in the block leading to an identical
    // merkle root).
    uint256 BuildMerkleTree(bool* mutated = NULL) const;

    std::vector<uint256> GetMerkleBranch(int nIndex) const;
    static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex);
    std::string ToString() const;
};


/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uintFakeHash> vHave;

    CBlockLocator() {}

    CBlockLocator(const std::vector<uintFakeHash>& vHaveIn)
    {
        vHave = vHaveIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // ANONCOIN_BLOCK_H
