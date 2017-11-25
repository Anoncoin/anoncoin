// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ANONCOIN_WALLETDB_H
#define ANONCOIN_WALLETDB_H

#include "amount.h"
#include "db.h"
#include "key.h"
#include "keystore.h"

#ifdef ENABLE_STEALTH
#include "base58.h"
#include "stealth.h"
#endif

#include <list>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

class CAccount;
class CAccountingEntry;
struct CBlockLocator;
class CKeyPool;
class CMasterKey;
class CScript;
class CWallet;
class CWalletTx;
class uint160;
class uint256;

/** Error statuses for the wallet database */
enum DBErrors
{
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_NONCRITICAL_ERROR,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
    DB_NEED_REWRITE
};

class CKeyMetadata {
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    int64_t nCreateTime; // 0 means unknown

    CKeyMetadata() {
        SetNull();
    }
    CKeyMetadata(int64_t nCreateTime_) {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = nCreateTime_;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nCreateTime);
    }

    void SetNull() {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
    }
};

#ifdef ENABLE_STEALTH
class CStealthKeyMetadata {
// -- used to get secret for keys created by stealth transaction with wallet locked
public:
    CStealthKeyMetadata() {}
    
    CStealthKeyMetadata(CPubKey pkEphem_, CPubKey pkScan_) : pkEphem(pkEphem_), pkScan(pkScan_) {}
    
    CPubKey pkEphem;
    CPubKey pkScan;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(pkEphem);
        READWRITE(pkScan);
    }

};
#endif

/** Access to the wallet database (wallet.dat) */
class CWalletDB : public CDB
{
public:
    CWalletDB(const std::string& strFilename, const char* pszMode = "r+", bool fFlushOnClose = true) : CDB(strFilename, pszMode, fFlushOnClose)
    {
    }

#ifdef ENABLE_STEALTH
    Dbc* GetAtCursor() {
        return GetCursor();
    }
    
    DbTxn* GetAtActiveTxn() {
        return activeTxn;
    }

    Dbc* GetTxnCursor() {
        if (!pdb)
            return NULL;
        
        DbTxn* ptxnid = activeTxn; // call TxnBegin first
        
        Dbc* pcursor = NULL;
        int ret = pdb->cursor(ptxnid, &pcursor, 0);
        if (ret != 0)
            return NULL;
        return pcursor;
    }

    bool WriteStealthKeyMeta(const CKeyID& keyId, const CStealthKeyMetadata& sxKeyMeta) {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("sxKeyMeta"), keyId), sxKeyMeta, true);
    }
    
    bool EraseStealthKeyMeta(const CKeyID& keyId) {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("sxKeyMeta"), keyId));
    }
    
    bool WriteStealthAddress(const CStealthAddress& sxAddr) {
        nWalletDBUpdated++;

        return Write(std::make_pair(std::string("sxAddr"), sxAddr.scan_pubkey), sxAddr, true);
    }
    
    bool ReadStealthAddress(CStealthAddress& sxAddr) {
        // -- set scan_pubkey before reading
        return Read(std::make_pair(std::string("sxAddr"), sxAddr.scan_pubkey), sxAddr);
    }
#endif

    bool WriteName(const std::string& strAddress, const std::string& strName);
    bool EraseName(const std::string& strAddress);

    bool WritePurpose(const std::string& strAddress, const std::string& purpose);
    bool ErasePurpose(const std::string& strAddress);

    bool WriteTx(uint256 hash, const CWalletTx& wtx);
    bool EraseTx(uint256 hash);

    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta);
    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta);
    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey);

    bool WriteCScript(const uint160& hash, const CScript& redeemScript);

    bool WriteWatchOnly(const CScript &script);
    bool EraseWatchOnly(const CScript &script);

    bool WriteBestBlock(const CBlockLocator& locator);
    bool ReadBestBlock(CBlockLocator& locator);

    bool WriteOrderPosNext(int64_t nOrderPosNext);

    bool WriteDefaultKey(const CPubKey& vchPubKey);

    bool ReadPool(int64_t nPool, CKeyPool& keypool);
    bool WritePool(int64_t nPool, const CKeyPool& keypool);
    bool ErasePool(int64_t nPool);

    bool WriteMinVersion(int nVersion);

    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);

    /// Write destination data key,value tuple to database
    bool WriteDestData(const std::string &address, const std::string &key, const std::string &value);
    /// Erase destination data tuple from wallet database
    bool EraseDestData(const std::string &address, const std::string &key);

    bool WriteAccountingEntry(const CAccountingEntry& acentry);
    CAmount GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors ReorderTransactions(CWallet* pwallet);
    DBErrors LoadWallet(CWallet* pwallet);
    DBErrors FindWalletTx(CWallet* pwallet, std::vector<uint256>& vTxHash, std::vector<CWalletTx>& vWtx);
    DBErrors ZapWalletTx(CWallet* pwallet, std::vector<CWalletTx>& vWtx);
    static bool Recover(CDBEnv& dbenv, const std::string& filename, bool fOnlyKeys);
    static bool Recover(CDBEnv& dbenv, const std::string& filename);

private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);

    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
};

bool BackupWallet(const CWallet& wallet, const std::string& strDest);
void ThreadFlushWalletDB(const std::string& strFile);

#endif // ANONCOIN_WALLETDB_H
