#include "base58.h"
#include "core_io.h"
#include "rpcserver.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "sync.h"
#include "timedata.h"
#include "wallet.h"
#include "walletdb.h"

#include "stealth.h"
#include "rpcstealth.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace std;
using namespace json_spirit;

Value getnewstealthaddress(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewstealthaddress [label]\n"
            "Returns a new Anoncoin stealth address for receiving payments anonymously.  ");
    
    if (pwalletMain->IsLocked())
        throw runtime_error("Failed: Wallet must be unlocked.");
    
    std::string sLabel;
    if (params.size() > 0)
        sLabel = params[0].get_str();
    
    CStealthAddress sxAddr;
    std::string sError;
    if (!pwalletMain->NewStealthAddress(sError, sLabel, sxAddr))
        throw runtime_error(std::string("Could get new stealth address: ") + sError);
    
    if (!pwalletMain->AddStealthAddress(sxAddr))
        throw runtime_error("Could not save to wallet.");
    
    return sxAddr.Encoded();
}

Value liststealthaddresses(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "liststealthaddresses [show_secrets=0]\n"
            "List owned stealth addresses.");
    
    bool fShowSecrets = false;
    
    if (params.size() > 0) {
        std::string str = params[0].get_str();
        
        if (str == "0" || str == "n" || str == "no" || str == "-" || str == "false")
            fShowSecrets = false;
        else
            fShowSecrets = true;
    }
    
    if (fShowSecrets) {
        if (pwalletMain->IsLocked())
            throw runtime_error("Failed: Wallet must be unlocked.");
    }
    
    Object result;
    
    std::set<CStealthAddress>::iterator it;
    for (it = pwalletMain->stealthAddresses.begin(); it != pwalletMain->stealthAddresses.end(); ++it) {
        if (it->scan_secret.size() < 1)
            continue; // stealth address is not owned
        
        if (fShowSecrets) {
            Object objA;
            objA.push_back(Pair("Label        ", it->label));
            objA.push_back(Pair("Address      ", it->Encoded()));
            objA.push_back(Pair("Scan Secret  ", HexStr(it->scan_secret.begin(), it->scan_secret.end())));
            objA.push_back(Pair("Spend Secret ", HexStr(it->spend_secret.begin(), it->spend_secret.end())));
            result.push_back(Pair("Stealth Address", objA));
        } else {
            result.push_back(Pair("Stealth Address", it->Encoded() + " - " + it->label));
        }
    }
    
    return result;
}

Value importstealthaddress(const Array& params, bool fHelp) {
    if (fHelp || params.size() < 2)
        throw runtime_error(
            "importstealthaddress <scan_secret> <spend_secret> [label]\n"
            "Import an owned stealth addresses.");
    
    std::string sScanSecret  = params[0].get_str();
    std::string sSpendSecret = params[1].get_str();
    std::string sLabel;
    
    
    if (params.size() > 2) {
        sLabel = params[2].get_str();
    }
    
    std::vector<uint8_t> vchScanSecret;
    std::vector<uint8_t> vchSpendSecret;
    
    if (IsHex(sScanSecret)) {
        vchScanSecret = ParseHex(sScanSecret);
    } else {
        if (!DecodeBase58(sScanSecret, vchScanSecret))
            throw runtime_error("Could not decode scan secret as hex or base58.");
    }
    
    if (IsHex(sSpendSecret)) {
        vchSpendSecret = ParseHex(sSpendSecret);
    } else {
        if (!DecodeBase58(sSpendSecret, vchSpendSecret))
            throw runtime_error("Could not decode spend secret as hex or base58.");
    }
    
    if (vchScanSecret.size() != 32)
        throw runtime_error("Scan secret is not 32 bytes.");
    if (vchSpendSecret.size() != 32)
        throw runtime_error("Spend secret is not 32 bytes.");
    
    
    ec_secret scan_secret;
    ec_secret spend_secret;
    
    memcpy(&scan_secret.e[0], &vchScanSecret[0], 32);
    memcpy(&spend_secret.e[0], &vchSpendSecret[0], 32);
    
    ec_point scan_pubkey, spend_pubkey;
    if (SecretToPublicKey(scan_secret, scan_pubkey) != 0)
        throw runtime_error("Could not get scan public key.");
    
    if (SecretToPublicKey(spend_secret, spend_pubkey) != 0)
        throw runtime_error("Could not get spend public key.");
    
    
    CStealthAddress sxAddr;
    sxAddr.label = sLabel;
    sxAddr.scan_pubkey = scan_pubkey;
    sxAddr.spend_pubkey = spend_pubkey;
    
    sxAddr.scan_secret = vchScanSecret;
    sxAddr.spend_secret = vchSpendSecret;
    
    Object result;
    bool fFound = false;
    // -- find if address already exists
    std::set<CStealthAddress>::iterator it;
    for (it = pwalletMain->stealthAddresses.begin(); it != pwalletMain->stealthAddresses.end(); ++it) {
        CStealthAddress &sxAddrIt = const_cast<CStealthAddress&>(*it);
        if (sxAddrIt.scan_pubkey == sxAddr.scan_pubkey && sxAddrIt.spend_pubkey == sxAddr.spend_pubkey) {
            if (sxAddrIt.scan_secret.size() < 1) {
                sxAddrIt.scan_secret = sxAddr.scan_secret;
                sxAddrIt.spend_secret = sxAddr.spend_secret;
                fFound = true; // update stealth address with secrets
                break;
            }
            
            result.push_back(Pair("result", "Import failed - stealth address exists."));
            return result;
        }
    }
    
    if (fFound) {
        result.push_back(Pair("result", "Success, updated " + sxAddr.Encoded()));
    } else {
        pwalletMain->stealthAddresses.insert(sxAddr);
        result.push_back(Pair("result", "Success, imported " + sxAddr.Encoded()));
    }
    
    
    if (!pwalletMain->AddStealthAddress(sxAddr))
        throw runtime_error("Could not save to wallet.");
    
    return result;
}


Value sendtostealthaddress(const Array& params, bool fHelp) {
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendtostealthaddress <stealth_address> <amount> [narration] [comment] [comment-to]\n"
            "<amount> is a real and is rounded to the nearest 0.000001"
            + HelpRequiringPassphrase());
    
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    
    std::string sEncoded = params[0].get_str();
    int64_t nAmount = AmountFromValue(params[1]);
    
    std::string sNarr;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        sNarr = params[2].get_str();
    
    if (sNarr.length() > 24)
        throw runtime_error("Narration must be 24 characters or less.");
    
    CStealthAddress sxAddr;
    Object result;
    
    if (!sxAddr.SetEncoded(sEncoded)) {
        result.push_back(Pair("result", "Invalid Anoncoin stealth address."));
        return result;
    }
    
    
    CWalletTx wtx;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["to"]      = params[4].get_str();
    
    std::string sError;
    if (!pwalletMain->SendStealthMoneyToDestination(sxAddr, nAmount, sNarr, wtx, sError))
        throw JSONRPCError(RPC_WALLET_ERROR, sError);

    return wtx.GetHash().GetHex();
    
    result.push_back(Pair("result", "Not implemented yet."));
    
    return result;
}

Value clearwallettransactions(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 0)
        throw runtime_error(
            "clearwallettransactions \n"
            "delete all transactions from wallet - reload with scanforalltxns\n"
            "Warning: Backup your wallet first!");
    
    
    
    Object result;
    
    uint32_t nTransactions = 0;
    
    char cbuf[256];
    
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        
        CWalletDB walletdb(pwalletMain->strWalletFile);
        walletdb.TxnBegin();
        Dbc* pcursor = walletdb.GetTxnCursor();
        if (!pcursor)
            throw runtime_error("Cannot get wallet DB cursor");
        
        Dbt datKey;
        Dbt datValue;
        
        datKey.set_flags(DB_DBT_USERMEM);
        datValue.set_flags(DB_DBT_USERMEM);
        
        std::vector<unsigned char> vchKey;
        std::vector<unsigned char> vchType;
        std::vector<unsigned char> vchKeyData;
        std::vector<unsigned char> vchValueData;
        
        vchKeyData.resize(100);
        vchValueData.resize(100);
        
        datKey.set_ulen(vchKeyData.size());
        datKey.set_data(&vchKeyData[0]);
        
        datValue.set_ulen(vchValueData.size());
        datValue.set_data(&vchValueData[0]);
        
        unsigned int fFlags = DB_NEXT; // same as using DB_FIRST for new cursor
        while (true) {
            int ret = pcursor->get(&datKey, &datValue, fFlags);
            
            if (ret == ENOMEM || ret == DB_BUFFER_SMALL) {
                if (datKey.get_size() > datKey.get_ulen()) {
                    vchKeyData.resize(datKey.get_size());
                    datKey.set_ulen(vchKeyData.size());
                    datKey.set_data(&vchKeyData[0]);
                }
                
                if (datValue.get_size() > datValue.get_ulen()) {
                    vchValueData.resize(datValue.get_size());
                    datValue.set_ulen(vchValueData.size());
                    datValue.set_data(&vchValueData[0]);
                }
                // -- try once more, when DB_BUFFER_SMALL cursor is not expected to move
                ret = pcursor->get(&datKey, &datValue, fFlags);
            }
            
            if (ret == DB_NOTFOUND)
                break;
            else if (datKey.get_data() == NULL || datValue.get_data() == NULL || ret != 0) {
                snprintf(cbuf, sizeof(cbuf), "wallet DB error %d, %s", ret, db_strerror(ret));
                throw runtime_error(cbuf);
            }
            
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            ssValue.SetType(SER_DISK);
            ssValue.clear();
            ssValue.write((char*)datKey.get_data(), datKey.get_size());
            
            ssValue >> vchType;
            
            
            std::string strType(vchType.begin(), vchType.end());
            
            //printf("strType %s\n", strType.c_str());
            
            if (strType == "tx") {
                uint256 hash;
                ssValue >> hash;
                
                if ((ret = pcursor->del(0)) != 0) {
                    printf("Delete transaction failed %d, %s\n", ret, db_strerror(ret));
                    continue;
                }
                
                pwalletMain->mapWallet.erase(hash);
                pwalletMain->NotifyTransactionChanged(pwalletMain, hash, CT_DELETED);
                
                nTransactions++;
            }
        }
        pcursor->close();
        walletdb.TxnCommit();
        
        
        //pwalletMain->mapWallet.clear();
    }
    
    snprintf(cbuf, sizeof(cbuf), "Removed %u transactions.", nTransactions);
    result.push_back(Pair("complete", std::string(cbuf)));
    result.push_back(Pair("", "Reload with scanforstealthtxns or re-download blockchain."));
    
    
    return result;
}

Value scanforalltxns(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "scanforalltxns [fromHeight]\n"
            "Scan blockchain for owned transactions.");
    
    Object result;
    int32_t nFromHeight = 0;
    
    CBlockIndex *pindex = chainActive.Genesis();
    
    
    if (params.size() > 0)
        nFromHeight = params[0].get_int();
    
    
    if (nFromHeight > 0) {
        pindex = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
        while (pindex->nHeight > nFromHeight
            && pindex->pprev)
            pindex = pindex->pprev;
    }
    
    if (pindex == NULL)
        throw runtime_error("Genesis Block is not set.");
    
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        
        pwalletMain->MarkDirty();
        
        pwalletMain->ScanForWalletTransactions(pindex, true);
        pwalletMain->ReacceptWalletTransactions();
    }
    
    result.push_back(Pair("result", "Scan complete."));
    
    return result;
}

Value scanforstealthtxns(const Array& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "scanforstealthtxns [fromHeight]\n"
            "Scan blockchain for owned stealth transactions.");
    
    Object result;
    uint32_t nBlocks = 0;
    uint32_t nTransactions = 0;
    int32_t nFromHeight = 0;
    
    CBlockIndex *pindex = chainActive.Genesis();
    
    
    if (params.size() > 0)
        nFromHeight = params[0].get_int();
    
    
    if (nFromHeight > 0) {
        pindex = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
        while (pindex->nHeight > nFromHeight
            && pindex->pprev)
            pindex = pindex->pprev;
    }
    
    if (pindex == NULL)
        throw runtime_error("Genesis Block is not set.");
    
    // -- locks in AddToWalletIfInvolvingMe
    
    bool fUpdate = true; // todo: make it an option?
    
    pwalletMain->nStealth = 0;
    pwalletMain->nFoundStealth = 0;
    
    while (pindex) {
        nBlocks++;
        CBlock block;
        ReadBlockFromDisk(block, pindex->GetBlockPos());
        
        BOOST_FOREACH(CTransaction& tx, block.vtx)
        {
            std::string dummy("");
            if (!IsStandardTx(tx, dummy))
                continue; // leave out coinbase and others
            nTransactions++;
            
            pwalletMain->AddToWalletIfInvolvingMe(tx, &block, fUpdate);
        }
        
        pindex = pindex->pskip;
    }
    
    printf("Scanned %u blocks, %u transactions\n", nBlocks, nTransactions);
    printf("Found %u stealth transactions in blockchain.\n", pwalletMain->nStealth);
    printf("Found %u new owned stealth transactions.\n", pwalletMain->nFoundStealth);
    
    char cbuf[256];
    snprintf(cbuf, sizeof(cbuf), "%u new stealth transactions.", pwalletMain->nFoundStealth);
    
    result.push_back(Pair("result", "Scan complete."));
    result.push_back(Pair("found", std::string(cbuf)));
    
    return result;
}


