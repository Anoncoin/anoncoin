// Copyright (c) 2012 Pieter Wuille
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ANONCOIN_ADDRMAN_H
#define ANONCOIN_ADDRMAN_H

//! This allows the extensions required to be compiled for the Anoncoin AddrManager b32.i2p destination lookup functionality
#define I2PADDRMAN_EXTENSIONS

// Many builder specific things set in the config file,
// Included here as we need ENABLE_I2PSAM
// Version 0.9.5.1 makes that requirement obsolete, I2PSAM has nothing to do with managing b32.i2p destination
// lookups, however leaving in the include, as it does no harm, and could be useful at a later date.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "chainparams.h"
#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "serialize.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"

#include <map>
#include <set>
#include <stdint.h>
#include <vector>

#include <openssl/rand.h>

/** Extended statistics about our B32.I2P address table */
class CDestinationStats
{
public:
    std::string sAddress;       // CNetAddr member
    std::string sBase64;        // CNetAddr member
    unsigned short uPort;       // CService member
    int64_t nLastTry;           // CAddress member
    uint64_t nServices;         // CAddress member
    bool fInTried;              // CAddrInfo member, Developer preference here to say <this means its good>
    int nAttempts;              // CAddrInfo member
    int64_t nSuccessTime;       // CAddrInfo member
    std::string sSource;        // CAddrInfo member, from CNetAddr of source
};

/**
 * Extended statistics about a CAddress
 */
class CAddrInfo : public CAddress
{
public:
    //! last try whatsoever by us (memory only)
    int64_t nLastTry;

private:
    //! where knowledge about this address first came from
    CNetAddr source;

    //! last successful connection by us
    int64_t nLastSuccess;

    //! connection attempts since last successful attempt
    int nAttempts;

    //! reference count in new sets (memory only)
    int nRefCount;

    //! in tried set? (memory only)
    bool fInTried;

    //! position in vRandom
    int nRandomPos;

    friend class CAddrMan;

public:

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CAddress*)this);
        READWRITE(source);
        READWRITE(nLastSuccess);
        READWRITE(nAttempts);
    }

    void Init()
    {
        nLastSuccess = 0;
        nLastTry = 0;
        nAttempts = 0;
        nRefCount = 0;
        fInTried = false;
        nRandomPos = -1;
    }

    CAddrInfo(const CAddress &addrIn, const CNetAddr &addrSource) : CAddress(addrIn), source(addrSource)
    {
        Init();
    }

    CAddrInfo() : CAddress(), source()
    {
        Init();
    }

    //! Calculate in which "tried" bucket this entry belongs
    int GetTriedBucket(const uint256 &nKey) const;

    //! Calculate in which "new" bucket this entry belongs, given a certain source
    int GetNewBucket(const uint256 &nKey, const CNetAddr& src) const;

    //! Calculate in which "new" bucket this entry belongs, using its default source
    int GetNewBucket(const uint256 &nKey) const
    {
        return GetNewBucket(nKey, source);
    }

    //! Calculate in which position of a bucket to store this entry.
    int GetBucketPosition(const uint256 &nKey, bool fNew, int nBucket) const;

    //! Determine whether the statistics about this entry are bad enough so that it can just be deleted
    bool IsTerrible(int64_t nNow = GetAdjustedTime()) const;

    //! Calculate the relative chance this entry should be given when selecting nodes to connect to
    double GetChance(int64_t nNow = GetAdjustedTime()) const;

};

/** Stochastic address manager
 *
 * Design goals:
 *  * Only keep a limited number of addresses around, so that addr.dat and memory requirements do not grow without bound.
 *  * Keep the address tables in-memory, and asynchronously dump the entire to able in peer.dat.
 *  * Make sure no (localized) attacker can fill the entire table with his nodes/addresses.
 *
 * To that end:
 *  * Addresses are organized into buckets.
 *    * Address that have not yet been tried go into 1024 "new" buckets.
 *      * Based on the address range (/16 for IPv4) of source of the information, 64 buckets are selected at random
 *      * The actual bucket is chosen from one of these, based on the range the address itself is located.
 *      * One single address can occur in up to 8 different buckets, to increase selection chances for addresses that
 *        are seen frequently. The chance for increasing this multiplicity decreases exponentially.
 *      * When adding a new address to a full bucket, a randomly chosen entry (with a bias favoring less recently seen
 *        ones) is removed from it first.
 *    * Addresses of nodes that are known to be accessible go into 256 "tried" buckets.
 *      * Each address range selects at random 8 of these buckets.
 *      * The actual bucket is chosen from one of these, based on the full address.
 *      * When adding a new good address to a full bucket, a randomly chosen entry (with a bias favoring less recently
 *        tried ones) is evicted from it, back to the "new" buckets.
 *    * Bucket selection is based on cryptographic hashing, using a randomly-generated 256-bit key, which should not
 *      be observable by adversaries.
 *    * Several indexes are kept for high performance. Defining DEBUG_ADDRMAN will introduce frequent (and expensive)
 *      consistency checks for the entire data structure.
 */
// #define DEBUG_ADDRMAN

//! total number of buckets for tried addresses
#define ADDRMAN_TRIED_BUCKET_COUNT 256

//! total number of buckets for new addresses
#define ADDRMAN_NEW_BUCKET_COUNT 1024

//! maximum allowed number of entries in buckets for new and tried addresses
#define ADDRMAN_BUCKET_SIZE 64

//! over how many buckets entries with tried addresses from a single group (/16 for IPv4) are spread
#define ADDRMAN_TRIED_BUCKETS_PER_GROUP 8

//! over how many buckets entries with new addresses originating from a single group are spread
#define ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP 64

//! in how many buckets for entries with new addresses a single address may occur
#define ADDRMAN_NEW_BUCKETS_PER_ADDRESS 8

//! how old addresses can maximally be
// CSlave_changed to 10 since enabled the sharing by default of dynamic I2P addresses
// #define ADDRMAN_HORIZON_DAYS 30
#define ADDRMAN_HORIZON_DAYS 10

//! after how many failed attempts we give up on a new node
#define ADDRMAN_RETRIES 3


//! how many successive failures are allowed ...
// CSlave_changed
// #define ADDRMAN_MAX_FAILURES 10
#define ADDRMAN_MAX_FAILURES 7

//! ... in at least this many days
#define ADDRMAN_MIN_FAIL_DAYS 7

//! the maximum percentage of nodes to return in a getaddr call
// Until we get so many users that we have tens of thousands of addresses on
// both i2p and ip, we do not want to limit the percentage of addresses we
// send because of how many we have in our peers.dat file
// #define ADDRMAN_GETADDR_MAX_PCT 23
// #define ADDRMAN_GETADDR_MAX_PCT 100
// CSlave_changed
#define ADDRMAN_GETADDR_MAX_PCT 70

//! the maximum number of nodes to return in a getaddr call
// was #define ADDRMAN_GETADDR_MAX 2500
// Anoncoin differs greatly here because of its I2P support, addresses are >33x larger
// than cryptocoins which only support ip addresses.  Further adding to the complexity
// is the fact some old peers can not accept i2p addresses on clearnet, and nodes running
// over i2p really don't care or want to bother sharing ip addresses.
//
// Reducing the number of addresses returned is not a simple matter of reducing this value
// however.  Although it will set a limit as to how many are sent in one request:
// We've changed the mruset setKnownAddrs in the node creation (see net.h) to 1250, from 5000
// that 5K is double what was set here as max (2500).  If you look at the way the GetAddr picks
// random addresses, even on this developers system, with 14K+ addrs in peer.dat it typically only
// sent less than about 800.  After study of the code, the conclusion is most are terrible,
// the routine is likely spinning through all 14K and rejecting most of them as not being worth
// sending, eventually hitting n==vRandom.size() and ending the search.
// Lowering this limit will not likely cause any change in the current behavior, until many new
// peers show up that are good ones.  Setting a new max, based on that same ratio change with
// the node creation process, would mean going to 625 addresses here....  Also note, that in main.cpp
// where addresses are spooled out in sendtrickle, the size of each message created, before pushing
// the addrs, has been reduced.
#define ADDRMAN_GETADDR_MAX 625

/**
 * Stochastical (IP) address manager
 */
class CAddrMan
{
private:
    //! critical section to protect the inner data structures
    mutable CCriticalSection cs;

    //! secret key to randomize bucket select with
    uint256 nKey;

    //! last used nId
    int nIdCount;

    //! table with information about all nIds
    std::map<int, CAddrInfo> mapInfo;

    //! find an nId based on its network address
    std::map<CNetAddr, int> mapAddr;

    //! randomly-ordered vector of all nIds
    std::vector<int> vRandom;

    //! number of "tried" entries
    int nTried;

    //! list of "tried" buckets
    int vvTried[ADDRMAN_TRIED_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

    //! number of (unique) "new" entries
    int nNew;

    //! list of "new" buckets
    int vvNew[ADDRMAN_NEW_BUCKET_COUNT][ADDRMAN_BUCKET_SIZE];

#ifdef I2PADDRMAN_EXTENSIONS
    //! table of i2p destination address hashes, and the id assigned to them
    // ToDO: Convert to unordered_map, so we can have immediate lookup, based on hash
    // typedef boost::unordered_map<uint256, int, I2pHasher> I2pMap;
    std::map<uint256, int> mapI2pHashes;
#endif

protected:

    //! Find an entry.
    CAddrInfo* Find(const CNetAddr& addr, int *pnId = NULL);

    //! find an entry, creating it if necessary.
    //! nTime and nServices of the found node are updated, if necessary.
    CAddrInfo* Create(const CAddress &addr, const CNetAddr &addrSource, int *pnId = NULL);

    //! Swap two elements in vRandom.
    void SwapRandom(unsigned int nRandomPos1, unsigned int nRandomPos2);

     //! Move an entry from the "new" table(s) to the "tried" table
    void MakeTried(CAddrInfo& info, int nId);

    //! Delete an entry. It must not be in tried, and have refcount 0.
    void Delete(int nId);

    //! Clear a position in a "new" table. This is the only place where entries are actually deleted.
    void ClearNew(int nUBucket, int nUBucketPos);

    //! Mark an entry "good", possibly moving it from "new" to "tried".
    void Good_(const CAddress &addr, int64_t nTime);

    //! Add an entry to the "new" table.
    bool Add_(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty);

    //! Mark an entry as attempted to connect.
    void Attempt_(const CService &addr, int64_t nTime);

    //! Select an address to connect to.
    //! nUnkBias determines how much to favor new addresses over tried ones (min=0, max=100)
    CAddrInfo Select_();

#ifdef DEBUG_ADDRMAN
    //! Perform consistency check. Returns an error code or zero.
    int Check_();
#endif

    //! Select several addresses at once.
#ifdef I2PADDRMAN_EXTENSIONS
    void GetAddr_(std::vector<CAddress> &vAddr, const bool fIpOnly, const bool fI2pOnly);
#else
    void GetAddr_(std::vector<CAddress> &vAddr);
#endif

    //! Mark an entry as currently-connected-to.
    void Connected_(const CService &addr, int64_t nTime);

#ifdef I2PADDRMAN_EXTENSIONS
    void CheckAndDeleteB32Hash( const int nID, const CAddrInfo& aTerrible );         // Used in Shrink twice
    CAddrInfo* LookupB32addr(const std::string& sB32addr);
#endif

public:
    /**
     * serialized format:
     * * version byte (currently 1)
     * * 0x20 + nKey (serialized as if it were a vector, for backward compatibility)
     * * nNew
     * * nTried
     * * number of "new" buckets XOR 2**30
     * * all nNew addrinfos in vvNew
     * * all nTried addrinfos in vvTried
     * * for each bucket:
     *   * number of elements
     *   * for each element: index
     *
     * 2**30 is xorred with the number of buckets to make addrman deserializer v0 detect it
     * as incompatible. This is necessary because it did not check the version number on
     * deserialization.
     *
     * Notice that vvTried, mapAddr and vVector are never encoded explicitly;
     * they are instead reconstructed from the other information.
     *
     * vvNew is serialized, but only used if ADDRMAN_UNKOWN_BUCKET_COUNT didn't change,
     * otherwise it is reconstructed as well.
     *
     * This format is more complex, but significantly smaller (at most 1.5 MiB), and supports
     * changes to the ADDRMAN_ parameters without breaking the on-disk structure.
     *
     * We don't use ADD_SERIALIZE_METHODS since the serialization and deserialization code has
     * very little in common.
     *
     */
    template<typename Stream>
    void Serialize(Stream &s, int nType, int nVersionDummy) const
    {
        LOCK(cs);

        unsigned char nVersion = 1;
        s << nVersion;
        s << ((unsigned char)32);
        s << nKey;
        s << nNew;
        s << nTried;

        int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
        s << nUBuckets;
        std::map<int, int> mapUnkIds;
        int nIds = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
            mapUnkIds[(*it).first] = nIds;
            const CAddrInfo &info = (*it).second;
            if (info.nRefCount) {
                assert(nIds != nNew); // this means nNew was wrong, oh ow
                s << info;
                nIds++;
            }
        }
        nIds = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
            const CAddrInfo &info = (*it).second;
            if (info.fInTried) {
                assert(nIds != nTried); // this means nTried was wrong, oh ow
                s << info;
                nIds++;
            }
        }
        for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            int nSize = 0;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1)
                    nSize++;
            }
            s << nSize;
            for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
                if (vvNew[bucket][i] != -1) {
                    int nIndex = mapUnkIds[vvNew[bucket][i]];
                    s << nIndex;
                }
            }
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersionDummy)
    {
        LOCK(cs);

        Clear();

        unsigned char nVersion;
        s >> nVersion;
        unsigned char nKeySize;
        s >> nKeySize;
        if (nKeySize != 32) throw std::ios_base::failure("Incorrect keysize in addrman deserialization");
        s >> nKey;
        s >> nNew;
        s >> nTried;
        int nUBuckets = 0;
        s >> nUBuckets;
#ifdef I2PADDRMAN_EXTENSIONS
        mapI2pHashes.clear();
#endif
        if (nVersion != 0) {
            nUBuckets ^= (1 << 30);
        }

        // Deserialize entries from the new table.
        for (int n = 0; n < nNew; n++) {
            CAddrInfo &info = mapInfo[n];
            s >> info;

#ifdef I2PADDRMAN_EXTENSIONS
            if( (info.nServices & 0xFFFFFFFFFFFFFF00) != 0 ) {
                LogPrint( "addrman", "While reading new %s, from %s found upper service bits set = %x, cleared.\n", info.ToString(), info.source.ToString(), info.nServices );
                info.nServices &= 0xFF;
            }
#endif
            
            mapAddr[info] = n;
            info.nRandomPos = vRandom.size();
            vRandom.push_back(n);

#ifdef I2PADDRMAN_EXTENSIONS
            if( info.CheckAndSetGarlicCat() )
                LogPrint( "addrman", "While reading new peers, did not expect to need the garliccat fixed for destination %s\n", info.ToString() );
            if( info.IsI2P() ) {
                info.SetPort( 0 );              // Make sure the CService port is set to ZERO
                uint256 b32hash = GetI2pDestinationHash( info.GetI2pDestination() );
                if( mapI2pHashes.count( b32hash ) == 0 )
                    mapI2pHashes[b32hash] = n;
                else
                    LogPrint( "addrman", "While reading new peer %s, could not create a base32 hash for one that already exists.\n", info.ToString() );
            } else if( info.GetPort() == 0 ) {                    // not yet sure why clearnet nodes have ports zeroed..ToDo: investigating
                LogPrint( "addrman", "While reading new peer %s, unexpected to find port set to zero, changed to default.\n", info.ToString() );
                info.SetPort( Params().GetDefaultPort() );
            }
#endif
            if (nVersion != 1 || nUBuckets != ADDRMAN_NEW_BUCKET_COUNT) {
                // In case the new table data cannot be used (nVersion unknown, or bucket count wrong),
                // immediately try to give them a reference based on their primary source address.
                int nUBucket = info.GetNewBucket(nKey);
                int nUBucketPos = info.GetBucketPosition(nKey, true, nUBucket);
                if (vvNew[nUBucket][nUBucketPos] == -1) {
                    vvNew[nUBucket][nUBucketPos] = n;
                    info.nRefCount++;
                }
            }
        }
        nIdCount = nNew;

        // Deserialize entries from the tried table.
        int nLost = 0;
        for (int n = 0; n < nTried; n++) {
            CAddrInfo info;
            s >> info;
#ifdef I2PADDRMAN_EXTENSIONS
            if( (info.nServices & 0xFFFFFFFFFFFFFF00) != 0 ) {
                LogPrint( "addrman", "While reading tried %s, from %s found upper service bits set = %x, cleared.\n", info.ToString(), info.source.ToString(), info.nServices );
                info.nServices &= 0xFF;
            }
#endif
            int nKBucket = info.GetTriedBucket(nKey);
            int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);
            if (vvTried[nKBucket][nKBucketPos] == -1) {
                info.nRandomPos = vRandom.size();
                info.fInTried = true;
                vRandom.push_back(nIdCount);
                mapInfo[nIdCount] = info;
                mapAddr[info] = nIdCount;
                vvTried[nKBucket][nKBucketPos] = nIdCount;
#ifdef I2PADDRMAN_EXTENSIONS
                if( info.CheckAndSetGarlicCat() )
                    LogPrint( "addrman", "While reading tried peers, did not expect to need the garliccat fixed for destination %s\n", info.ToString() );
                if( info.IsI2P() ) {
                    info.SetPort( 0 );              // Make sure the CService port is set to ZERO
                    uint256 b32hash = GetI2pDestinationHash( info.GetI2pDestination() );
                    if( mapI2pHashes.count( b32hash ) == 0 )
                        mapI2pHashes[b32hash] = nIdCount;
                    else
                        LogPrint( "addrman", "While reading tried peer %s, could not create a base32 hash for one that already exists.\n", info.ToString() );
                } else if( info.GetPort() == 0 ) {                    // not yet sure why clearnet nodes have ports zeroed..ToDo: investigating
                    LogPrint( "addrman", "While reading tried peer %s, unexpected to find port set to zero, changed to default.\n", info.ToString() );
                    info.SetPort( Params().GetDefaultPort() );
                }
#endif
                nIdCount++;
            } else {
                nLost++;
            }
        }
        nTried -= nLost;
        
        // Deserialize positions in the new table (if possible).
        for (int bucket = 0; bucket < nUBuckets; bucket++) {
            int nSize = 0;
            s >> nSize;
            for (int n = 0; n < nSize; n++) {
                int nIndex = 0;
                s >> nIndex;
                if (nIndex >= 0 && nIndex < nNew) {
                    CAddrInfo &info = mapInfo[nIndex];
                    int nUBucketPos = info.GetBucketPosition(nKey, true, bucket);
                    if (nVersion == 1 && nUBuckets == ADDRMAN_NEW_BUCKET_COUNT && vvNew[bucket][nUBucketPos] == -1 && info.nRefCount < ADDRMAN_NEW_BUCKETS_PER_ADDRESS) {
                        info.nRefCount++;
                        vvNew[bucket][nUBucketPos] = nIndex;
                    }
                }
            }
        }
 
        // Prune new entries with refcount 0 (as a result of collisions).
        int nLostUnk = 0;
        for (std::map<int, CAddrInfo>::const_iterator it = mapInfo.begin(); it != mapInfo.end(); ) {
            if (it->second.fInTried == false && it->second.nRefCount == 0) {
                std::map<int, CAddrInfo>::const_iterator itCopy = it++;
                Delete(itCopy->first);
                nLostUnk++;
            } else {
                it++;
            }
        }
        if (nLost + nLostUnk > 0) {
            LogPrint("addrman", "addrman lost %i new and %i tried addresses due to collisions\n", nLostUnk, nLost);
        }
 
        Check();
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return (CSizeComputer(nType, nVersion) << *this).size();
    }

    void Clear()
    {
        std::vector<int>().swap(vRandom);
        nKey = GetRandHash();
        for (size_t bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvNew[bucket][entry] = -1;
            }
        }
        for (size_t bucket = 0; bucket < ADDRMAN_TRIED_BUCKET_COUNT; bucket++) {
            for (size_t entry = 0; entry < ADDRMAN_BUCKET_SIZE; entry++) {
                vvTried[bucket][entry] = -1;
            }
        }

        nIdCount = 0;
        nTried = 0;
        nNew = 0;
    }
 
    CAddrMan()
    {
    Clear();
    }

    ~CAddrMan()
    {
    nKey.SetNull();
    }


    //! Return the number of (unique) addresses in all tables.
    int size()
    {
        return vRandom.size();
    }

#ifdef I2PADDRMAN_EXTENSIONS
    //! Return the number of (unique) b32.i2p addresses in the hash table
    int b32HashTableSize()
    {
        return mapI2pHashes.size();
    }

    std::string GetI2pBase64Destination(const std::string& sB32addr);
    int CopyDestinationStats( std::vector<CDestinationStats>& vStats );
#endif

    //! Consistency check
    void Check()
    {
#ifdef DEBUG_ADDRMAN
        {
            LOCK(cs);
            int err;
            if ((err=Check_()))
                LogPrintf("ADDRMAN CONSISTENCY CHECK FAILED!!! err=%i\n", err);
        }
#endif
    }

    //! Add a single address.
    bool Add(const CAddress &addr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        bool fRet = false;
        {
            LOCK(cs);
            Check();
            fRet |= Add_(addr, source, nTimePenalty);
            Check();
        }
        if (fRet)
            LogPrint("addrman", "Added %s from %s: %i tried, %i new\n", addr.ToStringIPPort(), source.ToString(), nTried, nNew);
        return fRet;
    }

    //! Add multiple addresses.
    bool Add(const std::vector<CAddress> &vAddr, const CNetAddr& source, int64_t nTimePenalty = 0)
    {
        int nAdd = 0;
        {
            LOCK(cs);
            Check();
            for (std::vector<CAddress>::const_iterator it = vAddr.begin(); it != vAddr.end(); it++)
                nAdd += Add_(*it, source, nTimePenalty) ? 1 : 0;
            Check();
        }
        if (nAdd)
            LogPrint("addrman", "Added %i addresses from %s: %i tried, %i new\n", nAdd, source.ToString(), nTried, nNew);
        return nAdd > 0;
    }

    //! Mark an entry as accessible.
    void Good(const CAddress &addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Good_(addr, nTime);
            Check();
        }
    }

    //! Mark an entry as connection attempted to.
    void Attempt(const CService &addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Attempt_(addr, nTime);
            Check();
        }
    }

    /**
     * Choose an address to connect to.
     * nUnkBias determines how much "new" entries are favored over "tried" ones (0-100).
     */
    CAddrInfo Select()
    {
        CAddrInfo addrRet;
        {
            LOCK(cs);
            Check();
            addrRet = Select_();
            Check();
        }
        return addrRet;
    }

    //! Return a bunch of addresses, selected at random.
#ifdef I2PADDRMAN_EXTENSIONS
    std::vector<CAddress> GetAddr(const bool fIpOnly, const bool fI2pOnly)
#else
    std::vector<CAddress> GetAddr()
#endif
    {
        Check();
        std::vector<CAddress> vAddr;
        {
            LOCK(cs);
#ifdef I2PADDRMAN_EXTENSIONS
            GetAddr_(vAddr, fIpOnly, fI2pOnly);
#else
            GetAddr_(vAddr);
#endif
        }
        Check();
        return vAddr;
    }

    //! Mark an entry as currently-connected-to.
    void Connected(const CService &addr, int64_t nTime = GetAdjustedTime())
    {
        {
            LOCK(cs);
            Check();
            Connected_(addr, nTime);
            Check();
        }
    }
};

#endif // ANONCOIN_ADDRMAN_H
