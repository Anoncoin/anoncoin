// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

#include "amount.h"
#include "checkpoints.h"
#include "pow.h"
#include "uint256.h"

#include <stdint.h>

#include <boost/scoped_ptr.hpp>

//! Constants found in this source codes header(.h)
//! -dbcache default (MiB)
const int64_t nDefaultDbCache = 400;
//! max. -dbcache in (MiB)
const int64_t nMaxDbCache = sizeof(void*) > 4 ? 4096 : 1024;
//! min. -dbcache in (MiB)
const int64_t nMinDbCache = 4;

using namespace std;

void static BatchWriteCoins(CLevelDBBatch &batch, const uint256 &hash, const CCoins &coins) {
    if (coins.IsPruned()) {
        //LogPrintf( "CCoinsViewDB::BatchWriteCoins() erased hash %s it has been Pruned.\n", hash.ToString() );
        batch.Erase(make_pair('c', hash));
    }
    else
        batch.Write(make_pair('c', hash), coins);
}

void static BatchWriteHashBestChain(CLevelDBBatch &batch, const uint256 &hash)
{
    // LogPrintf( "CCoinsViewDB::BatchWriteHashBestChain() hash %s\n", hash.ToString() );
    batch.Write('B', hash);
}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe) {
}

bool CCoinsViewDB::GetCoins(const uint256 &txid, CCoins &coins) const {
    bool fResult = db.Read(make_pair('c', txid), coins);
    //LogPrintf( "CCoinsViewDB::GetCoins() for %s found on disk=%d\n", txid.ToString(), fResult );
    return fResult;
}

bool CCoinsViewDB::HaveCoins(const uint256 &txid) const {
    bool fResult = db.Exists(make_pair('c', txid));
    //LogPrintf( "CCoinsViewDB::HaveCoins() for %s found on disk=%d\n", txid.ToString(), fResult );
    return fResult;
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    bool fResult = db.Read('B', hashBestChain);
    //LogPrintf( "CCoinsViewDB::GetBestBlock() read=%d found %s\n", fResult, hashBestChain.ToString() );
    if (!fResult)
        return uint256(0);
    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CLevelDBBatch batch;
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            BatchWriteCoins(batch, it->first, it->second.coins);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (hashBlock != uint256(0))
        BatchWriteHashBestChain(batch, hashBlock);
    //else
        //LogPrintf( "CCoinsViewDB::BatchWrite() WARNING - No hashBlock set to write BestChain hash.\n" );

    // LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    LogPrint("coindb", "Committing %u changed transactions (out of %u) to coin database, best hashblock=%s\n", (unsigned int)changed, (unsigned int)count, hashBlock.ToString());
    return db.WriteBatch(batch);
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe) {
}

bool CBlockTreeDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    // LogPrintf( "Writing blockindex hash: %s\n", blockindex.GetBlockHash().ToString());
    return Write(make_pair('b', blockindex.GetBlockHash()), blockindex);
}

bool CBlockTreeDB::WriteBlockFileInfo(int nFile, const CBlockFileInfo &info) {
    return Write(make_pair('f', nFile), info);
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(make_pair('f', nFile), info);
}

bool CBlockTreeDB::WriteLastBlockFile(int nFile) {
    return Write('l', nFile);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write('R', '1');
    else
        return Erase('R');
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists('R');
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read('l', nFile);
}

bool CCoinsViewDB::GetStats(CCoinsStats &stats) const {
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    boost::scoped_ptr<leveldb::Iterator> pcursor(const_cast<CLevelDBWrapper*>(&db)->NewIterator());
    pcursor->SeekToFirst();

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = GetBestBlock();
    ss << stats.hashBlock;
    int64_t nTotalAmount = 0;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'c') {
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CCoins coins;
                ssValue >> coins;
                uint256 txhash;
                ssKey >> txhash;
                ss << txhash;
                ss << VARINT(coins.nVersion);
                ss << (coins.fCoinBase ? 'c' : 'n');
                ss << VARINT(coins.nHeight);
                stats.nTransactions++;
                for (unsigned int i=0; i<coins.vout.size(); i++) {
                    const CTxOut &out = coins.vout[i];
                    if (!out.IsNull()) {
                        stats.nTransactionOutputs++;
                        ss << VARINT(i+1);
                        ss << out;
                        nTotalAmount += out.nValue;
                    }
                }
                stats.nSerializedSize += 32 + slValue.size();
                ss << VARINT(0);
            }
            pcursor->Next();
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    stats.nHeight = mapBlockIndex.find(GetBestBlock())->second->nHeight;
    stats.hashSerialized = ss.GetHash();
    stats.nTotalAmount = nTotalAmount;
    return true;
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(make_pair('t', txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CLevelDBBatch batch;
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(make_pair('t', it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair('F', name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair('F', name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts( vector<BlockTreeEntry>& vSortedByHeight )
{
    boost::scoped_ptr<leveldb::Iterator> pcursor(NewIterator());

    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet << make_pair('b', uint256(0));
    pcursor->Seek(ssKeySet.str());

    BlockTreeEntry aBlockDetails;
    //! Load mapBlockIndex <-- Not yet, but a good start. Anoncoin works differently then other coins, all we do here is a fast load of what is on disk
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'b') {
                ssKey >> aBlockDetails.uintRealHash;
                leveldb::Slice slValue = pcursor->value();
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CDiskBlockIndex diskindex;
                ssValue >> diskindex;
                aBlockDetails.nHeight = diskindex.nHeight;

                //! NOTE: On constructing block index objects.  Computing many hash values here can lead to many minutes of waiting for
                //! the user.  This has been re-written several times in order to be as fast as possible for loading.  We don't
                //! re-compute hashes here any longer, the fake sha256d ones that are in the mined blocks for the previous block,
                //! or the real scrypt hashes Anoncoin has used for years as POW.  Just create the CBlockIndex objects and add them to
                //! a vector of CBlockIndex objects, we will later sort, calculate hashes, organize it and build the mapBlockIndex lookup
                //! system used by the rest of the software....

                //! Create a new empty CBlockIndex each time we read one
                CBlockIndex* pindexNew = new CBlockIndex();
                if (!pindexNew)
                    throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");

                aBlockDetails.pBlockIndex = pindexNew;
                //! For speed we'll initially just save the previous block hash (sha256d) in the fakeBIhash field of the new object, this
                //! is NORMALLY used to hold the sha256d hash of THIS block, but we don't have that value computed yet, and this works fine
                //! as a temporary holding location, until later when we do the calculations.  See LoadBlockIndexDB() in main.cpp where all
                //! the time complex work is done.
                pindexNew->fakeBIhash     = diskindex.hashPrev;
                //!pindexNew->pprev        We can not set this up yet, without allot of time complexity
                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->nBits          = diskindex.nBits;
                pindexNew->nNonce         = diskindex.nNonce;
                pindexNew->nStatus        = diskindex.nStatus;
                pindexNew->nTx            = diskindex.nTx;
                //! Just store it in the vector for later sorting & processing.
                vSortedByHeight.push_back(aBlockDetails);
                //! No pow checks here, just go to the next blockindex
                pcursor->Next();
            } else {
                break; // if shutdown requested or finished loading block index
            }
        } catch (std::exception &e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    //LogPrintf("%s : The Data Position of the last blockindex entry is %d\n", __func__, vSortedByHeight[vSortedByHeight.size() - 1].second->nDataPos);

    return true;
}
