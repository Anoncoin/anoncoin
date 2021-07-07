// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ANONCOIN_WALLET_H
#define ANONCOIN_WALLET_H

#include "amount.h"
#include "block.h"
#include "crypter.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script.h"

#ifdef ENABLE_STEALTH
#include "stealth.h"
#endif

#include "transaction.h"
#include "ui_interface.h"
#include "util.h"
#include "walletdb.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/shared_ptr.hpp>

#ifdef ENABLE_STEALTH
typedef std::map<CKeyID, CStealthKeyMetadata> StealthKeyMetaMap;
#endif
typedef std::map<std::string, std::string> mapValue_t;

//! Constant definitions found in the wallet source code file.

//! -paytxfee default
extern const CAmount DEFAULT_TRANSACTION_FEE;
//! -paytxfee will warn if called with a higher fee than this amount (in satoshis) per KB
extern const CAmount nHighTransactionFeeWarning;
//! -maxtxfee default
extern const CAmount DEFAULT_TRANSACTION_MAXFEE;
//! -txconfirmtarget default
extern const unsigned int DEFAULT_TX_CONFIRM_TARGET;
//! -maxtxfee will warn if called with a higher fee than this amount (in satoshis)
extern const CAmount nHighTransactionMaxFeeWarning;
//! Largest (in bytes) free transaction we're willing to create
extern const uint32_t MAX_FREE_TRANSACTION_CREATE_SIZE;

//! Variable definitions found in the wallet source code file.

//!
extern bool bSpendZeroConfChange;
//!
extern bool fSendFreeTransactions;
//!
extern bool fPayAtLeastCustomFee;
//! Migrating all code to use this new FeeRate as the softwares definition of the minimum Tx fee
extern CFeeRate w_minTxFeeRate;
//!
extern CFeeRate payTxFee;
//!
extern CAmount maxTxFee;
//!
extern unsigned int nTxConfirmTarget;
//! Setting required for v9 code to work (depreciate)
//!
extern int64_t nTransactionFee;

class CCoinControl;
class CScript;
class CWallet;


//! Defined in wallet.cpp for use in the CAccountingEntry class serialization routines
void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue);
void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue);

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_LATEST  = 60000,
//    FEATURE_STEALTH = 70000 // stealth addresses
};

/**
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};


/**
 * Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    CAmount nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64_t nOrderPos;  //! position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead())
        {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty()))
            {
                CDataStream ss(nType, nVersion);
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead())
        {
            mapValue.clear();
            if (std::string::npos != nSepPos)
            {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), nType, nVersion);
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires=0)
    {
        nTimeCreated = (nExpires ? GetTime() : 0);
        nTimeExpires = nExpires;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};

/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;

    CKeyPool()
    {
        nTime = GetTime();
    }

    CKeyPool(const CPubKey& vchPubKeyIn)
    {
        nTime = GetTime();
        vchPubKey = vchPubKeyIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
    }
};

/** Address book data */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData()
    {
        purpose = "unknown";
    }

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

/** A key allocated from the key pool. */
class CReserveKey : public CReserveScript
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    CPubKey vchPubKey;
public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey);
    void KeepKey();
    void KeepScript() { KeepKey(); }
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
    int GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const;

protected:
    uintFakeHash hashBlock;

public:
    std::vector<uint256> vMerkleBranch;
    int nIndex;

    // memory only
    mutable bool fMerkleVerified;


    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock = 0;
        nIndex = -1;
        fMerkleVerified = false;
    }

    uintFakeHash GetTxBlockHash() const { return hashBlock; }
    void SetTxBlockHash( const uintFakeHash& hashBlockIn ) { hashBlock = hashBlockIn; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(*(CTransaction*)this);
        nVersion = this->nVersion;
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    int SetMerkleBranch(const CBlock* pblock=NULL);

    /**
     * Return depth of transaction in blockchain:
     * -1  : not in blockchain, and not in memory pool (conflicted transaction)
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { CBlockIndex *pindexRet; return GetDepthInMainChainINTERNAL(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    bool AcceptToMemoryPool(bool fLimitFree=true);
};

//! New construct, not yet used. ToDo:
struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/**
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //! time received by this node
    unsigned int nTimeSmart;
    char fFromMe;
    std::string strFromAccount;
    int64_t nOrderPos; //! position in ordered transaction list

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(const CWallet* pwalletIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nAvailableCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (ser_action.ForRead())
            Init(NULL);
        char fSpent = false;

        if (!ser_action.ForRead())
        {
            mapValue["fromaccount"] = strFromAccount;

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //! Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead())
        {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("version");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter filter) const;
    CAmount GetCredit(const isminefilter filter) const;
    CAmount GetImmatureCredit(bool fUseCache=true) const;
    CAmount GetAvailableCredit(bool fUseCache=true) const;
    CAmount GetImmatureWatchOnlyCredit(const bool fUseCache=true) const;
    CAmount GetAvailableWatchOnlyCredit(const bool fUseCache=true) const;
    CAmount GetChange() const;

    void GetAmounts(std::list<std::pair<CTxDestination, int64_t> >& listReceived,
                    std::list<std::pair<CTxDestination, int64_t> >& listSent, CAmount& nFee, std::string& strSentAccount, const isminefilter filter) const;

    void GetAccountAmounts(const std::string& strAccount, CAmount& nReceived,
                           CAmount& nSent, CAmount& nFee, const isminefilter filter) const;

    bool IsFromMe(const isminefilter filter) const
    {
        return (GetDebit(filter) > 0);
    }

    bool IsTrusted() const;
    bool WriteToDisk(CWalletDB *pwalletdb);
    int64_t GetTxTime() const;
    int GetRequestCount() const;
    bool RelayWalletTransaction();
    std::set<uint256> GetConflicts() const;
};

class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;
    bool fSpendable;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn; fSpendable = fSpendableIn;
    }

    std::string ToString() const
    {
        return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString().c_str(), i, nDepth, FormatMoney(tx->vout[i].nValue).c_str());
    }

    void print() const
    {
        LogPrintf("%s\n", ToString().c_str());
    }
};

/**
 * A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore, public CValidationInterface
{
private:
    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    typedef std::multimap<COutPoint, uint256> TxSpends;

    bool SelectCoins(const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl *coinControl = NULL) const;

    CWalletDB *pwalletdbEncryption;

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

    int64_t nNextResend;
    int64_t nLastResend;

    //! Used to keep track of spent outpoints, and detect and report conflicts (double-spends or
    //! mutated transactions where the mutant gets mined).
    TxSpends mapTxSpends;

    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; }
    //! Look up a destination data tuple in the store, return true if found false otherwise
    bool GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const;
    //!
    bool NewKeyPool();

public:
    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;

    /**
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable CCriticalSection cs_wallet;

    bool fFileBacked;
    uint32_t nMasterKeyMaxID;
    int64_t nOrderPosNext;
    int64_t nTimeFirstKey;

    std::string strWalletFile;

    //! ToDo: These maps and set should all be made private, and references made to them be done as calls to new public methods...
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;
    std::map<uint256, CWalletTx> mapWallet;
    std::map<uintFakeHash, int> mapRequestCount;            //! This is used to track the block & transaction hashes from our wallet, and is stored as sha256d hashes for both.
    std::map<CTxDestination, CAddressBookData> mapAddressBook;
    MasterKeyMap mapMasterKeys;

    std::set<COutPoint> setLockedCoins;
    std::set<int64_t> setKeyPool;

#ifdef ENABLE_STEALTH
    std::set<CStealthAddress> stealthAddresses;
    StealthKeyMetaMap mapStealthKeyMeta;
    uint32_t nStealth, nFoundStealth; // for reporting, zero before use
#endif


    CPubKey vchDefaultKey;

    // Whoever keeps moving these dam txfee variables around, putting them in as static class members, such
    // as was done here in the wallet class, should be shot.
    // While trying to upgrade code for v9 that was done in the transaction class.  What a huge pain-in-the-ass they have been.
    // Moved up to the top of this file & declared as external, with the actual variable
    // placed in wallet.cpp and storage allocated for it their during compile time as should have been done in the
    // first place.  Having it here will not allow linking unless the whole wallet code base is upgraded, and I'm
    // not doing that today!  GR
    //
    // static CFeeRate minTxFee;       // V10 param, now set in init.cpp by user param -mintxfee
    // static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool);

    CWallet()
    {
        SetNull();
    }

    CWallet(std::string strWalletFileIn)
    {
        SetNull();

        strWalletFile = strWalletFileIn;
        fFileBacked = true;
    }

    ~CWallet()
    {
        delete pwalletdbEncryption;
        pwalletdbEncryption = NULL;
    }

    void SetNull()
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nNextResend = 0;
        nLastResend = 0;
        nTimeFirstKey = 0;
    }


#ifdef ENABLE_STEALTH

    bool Lock();
    bool CreateTransaction(CScript scriptPubKey, int64_t nValue, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, std::string& failReason, const CCoinControl* coinControl);

    bool NewStealthAddress(std::string& sError, std::string& sLabel, CStealthAddress& sxAddr);
    bool AddStealthAddress(CStealthAddress& sxAddr);
    bool UnlockStealthAddresses(const CKeyingMaterial& vMasterKeyIn);
    bool UpdateStealthAddress(std::string &addr, std::string &label, bool addIfNotExist);

    bool CreateStealthTransaction(CScript scriptPubKey, int64_t nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl=NULL);
    std::string SendStealthMoney(CScript scriptPubKey, int64_t nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, bool fAskFee=false);
    bool SendStealthMoneyToDestination(CStealthAddress& sxAddress, int64_t nValue, std::string& sNarr, CWalletTx& wtxNew, std::string& sError, bool fAskFee=false);
    bool FindStealthTransactions(const CTransaction& tx, mapValue_t& mapNarr);

#endif

    //!
    const CWalletTx* GetWalletTx(const uint256& hash) const;
    //!
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl = NULL) const;
    //!
    bool SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet) const;
    //!
    bool IsSpent(const uint256& hash, unsigned int n) const;
    //!
    bool IsLockedCoin(uint256 hash, unsigned int n) const;
    //!
    void LockCoin(COutPoint& output);
    //!
    void UnlockCoin(COutPoint& output);
    //!
    void UnlockAllCoins();
    //!
    void ListLockedCoins(std::vector<COutPoint>& vOutpts);
    //! Keystore implementation, Generate a new key...
    CPubKey GenerateNewKey();
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey &pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
    //! Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &metadata);
    //!
    bool LoadMinVersion(int nVersion) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }
    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    //!
    bool AddCScript(const CScript& redeemScript);
    //!
    bool LoadCScript(const CScript& redeemScript);
    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
    //! Adds a destination data tuple to the store, without saving it to disk
    bool LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript &dest);
    //!
    bool RemoveWatchOnly(const CScript &dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript &dest);
    //!
    bool Unlock(const SecureString& strWalletPassphrase);
    //!
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    //!
    bool EncryptWallet(const SecureString& strWalletPassphrase);
    //!
    void GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const;
    //! Increment the next transaction order id, return next transaction order id
    int64_t IncOrderPosNext(CWalletDB *pwalletdb = NULL);
    /**
     * Get the wallet's activity log
     * @return multimap of ordered transactions and accounting entries
     * @warning Returned pointers are *only* valid within the scope of passed acentries
     */
    TxItems OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount = "");
    //!
    void MarkDirty();
    //!
    bool AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb);
    //!
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate);
    //!
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    //!
    void ReacceptWalletTransactions();
    //!
    std::vector<uint256> ResendWalletTransactionsBefore(int64_t nTime);
    //!
    CAmount GetBalance() const;
    //!
    CAmount GetUnconfirmedBalance() const;
    //!
    CAmount GetImmatureBalance() const;
    //!
    CAmount GetWatchOnlyBalance() const;
    //!
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    //!
    CAmount GetImmatureWatchOnlyBalance() const;
    //!
    bool CreateTransaction(const std::vector<std::pair<CScript, CAmount> >& vecSend,
                           CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet,
                           std::string& strFailReason, const CCoinControl *coinControl = NULL);
    //!
    bool CreateTransaction(CScript scriptPubKey, CAmount nValue,
                           CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet,
                           std::string& strFailReason, const CCoinControl *coinControl = NULL);
    //!
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);
    //!
    bool TopUpKeyPool(unsigned int kpSize = 0);
    //!
    int64_t AddReserveKey(const CKeyPool& keypool);
    //!
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool);
    //!
    void KeepKey(int64_t nIndex);
    //!
    void ReturnKey(int64_t nIndex);
    //!
    bool GetKeyFromPool(CPubKey &key);
    //!
    int64_t GetOldestKeyPoolTime();
    //!
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const;
    //!
    std::set< std::set<CTxDestination> > GetAddressGroupings();
    //!
    std::map<CTxDestination, int64_t> GetAddressBalances();
    //!
    std::set<CTxDestination> GetAccountAddresses(std::string strAccount) const;
    //!
    isminetype IsMine(const CTxIn& txin) const;
    //!
    isminetype IsMine(const CTxOut& txout) const { return ::IsMine(*this, txout.scriptPubKey); }
    //!
    bool IsMine(const CTransaction& tx) const;
    //!
    int64_t GetDebit(const CTxIn& txin, const isminefilter filter) const;
    //!
    int64_t GetDebit(const CTransaction& tx, const isminefilter filter) const;
    //!
    int64_t GetCredit(const CTxOut& txout, const isminefilter filter) const;
    //!
    int64_t GetCredit(const CTransaction& tx, const isminefilter filter) const;
    //!
    int64_t GetChange(const CTxOut& txout) const;
    //!
    int64_t GetChange(const CTransaction& tx) const;
    //!
    bool IsChange(const CTxOut& txout) const;
    //! should probably be renamed to IsRelevantToMe
    bool IsFromMe(const CTransaction& tx) const { return (GetDebit(tx, ISMINE_ALL) > 0); }
    //!
    DBErrors LoadWallet(bool fFirstRunRet);
    //!
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    //!
    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose);
    //!
    bool DelAddressBook(const CTxDestination& address);
    //!
    uint32_t GetKeyPoolSize() { AssertLockHeld(cs_wallet); return setKeyPool.size(); }
    //!
    bool SetDefaultKey(const CPubKey &vchPubKey);
    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);
    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);
    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }
    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    //! NOTE - THESE WALLET METHODS are TIED to BOOST signals2 in the validation interface.
    //! Should not be thought of as just another method within this class, keep that in mind when
    //! references are made to them.  They are more like slots, than anything else.
    //! Each wallet has these validation interface methods defined, and handles processing
    //! of wallet related event activity through them...GR
    //!
    void SyncTransaction( const CTransaction& tx, const CBlock* pblock );
    //!
    void SetBestChain( const CBlockLocator& loc );
    //!
    void UpdatedTransaction( const uint256& uintTxHash );
    //!
    void Inventory( const uintFakeHash &uintShad );
    //!
    void ResendWalletTransactions();
    //! BlockChecked() is not used by a wallet, however a miner cares about the validation result on a newly submitted block
    virtual void BlockChecked(const CBlock&, const CValidationState&) {};
    //! Returns a reserved key from the primary wallet, it can then be used by a miner after submitting a new found block.
    void GetScriptForMining(boost::shared_ptr<CReserveScript>& script);
    //!
    void ResetRequestCount(const uintFakeHash &shad);

    //!
    //! THESE WALLET METHODS are BOOST signals, defined for each wallet and OUTSIDE
    //! of the validation interface, used extensively in QT applications...
    //!

    /**
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination
            &address, const std::string &label, bool isMine,
            const std::string &purpose,
            ChangeType status)> NotifyAddressBookChanged;

    /**
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256& uintTxHash, ChangeType status)> NotifyTransactionChanged;

    /** Show progress e.g. for rescan */
    boost::signals2::signal<void (const std::string &title, int nProgress)> ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;
};

#endif
