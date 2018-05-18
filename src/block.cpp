// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block.h"

#include "Gost3411.h"
#include "hash.h"
#include "scrypt.h"
#include "tinyformat.h"
#include "util.h"

//! Constants found in this source codes header(.h)
//! The maximum allowed size for a serialized block, in bytes (network rule)
const uint32_t MAX_BLOCK_SIZE = 1000000;

BlockHashCorrectionMap mapBlockHashCrossReference;

uint256 uintFakeHash::GetRealHash() const
{
    BlockHashCorrectionMap::iterator mi = mapBlockHashCrossReference.find(*this);
    return (mi != mapBlockHashCrossReference.end()) ? mi->second : uint256(0);
}

void uintFakeHash::SetRealHash( const uint256& realHash )
{
    BlockHashCorrectionMap::iterator mi = mapBlockHashCrossReference.insert(std::make_pair(*this, realHash)).first;
}

uintFakeHash CBlockHeader::CalcSha256dHash(const bool fForceUpdate) const
{
    if( !fCalcSha256d || fForceUpdate ) {
        uint256 tHash;
        tHash = Hash(BEGIN(nVersion), END(nNonce));
        //! We can do this in a constant class method because we declared them as mutable
        sha256dHash = tHash;
        fCalcSha256d = true;
    }
    return sha256dHash;
}

uint256 CBlockHeader::GetHash(const bool fForceUpdate) const
{
    /*if (this->nHeight > HARDFORK_BLOCK3)
    {
        return GetGost3411Hash();
    }*/
    
    if( !fCalcScrypt || fForceUpdate ) {
        uint256 tHash;
        scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(tHash));
        //! We can do this in a constant class method because we declared them as mutable
        therealHash = tHash;
        fCalcScrypt = true;
    }
    return therealHash;
}

using namespace ::i2p::crypto;

uint256 CBlockHeader::GetGost3411Hash() const
{
    static unsigned char pblank[1];
    uint8_t hash1[64];
    //GOSTR3411_2012_512 ((this->begin() == this->end() ? pblank : (unsigned char*)&this->begin()[0]), (this->end() - this->begin()) * sizeof(this->begin()[0]), hash1);
    GOSTR3411_2012_512 ((BEGIN(nVersion) == END(nNonce) ? pblank : (unsigned char*)&BEGIN(nVersion)[0]), (END(nNonce) - BEGIN(nVersion)) * sizeof(BEGIN(nVersion)[0]), hash1);
    uint32_t digest[8];
    GOSTR3411_2012_256 (hash1, 64, (uint8_t *)digest);
    // to little endian
    for (int i = 0; i < 8; i++)
        gost3411Hash.begin()[i] = ByteReverse (digest[7-i]);
    return gost3411Hash;
}

uint256 CBlock::BuildMerkleTree(bool* fMutated) const
{
    /* WARNING! If you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (CVE-2012-2459).

       The reason is that if the number of hashes in the list at a given time
       is odd, the last one is duplicated before computing the next level (which
       is unusual in Merkle trees). This results in certain sequences of
       transactions leading to the same merkle root. For example, these two
       trees:

                    A               A
                  /  \            /   \
                B     C         B       C
               / \    |        / \     / \
              D   E   F       D   E   F   F
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash A (because the hash of both
       of (F) and (F,F) is C).

       The vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. If the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. We defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root. Assuming no double-SHA256 collisions, this will detect all
       known ways of changing the transactions without affecting the merkle
       root.
    */
    vMerkleTree.clear();
    vMerkleTree.reserve(vtx.size() * 2 + 16); // Safe upper bound for the number of total nodes.
    for (std::vector<CTransaction>::const_iterator it(vtx.begin()); it != vtx.end(); ++it)
        vMerkleTree.push_back(it->GetHash());
    int j = 0;
    bool mutated = false;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            if (i2 == i + 1 && i2 + 1 == nSize && vMerkleTree[j+i] == vMerkleTree[j+i2]) {
                // Two identical hashes at the end of the list at a particular level.
                mutated = true;
            }
            vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j+i]),  END(vMerkleTree[j+i]),
                                       BEGIN(vMerkleTree[j+i2]), END(vMerkleTree[j+i2])));
        }
        j += nSize;
    }
    if (fMutated) {
        *fMutated = mutated;
    }
    return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
}

std::vector<uint256> CBlock::GetMerkleBranch(int nIndex) const
{
    if (vMerkleTree.empty())
        BuildMerkleTree();
    std::vector<uint256> vMerkleBranch;
    int j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        int i = std::min(nIndex^1, nSize-1);
        vMerkleBranch.push_back(vMerkleTree[j+i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return 0;
    for (std::vector<uint256>::const_iterator it(vMerkleBranch.begin()); it != vMerkleBranch.end(); ++it)
    {
        if (nIndex & 1)
            hash = Hash(BEGIN(*it), END(*it), BEGIN(hash), END(hash));
        else
            hash = Hash(BEGIN(hash), END(hash), BEGIN(*it), END(*it));
        nIndex >>= 1;
    }
    return hash;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s gost3411=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        GetGost3411Hash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].ToString() << "\n";
    }
    s << "  vMerkleTree: ";
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        s << " " << vMerkleTree[i].ToString();
    s << "\n";
    return s.str();
}
