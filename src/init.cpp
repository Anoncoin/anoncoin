// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Many builder specific things set in the config file, ENABLE_WALLET is a good example.  Don't forget to include it this way in your source files.
#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "init.h"

#include "addrman.h"
#include "amount.h"
#include "consensus.h"
#include "checkpoints.h"
#include "compat/sanity.h"
//#include "coins.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "pow.h"
//#include "random.h"
#include "rpcserver.h"
#include "scrypt.h"
#include "txdb.h"
#include "ui_interface.h"                                   // Include this if you want language translation capability in your source files
#include "util.h"
#ifdef ENABLE_WALLET
#include "db.h"
#include "wallet.h"
#include "walletdb.h"
#endif
#ifdef ENABLE_I2PSAM
#include "i2pwrapper.h"
#endif

// #include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef WIN32
#include <signal.h>
#endif

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <openssl/crypto.h>

using namespace std;

#ifdef ENABLE_WALLET
CWallet* pwalletMain = NULL;
#endif
bool fFeeEstimatesInitialized = false;

#ifdef WIN32
// Win32 LevelDB doesn't use filedescriptors, and the ones used for
// accessing block files, don't count towards to fd_set size limit
// anyway.
#define MIN_CORE_FILEDESCRIPTORS 0
#else
#define MIN_CORE_FILEDESCRIPTORS 150
#endif

/** Used to pass flags to the Bind() function */
enum BindFlags {
    BF_NONE         = 0,
    BF_EXPLICIT     = (1U << 0),
    BF_REPORT_ERROR = (1U << 1),
    BF_WHITELIST    = (1U << 2),
};

static const char* FEE_ESTIMATES_FILENAME="fee_estimates.dat";
CClientUIInterface uiInterface;

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

//
// Thread management and startup/shutdown:
//
// The network-processing threads are all part of a thread group
// created by AppInit() or the Qt main() function.
//
// A clean exit happens when StartShutdown() or the SIGTERM
// signal handler sets fRequestShutdown, which triggers
// the DetectShutdownThread(), which interrupts the main thread group.
// DetectShutdownThread() then exits, which causes AppInit() to
// continue (it .joins the shutdown thread).
// Shutdown() is then
// called to clean up database connections, and stop other
// threads that should only be stopped after the main network-processing
// threads have exited.
//
// Note that if running -daemon the parent process returns from AppInit2
// before adding any threads to the threadGroup, so .join_all() returns
// immediately and the parent exits from main().
//
// Shutdown for Qt is very similar, only it uses a QTimer to detect
// fRequestShutdown getting set, and then does the normal Qt
// shutdown thing.
//

volatile bool fRequestShutdown = false;

void StartShutdown()
{
    fRequestShutdown = true;
}

bool ShutdownRequested()
{
    return fRequestShutdown;
}

class CCoinsViewErrorCatcher : public CCoinsViewBacked
{
public:
    CCoinsViewErrorCatcher(CCoinsView* view) : CCoinsViewBacked(view) {}
    bool GetCoins(const uint256 &txid, CCoins &coins) const {
        try {
            bool fResult = CCoinsViewBacked::GetCoins(txid, coins);
//            LogPrintf( "CCoinsViewErrorCatcher::GetCoins() for %s called CCoinsViewBacked::GetCoins(), which returned=%d, coins show IsPruned=%d\n", txid.ToString(), fResult, fResult ? coins.IsPruned() : 0 );
            return fResult;
        } catch(const std::runtime_error& e) {
            uiInterface.ThreadSafeMessageBox(_("Error reading from database, shutting down."), "", CClientUIInterface::MSG_ERROR);
            LogPrintf("Error reading from database: %s\n", e.what());
            // Starting the shutdown sequence and returning false to the caller would be
            // interpreted as 'entry not found' (as opposed to unable to read data), and
            // could lead to invalid interpration. Just exit immediately, as we can't
            // continue anyway, and all writes should be atomic.
            abort();
        }
    }
    // Writes do not need similar protection, as failure to write is handled by the caller.
};

static CCoinsViewDB *pcoinsdbview = NULL;
static CCoinsViewErrorCatcher *pcoinscatcher = NULL;

void Shutdown()
{
    LogPrintf("%s : In progress...\n", __func__);
    static CCriticalSection cs_Shutdown;
    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown)
        return;

    /// Note: Shutdown() must be able to handle cases in which AppInit2() failed part of the way,
    /// for example if the data directory was found to be locked.
    /// Be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    RenameThread("anoncoin-shutoff");
    mempool.AddTransactionsUpdated(1);
    StopRPCThreads();
#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdb.Flush(false);
    GenerateAnoncoins(false, NULL, 0);
#endif
    StopNode();
    UnregisterNodeSignals(GetNodeSignals());

    if (fFeeEstimatesInitialized)
    {
        boost::filesystem::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
        CAutoFile est_fileout(fopen(est_path.string().c_str(), "wb"), SER_DISK, CLIENT_VERSION);
        if (!est_fileout.IsNull())
            mempool.WriteFeeEstimates(est_fileout);
        else
            LogPrintf("%s : Failed to write fee estimates to %s\n", __func__, est_path.string());
        fFeeEstimatesInitialized = false;
    }
#ifdef ENABLE_I2PSAM
    if( IsI2PEnabled() ) I2PSession::Instance().stopForwardingAll();
#endif
    {
        LOCK(cs_main);
        if (pcoinsTip != NULL) {
            FlushStateToDisk();
        }
        delete pRetargetPid;
        pRetargetPid = NULL;
        delete pcoinsTip;
        pcoinsTip = NULL;
        delete pcoinscatcher;
        pcoinscatcher = NULL;
        delete pcoinsdbview;
        pcoinsdbview = NULL;
        delete pblocktree;
        pblocktree = NULL;
    }
#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdb.Flush(true);
#endif
#ifndef WIN32
    boost::filesystem::remove(GetPidFile());
#endif
    UnregisterAllValidationInterfaces();
#ifdef ENABLE_WALLET
    delete pwalletMain;
    pwalletMain = NULL;
#endif
    LogPrintf("%s : done\n", __func__);
}

/**
 * Signal handlers are very limited in what they are allowed to do, so:
 */
void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}

void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}

bool static InitError(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "Shutdown", CClientUIInterface::MSG_ERROR | CClientUIInterface::NOSHOWGUI);
    return false;
}

bool static InitWarning(const std::string &str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING | CClientUIInterface::NOSHOWGUI);
    return true;
}

bool static Bind(const CService &addr, unsigned int flags) {
    if (!(flags & BF_EXPLICIT) && IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError, (flags & BF_WHITELIST) != 0)) {
        if (flags & BF_REPORT_ERROR)
            return InitError(strError);
        return false;
    }
    return true;
}

void OnRPCStopped()
{
    cvBlockChange.notify_all();
    LogPrint("rpc", "RPC stopped.\n");
}

void OnRPCPreCommand(const CRPCCommand& cmd)
{
    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode", false) &&
        !cmd.okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);
}

// Core-specific options shared between UI, daemon and RPC client
std::string HelpMessage(HelpMessageMode hmm)
{
    string strUsage = _("Options:") + "\n";
    strUsage += "  -?                     " + _("This help message") + "\n";
    strUsage += "  -alertnotify=<cmd>     " + _("Execute command when a relevant alert is received or we see a really long fork (%s in cmd is replaced by message)") + "\n";
    strUsage += "  -blocknotify=<cmd>     " + _("Execute command when the best block changes (%s in cmd is replaced by the real block hash)") + "\n";
    strUsage += "  -checkblocks=<n>       " + strprintf(_("How many blocks to check at startup (default: %u, 0 = all)"), 980) + "\n";
    strUsage += "  -checklevel=<n>        " + strprintf(_("How thorough the block verification of -checkblocks is (0-4, default: %u)"), 3) + "\n";
    strUsage += "  -conf=<file>           " + strprintf(_("Specify configuration file (default: %s)"), "anoncoin.conf") + "\n";
    if (hmm == HMM_ANONCOIND)
    {
#if !defined(WIN32)
        strUsage += "  -daemon                " + _("Run in the background as a daemon and accept commands") + "\n";
#endif
    }
    strUsage += "  -datadir=<dir>         " + _("Specify data directory") + "\n";
    strUsage += "  -dbcache=<n>           " + strprintf(_("Set database cache size in megabytes (%d to %d, default: %d)"), nMinDbCache, nMaxDbCache, nDefaultDbCache) + "\n";
    strUsage += "  -loadblock=<file>      " + _("Imports blocks from external blk000??.dat file") + " " + _("on startup") + "\n";
    strUsage += "  -maxorphantx=<n>       " + strprintf(_("Keep at most <n> unconnectable transactions in memory (default: %u)"), DEFAULT_MAX_ORPHAN_TRANSACTIONS) + "\n";
    strUsage += "  -par=<n>               " + strprintf(_("Set the number of script verification threads (%u to %d, 0 = auto, <0 = leave that many cores free, default: %d)"), -(int)boost::thread::hardware_concurrency(), MAX_SCRIPTCHECK_THREADS, DEFAULT_SCRIPTCHECK_THREADS) + "\n";
#ifndef WIN32
    strUsage += "  -pid=<file>            " + strprintf(_("Specify pid file (default: %s)"), "anoncoind.pid") + "\n";
#endif
    strUsage += "  -reindex               " + _("Rebuild block chain index from current blk000??.dat files") + " " + _("on startup") + "\n";
    strUsage += "  -txindex               " + strprintf(_("Maintain a full transaction index, used by the getrawtransaction rpc call (default: %u)"), 1) + "\n";

    strUsage += "\n" + _("Connection options:") + "\n";
    strUsage += "  -addnode=<ip>          " + _("Add a node to connect to and attempt to keep the connection open") + "\n";
    strUsage += "  -banscore=<n>          " + strprintf(_("Threshold for disconnecting misbehaving peers (default: %u)"), 100) + "\n";
    strUsage += "  -bantime=<n>           " + strprintf(_("Number of seconds to keep misbehaving peers from reconnecting (default: %u)"), 86400) + "\n";
    strUsage += "  -bind=<addr>           " + _("Bind to given address and always listen on it. Use [host]:port notation for IPv6") + "\n";
    strUsage += "  -connect=<ip>          " + _("Connect only to the specified node(s)") + "\n";
    strUsage += "  -discover              " + _("Discover own IP address (default: 1 when listening and no -externalip)") + "\n";
    strUsage += "  -dns                   " + _("Allow DNS lookups for -addnode, -seednode and -connect") + " " + _("(default: 1)") + "\n";
    strUsage += "  -dnsseed               " + _("Query for peer addresses via DNS lookup, if low on addresses (default: 1 unless -connect)") + "\n";
    strUsage += "  -externalip=<ip>       " + _("Specify your own public address") + "\n";
    strUsage += "  -forcednsseed          " + strprintf(_("Always query for peer addresses via DNS lookup (default: %u)"), 0) + "\n";
    strUsage += "  -listen                " + _("Accept connections from outside (default: 1 if no -proxy or -connect)") + "\n";
    strUsage += "  -maxconnections=<n>    " + strprintf(_("Maintain at most <n> connections to peers (default: %u)"), 125) + "\n";
    strUsage += "  -maxreceivebuffer=<n>  " + strprintf(_("Maximum per-connection receive buffer, <n>*1000 bytes (default: %u)"), 5000) + "\n";
    strUsage += "  -maxsendbuffer=<n>     " + strprintf(_("Maximum per-connection send buffer, <n>*1000 bytes (default: %u)"), 1000) + "\n";
    strUsage += "  -onion=<ip:port>       " + strprintf(_("Use separate SOCKS5 proxy to reach peers via Tor hidden services (default: %s)"), "-proxy") + "\n";
    strUsage += "  -onlynet=<net>         " + _("Only connect to nodes in network <net> (ipv4, ipv6, onion, tor or i2p). Cumulative is allowed eg: onlynet=i2p onlynet=tor turn into darknet only mode.") + "\n";
    strUsage += "  -port=<port>           " + strprintf(_("Listen for connections on <port> (default: %u or testnet: %u)"), 9377, 19377) + "\n";
    strUsage += "  -proxy=<ip:port>       " + _("Connect through SOCKS5 proxy") + "\n";
    strUsage += "  -seednode=<ip>         " + _("Connect to a node to retrieve peer addresses, and disconnect") + "\n";
    strUsage += "  -timeout=<n>           " + strprintf(_("Specify connection timeout in milliseconds (minimum: 1, default: %d)"), DEFAULT_CONNECT_TIMEOUT) + "\n";
#ifdef USE_UPNP
#if USE_UPNP
    strUsage += "  -upnp                  " + _("Use UPnP to map the listening port (default: 1 when listening)") + "\n";
#else
    strUsage += "  -upnp                  " + strprintf(_("Use UPnP to map the listening port (default: %u)"), 0) + "\n";
#endif
#endif
    strUsage += "  -whitebind=<addr>      " + _("Bind to given address and whitelist peers connecting to it. Use [host]:port notation for IPv6") + "\n";
    strUsage += "  -whitelist=<netmask>   " + _("Whitelist peers connecting from the given netmask or IP address. Can be specified multiple times.") + "\n";
    strUsage += "                         " + _("Whitelisted peers cannot be DoS banned and their transactions are always relayed, even if they are already in the mempool, useful e.g. for a gateway") +

#ifdef ENABLE_WALLET
    strUsage += "\n" + _("Wallet options:") + "\n";
    strUsage += "  -disablewallet         " + _("Do not load the wallet and disable wallet RPC calls") + "\n";
    strUsage += "  -keypool=<n>           " + strprintf(_("Set key pool size to <n> (default: %u)"), 100) + "\n";
    if (GetBoolArg("-help-debug", false))
        strUsage += "  -mintxfee=<amt>        " + strprintf(_("Fees (in ANC/Kb) smaller than this are considered zero fee for transaction creation (default: %s)"), FormatMoney(w_minTxFeeRate.GetFeePerK())) + "\n";    strUsage += "  -paytxfee=<amt>        " + _("Fee per kB to add to transactions you send") + "\n";
    strUsage += "  -paytxfee=<amt>        " + strprintf(_("Fee (in ANC/kB) to add to transactions you send (default: %s)"), FormatMoney(payTxFee.GetFeePerK())) + "\n";
    strUsage += "  -rescan                " + _("Rescan the block chain for missing wallet transactions") + " " + _("on startup") + "\n";
    strUsage += "  -salvagewallet         " + _("Attempt to recover private keys from a corrupt wallet.dat") + " " + _("on startup") + "\n";
    strUsage += "  -sendfreetransactions  " + strprintf(_("Send transactions as zero-fee transactions if possible (default: %u)"), 0) + "\n";
    strUsage += "  -spendzeroconfchange   " + strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), 0) + "\n";
    strUsage += "  -txconfirmtarget=<n>   " + strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), 1) + "\n";
    strUsage += "  -maxtxfee=<amt>        " + strprintf(_("Maximum total fees to use in a single wallet transaction, setting too low may abort large transactions (default: %s)"), FormatMoney(maxTxFee)) + "\n";
    strUsage += "  -upgradewallet         " + _("Upgrade wallet to latest format") + " " + _("on startup") + "\n";
    strUsage += "  -wallet=<file>         " + _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), "wallet.dat") + "\n";
    strUsage += "  -walletnotify=<cmd>    " + _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)") + "\n";
    strUsage += "  -zapwallettxes         " + _("Clear list of wallet transactions (diagnostic tool; implies -rescan)") + "\n";
#endif

    strUsage += "\n" + _("Debugging/Testing options:") + "\n";
    if (GetBoolArg("-help-debug", false))
    {
        strUsage += "  -checkpoints           " + strprintf(_("Only accept block chain matching built-in checkpoints (default: %u)"), 1) + "\n";
        strUsage += "  -dblogsize=<n>         " + strprintf(_("Flush database activity from memory pool to disk log every <n> megabytes (default: %u)"), 100) + "\n";
        strUsage += "  -disablesafemode       " + strprintf(_("Disable safemode, override a real safe mode event (default: %u)"), 0) + "\n";
        strUsage += "  -testsafemode          " + strprintf(_("Force safe mode (default: %u)"), 0) + "\n";
        strUsage += "  -dropmessagestest=<n>  " + _("Randomly drop 1 of every <n> network messages") + "\n";
        strUsage += "  -fuzzmessagestest=<n>  " + _("Randomly fuzz 1 of every <n> network messages") + "\n";
        strUsage += "  -flushwallet           " + strprintf(_("Run a thread to flush wallet periodically (default: %u)"), 1) + "\n";
        strUsage += "  -stopafterblockimport  " + strprintf(_("Stop running after importing blocks from disk (default: %u)"), 0) + "\n";
    }
    strUsage += "  -debug=<category>      " + strprintf(_("Output debugging information (default: %u, supplying <category> is optional)"), 0) + "\n";
    strUsage += "                         " + _("If <category> is not supplied, output all debugging information.") + "\n";
    strUsage += "                         " + _("<category> can be:");
    strUsage +=                                 " addrman, alert, bench, coindb, db, lock, mempool, net, gui, rand, rpc, selectcoins, version"; // Don't translate these and qt below
    if (hmm == HMM_ANONCOIN_QT)
        strUsage += ", qt";
    strUsage += ".\n";
#ifdef ENABLE_WALLET
    strUsage += "  -gen                   " + _("Generate coins (default: 0)") + "\n";
    strUsage += "  -genproclimit=<n>      " + strprintf(_("Set the number of threads for coin generation if enabled (-1 = all cores, default: %d)"), 1) + "\n";
#endif
    strUsage += "  -help-debug            " + _("Show all debugging options (usage: --help -help-debug)") + "\n";
    strUsage += "  -logips                " + strprintf(_("Include IP addresses in debug output (default: %u)"), 0) + "\n";
    strUsage += "  -logtimestamps         " + strprintf(_("Prepend debug output with timestamp (default: %u)"), 1) + "\n";
    if (GetBoolArg("-help-debug", false))
    {
        strUsage += "  -limitfreerelay=<n>    " + strprintf(_("Continuously rate-limit free transactions to <n>*1000 bytes per minute (default:%u)"), 15) + "\n";
        strUsage += "  -relaypriority         " + strprintf(_("Require high priority for relaying free or low-fee transactions (default:%u)"), 1) + "\n";
        strUsage += "  -maxsigcachesize=<n>   " + strprintf(_("Limit size of signature cache to <n> entries (default: %u)"), 50000) + "\n";
    }
    strUsage += "  -minrelaytxfee=<amt>   " + strprintf(_("Fees (in ANC/Kb) smaller than this are considered zero fee for relaying (default: %s)"), FormatMoney(::minRelayTxFee.GetFeePerK())) + "\n";
    strUsage += "  -printtoconsole        " + _("Send trace/debug info to console instead of debug.log file") + "\n";
    if (GetBoolArg("-help-debug", false))
    {
        strUsage += "  -printblock=<hash>     " + _("Print block on startup, if found in block index") + "\n";
//        strUsage += "  -printblocktree        " + _("Print block tree on startup (default: 0)") + "\n";
        strUsage += "  -printpriority         " + strprintf(_("Log transaction priority and fee per kB when mining blocks (default: %u)"), 0) + "\n";
        strUsage += "  -privdb                " + strprintf(_("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)"), 1) + "\n";
        strUsage += "  -regtest               " + _("Enter regression test mode, which uses a special chain in which blocks can be solved instantly.") + "\n";
        strUsage += "                         " + _("This is intended for regression testing tools and app development.") + "\n";
        strUsage += "                         " + _("In this mode -genproclimit controls how many blocks are generated immediately.") + "\n";
    }
    strUsage += "  -shrinkdebugfile       " + _("Shrink debug.log file on client startup (default: 1 when no -debug)") + "\n";
    strUsage += "  -testnet               " + _("Use the test network") + "\n";

    strUsage += "\n" + _("Node relay options:") + "\n";
    strUsage += "  -datacarrier           " + strprintf(_("Relay and mine data carrier transactions (default: %u)"), 1) + "\n";
//    strUsage += "  -datacarriersize       " + strprintf(_("Maximum size of data in data carrier transactions we relay and mine (default: %u)"), MAX_OP_RETURN_RELAY) + "\n";

    strUsage += "\n" + _("Block creation options:") + "\n";
    strUsage += "  -blockminsize=<n>      " + strprintf(_("Set minimum block size in bytes (default: %u)"), 0) + "\n";
    strUsage += "  -blockmaxsize=<n>      " + strprintf(_("Set maximum block size in bytes (default: %d)"), DEFAULT_BLOCK_MAX_SIZE) + "\n";
    strUsage += "  -blockprioritysize=<n> " + strprintf(_("Set maximum size of high-priority/low-fee transactions in bytes (default: %d)"), DEFAULT_BLOCK_PRIORITY_SIZE) + "\n";

    strUsage += "\n" + _("RPC server options:") + "\n";
    strUsage += "  -server                " + _("Accept command line and JSON-RPC commands") + "\n";
    strUsage += "  -rest                  " + strprintf(_("Accept public REST requests (default: %u)"), 0) + "\n";
    strUsage += "  -rpcbind=<addr>        " + _("Bind to given address to listen for JSON-RPC connections. Use [host]:port notation for IPv6. This option can be specified multiple times (default: bind to all interfaces)") + "\n";
    strUsage += "  -rpcuser=<user>        " + _("Username for JSON-RPC connections") + "\n";
    strUsage += "  -rpcpassword=<pw>      " + _("Password for JSON-RPC connections") + "\n";
    strUsage += "  -rpcport=<port>        " + strprintf(_("Listen for JSON-RPC connections on <port> (default: %u or testnet: %u)"), 9376, 19376) + "\n";
    strUsage += "  -rpcallowip=<ip>       " + _("Allow JSON-RPC connections from specified source. Valid for <ip> are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24). This option can be specified multiple times") + "\n";
    strUsage += "  -rpcthreads=<n>        " + strprintf(_("Set the number of threads to service RPC calls (default: %d)"), 4) + "\n";
    strUsage += "  -rpckeepalive          " + strprintf(_("RPC support for HTTP persistent connections (default: %d)"), 1) + "\n";

    strUsage += "\n" + _("RPC SSL options: (see the Wiki for SSL setup instructions)") + "\n";
    strUsage += "  -rpcssl                                  " + _("Use OpenSSL (https) for JSON-RPC connections") + "\n";
    strUsage += "  -rpcsslcertificatechainfile=<file.cert>  " + strprintf(_("Server certificate file (default: %s)"), "server.cert") + "\n";
    strUsage += "  -rpcsslprivatekeyfile=<file.pem>         " + strprintf(_("Server private key (default: %s)"), "server.pem") + "\n";
    strUsage += "  -rpcsslciphers=<ciphers>                 " + strprintf(_("Acceptable ciphers (default: %s)"), "TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH") + "\n";

    strUsage += "\n"+ _("I2P Options:") + "\n";
    strUsage += "  -generatei2pdestination                         " + _("Generate an I2P destination, print it, save it to i2pkey.dat and exit.") + "\n";
    strUsage += "  -i2p.mydestination.privatekey=<private i2p key> " + _("Your full destination private keys. If it is not specified, the client will try to read i2pkey.dat for a permanent destination. If both fail the I2P wrapper will generate a random destination for you.") + "\n";
    strUsage += "  -i2p.options.enabled=1                          " + _("Enable I2P. This is also needed to call -generatei2pdestination.") + "\n";
    strUsage += "  -onlynet=i2p                                    " + _("Enable I2P only mode.") + "\n";
    strUsage += "  -i2p.mydestination.static=1                     " + _("To use an I2P permanent destination. If static=0 a dynamic destination will be generated even if a privatekey is found. Default=1.") + "\n";
    strUsage += "  -i2p.mydestination.shareaddr=1                  " + _("To share your I2P destination. Increase your network connectivity. Default=1.") + "\n";
    strUsage += "  -i2p.options.samhost=<ip or host name>          " + _("Address of the SAM bridge host. If it is not specified, value will be \"127.0.0.1\".") + "\n";
    strUsage += "  -i2p.options.samport=<port>                     " + _("Port number of the SAM bridge host. If it is not specified, value will be \"7656\".") + "\n";
    strUsage += "  -i2p.options.sessionname=<session name>         " + _("Name of an I2P session. If it is not specified, value will be \"Anoncoin-client\"") + "\n";

    return strUsage;
}

std::string LicenseInfo()
{
    return FormatParagraph(strprintf(_("Copyright (C) 2013-%i The Anoncoin Core Developers"), COPYRIGHT_YEAR)) + "\n" +
           "\n" +
           FormatParagraph(_("This is experimental software.")) + "\n" +
           "\n" +
           FormatParagraph(_("Distributed under the MIT software license, see the accompanying file COPYING or <http://www.opensource.org/licenses/mit-license.php>.")) + "\n" +
           "\n" +
           FormatParagraph(_("This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit <https://www.openssl.org/> and cryptographic software written by Eric Young and UPnP software written by Thomas Bernard.")) +
           "\n";
}

static void BlockNotifyCallback(const uint256& hashNewTip)
{
    std::string strCmd = GetArg("-blocknotify", "");

    boost::replace_all(strCmd, "%s", hashNewTip.GetHex());
    boost::thread t(runCommand, strCmd); // thread runs free
}

struct CImportingNow
{
    CImportingNow() {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow() {
        assert(fImporting == true);
        fImporting = false;
    }
};

void ThreadImport(std::vector<boost::filesystem::path> vImportFiles)
{
    RenameThread("anoncoin-loadblk");

    // -reindex
    if (fReindex) {
        CImportingNow imp;
        int nFile = 0;
        while (true) {
            CDiskBlockPos pos(nFile, 0);
            if (!boost::filesystem::exists(GetBlockPosFilename(pos, "blk")))
                break; // No block files left to reindex
            FILE *file = OpenBlockFile(pos, true);
            if (!file)
                break; // This error is logged in OpenBlockFile
            LogPrintf("Reindexing block file blk%05u.dat...\n", (unsigned int)nFile);
            LoadExternalBlockFile(file, &pos);
            nFile++;
        }
        pblocktree->WriteReindexing(false);
        fReindex = false;
        LogPrintf("Reindexing finished\n");
        // To avoid ending up in a situation without genesis block, re-try initializing (no-op if reindexing worked):
        InitBlockIndex();
    }

    // hardcoded $DATADIR/bootstrap.dat
    boost::filesystem::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (boost::filesystem::exists(pathBootstrap)) {
        FILE *file = fopen(pathBootstrap.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            boost::filesystem::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LogPrintf("Importing bootstrap.dat...\n");
            LoadExternalBlockFile(file);
            RenameOver(pathBootstrap, pathBootstrapOld);
        } else {
            LogPrintf("Warning: Could not open bootstrap file %s\n", pathBootstrap.string());
        }
    }

    // -loadblock=
    BOOST_FOREACH(boost::filesystem::path &path, vImportFiles) {
        FILE *file = fopen(path.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            LogPrintf("Importing blocks file %s...\n", path.string());
            LoadExternalBlockFile(file);
        } else {
            LogPrintf("Warning: Could not open blocks file %s\n", path.string());
        }
    }

    if (GetBoolArg("-stopafterblockimport", false)) {
        LogPrintf("Stopping after block import\n");
        StartShutdown();
    }
}

/** Sanity checks
 *  Ensure that Anoncoin is running in a usable environment with all
 *  necessary library support.
 */
bool InitSanityCheck(void)
{
    if(!ECC_InitSanityCheck()) {
        InitError("OpenSSL appears to lack support for elliptic curve cryptography. For more "
                  "information, visit https://en.bitcoin.it/wiki/OpenSSL_and_EC_Libraries");
        return false;
    }
    if (!glibc_sanity_test() || !glibcxx_sanity_test())
        return false;

    return true;
}

/** Initialize anoncoin.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2(boost::thread_group& threadGroup)
{
    // ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    // Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
    // A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
    // We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
    // which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL (WINAPI *PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol = (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != NULL) setProcDEPPol(PROCESS_DEP_ENABLE);

    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion ) != 2 || HIBYTE(wsadata.wVersion) != 2)
    {
        return InitError(strprintf("Error: Winsock library failed to start (WSAStartup returned error %d)", ret));
    }
#endif
#ifndef WIN32
    if (GetBoolArg("-sysperms", false)) {
#ifdef ENABLE_WALLET
        if (!GetBoolArg("-disablewallet", false))
            return InitError("Error: -sysperms is not allowed in combination with enabled wallet functionality");
#endif
    } else {
        umask(077);
    }

    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Reopen debug.log on SIGHUP
    struct sigaction sa_hup;
    sa_hup.sa_handler = HandleSIGHUP;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, NULL);

#if defined (__SVR4) && defined (__sun)
    // ignore SIGPIPE on Solaris
    signal(SIGPIPE, SIG_IGN);
#endif
#endif

    // ********************************************************* Step 2: parameter interactions
    // Set this early so that parameter interactions go to console
    fPrintToConsole = GetBoolArg("-printtoconsole", false);
    fLogTimestamps = GetBoolArg("-logtimestamps", true);
    fLogIPs = GetBoolArg("-logips", false);
    fLogI2Ps = GetBoolArg("-logi2ps", false);

    // LogPrintf( "\nAppInit2 : parameter interactions only tell you what other values will be assumed & required.\n" );
    // LogPrintf( "Any values you set on the command line or in a config file will override those suggestions.\n" );

    if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect or -proxy is specified
        if (SoftSetBoolArg("-listen", true))
            LogPrintf("AppInit2 : parameter interaction: -bind or -whitebind set -> setting -listen=1\n");
    }

    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        if (SoftSetBoolArg("-dnsseed", false))
            LogPrintf("AppInit2 : parameter interaction: -connect set -> setting -dnsseed=0\n");
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("AppInit2 : parameter interaction: -connect set -> setting -listen=0\n");
    }

    if (mapArgs.count("-proxy")) {
        // to protect privacy, do not listen by default if a default proxy server is specified
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("AppInit2 : parameter interaction: -proxy set -> setting -listen=0\n");
        // to protect privacy, do not discover addresses by default
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -proxy set -> setting -discover=0\n");
    }

    if (!GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        if (SoftSetBoolArg("-upnp", false))
            LogPrintf("AppInit2 : parameter interaction: -listen=0 -> setting -upnp=0\n");
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -listen=0 -> setting -discover=0\n");
    }

    if (mapArgs.count("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -externalip set -> setting -discover=0\n");
    }

    if (GetBoolArg("-salvagewallet", false)) {
        // Rewrite just private keys: rescan to find transactions
        if (SoftSetBoolArg("-rescan", true))
            LogPrintf("AppInit2 : parameter interaction: -salvagewallet=1 -> setting -rescan=1\n");
    }

    // -zapwallettx implies a rescan
    if (GetBoolArg("-zapwallettxes", false)) {
        if (SoftSetBoolArg("-rescan", true))
            LogPrintf("AppInit2 : parameter interaction: -zapwallettxes=1 -> setting -rescan=1\n");
    }

    // Make sure enough file descriptors are available
    int nBind = std::max((int)mapArgs.count("-bind") + (int)mapArgs.count("-whitebind"), 1);
    nMaxConnections = GetArg("-maxconnections", 125);
    nMaxConnections = std::max(std::min(nMaxConnections, (int)(FD_SETSIZE - nBind - MIN_CORE_FILEDESCRIPTORS)), 0);
    int nFD = RaiseFileDescriptorLimit(nMaxConnections + MIN_CORE_FILEDESCRIPTORS);
    if (nFD < MIN_CORE_FILEDESCRIPTORS)
        return InitError(_("Not enough file descriptors available."));
    if (nFD - MIN_CORE_FILEDESCRIPTORS < nMaxConnections)
        nMaxConnections = nFD - MIN_CORE_FILEDESCRIPTORS;

    // ********************************************************* Step 3: parameter-to-internal-flags
    bool fTestNet = GetBoolArg("-testnet", false);
    if (fTestNet)
    {
        // TESTNET
        CashIsKing::ANCConsensus::nDifficultySwitchHeight6 = 110;
        LogPrintf("-- Consensus TESTNET - Switch at 110\n");
    }

    ancConsensus.bShouldDebugLogPoW = GetBoolArg("-extrapowdebug", false);

    fDebug = !mapMultiArgs["-debug"].empty();
    // Special-case: if -debug=0/-nodebug is set, turn off debugging messages
    const vector<string>& categories = mapMultiArgs["-debug"];
    if (GetBoolArg("-nodebug", false) || find(categories.begin(), categories.end(), string("0")) != categories.end())
        fDebug = false;

    // Check for -debugnet
    if (GetBoolArg("-debugnet", false))
        InitWarning(_("Warning: Unsupported argument -debugnet ignored, use -debug=net."));
    // Check for -socks - as this is a privacy risk to continue, exit here
    if (mapArgs.count("-socks"))
        return InitError(_("Error: Unsupported argument -socks found. Setting SOCKS version isn't possible anymore, only SOCKS5 proxies are supported."));
    // Check for -tor - as this is a privacy risk to continue, exit here
    if (GetBoolArg("-tor", false))
        return InitError(_("Error: Unsupported argument -tor found, use -onion."));

    if (GetBoolArg("-benchmark", false))
        InitWarning(_("Warning: Unsupported argument -benchmark ignored, use -debug=bench."));

    // Checkmempool and checkblockindex default to true in regtest mode
    mempool.setSanityCheck(GetBoolArg("-checkmempool", RegTest()));
    fCheckBlockIndex = GetBoolArg("-checkblockindex", RegTest());
    Checkpoints::fEnabled = GetBoolArg("-checkpoints", true);

    // -par=0 means autodetect, but nScriptCheckThreads==0 means no concurrency
    nScriptCheckThreads = GetArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    if (nScriptCheckThreads <= 0)
        nScriptCheckThreads += boost::thread::hardware_concurrency();
    if (nScriptCheckThreads <= 1)
        nScriptCheckThreads = 0;
    else if (nScriptCheckThreads > MAX_SCRIPTCHECK_THREADS)
        nScriptCheckThreads = MAX_SCRIPTCHECK_THREADS;

    fServer = GetBoolArg("-server", false);
    setvbuf(stdout, NULL, _IOLBF, 0);
#ifdef ENABLE_WALLET
    bool fDisableWallet = GetBoolArg("-disablewallet", false);
#endif

    nConnectTimeout = GetArg("-timeout", DEFAULT_CONNECT_TIMEOUT);
    if (nConnectTimeout <= 0)
        nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;

    // Continue to put "/P2SH/" in the coinbase to monitor
    // BIP16 support.
    // This can be removed eventually...
    const char* pszP2SH = "/P2SH/";
    COINBASE_FLAGS << std::vector<unsigned char>(pszP2SH, pszP2SH+strlen(pszP2SH));

    // Fee-per-kilobyte amount considered the same as "free"
    // If you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    if (mapArgs.count("-minrelaytxfee"))
    {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-minrelaytxfee"], n) && n > 0)
            ::minRelayTxFee = CFeeRate(n);
        else
            return InitError(strprintf(_("Invalid amount for -minrelaytxfee=<amount>: '%s'"), mapArgs["-minrelaytxfee"]));
        tx_nMinRelayTxFee = n;       // Critical global value, allows many v9 routines to run properly until v10 upgrade is done (esp QT)
    }

#ifdef ENABLE_WALLET
    if (mapArgs.count("-mintxfee"))
    {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-mintxfee"], n) && n > 0)
            w_minTxFeeRate = CFeeRate(n);
        else
            return InitError(strprintf(_("Invalid amount for -mintxfee=<amount>: '%s'"), mapArgs["-mintxfee"]));
        tx_nMinTxFee = n;            // Critical global value, allows many v9 routines to run properly until v10 upgrade is done (esp QT)
    }
    if (mapArgs.count("-paytxfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(mapArgs["-paytxfee"], nFeePerK))
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), mapArgs["-paytxfee"]));
        if (nFeePerK > nHighTransactionFeeWarning)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                       mapArgs["-paytxfee"], ::minRelayTxFee.ToString()));
        }
        nTransactionFee = nFeePerK;         // Critical global value, allows many v9 routines to run properly until v10 upgrade is done (esp QT)
    }
    if (mapArgs.count("-maxtxfee"))
    {
        CAmount nMaxFee = 0;
        if (!ParseMoney(mapArgs["-maxtxfee"], nMaxFee))
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s'"), mapArgs["-maptxfee"]));
        if (nMaxFee > nHighTransactionMaxFeeWarning)
            InitWarning(_("Warning: -maxtxfee is set very high! Fees this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee)
        {
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                       mapArgs["-maxtxfee"], ::minRelayTxFee.ToString()));
        }
    }
    nTxConfirmTarget = GetArg("-txconfirmtarget", 1);
    bSpendZeroConfChange = GetArg("-spendzeroconfchange", false);
    fSendFreeTransactions = GetArg("-sendfreetransactions", false);

    std::string strWalletFile = GetArg("-wallet", "wallet.dat");
#endif // ENABLE_WALLET

    fIsBareMultisigStd = GetArg("-permitbaremultisig", true) != 0;
    // ToDo:
    // nMaxDatacarrierBytes = GetArg("-datacarriersize", nMaxDatacarrierBytes);

    // ********************************************************* Step 4: application initialization: dir lock, daemonize, pidfile, debug log

    // Sanity check
    if (!InitSanityCheck())
        return InitError(_("Initialization sanity check failed. Anoncoin Core is shutting down."));

    std::string strDataDir = GetDataDir().string();
#ifdef ENABLE_WALLET
    // Wallet file must be a plain filename without a directory
    if (strWalletFile != boost::filesystem::basename(strWalletFile) + boost::filesystem::extension(strWalletFile))
        return InitError(strprintf(_("Wallet %s resides outside data directory %s"), strWalletFile, strDataDir));
#endif
    // Make sure only a single Anoncoin process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
    if (!lock.try_lock())
        return InitError(strprintf(_("Cannot obtain a lock on data directory %s. Anoncoin Core is probably already running."), strDataDir));
#ifndef WIN32
    CreatePidFile(GetPidFile(), getpid());
#endif
    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    LogPrintf("Anoncoin version %s (%s)\n", FormatFullVersion(), CLIENT_DATE);
    LogPrintf("Using OpenSSL version %s\n", SSLeay_version(SSLEAY_VERSION));
#ifdef ENABLE_WALLET
    LogPrintf("Using BerkeleyDB version %s\n", DbEnv::version(0, 0, 0));
#endif
    if (!fLogTimestamps)
        LogPrintf("Startup time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()));
    LogPrintf("Default data directory %s\n", GetDefaultDataDir().string());
    LogPrintf("Using data directory %s\n", strDataDir);
    LogPrintf("Using config file %s\n", GetConfigFile().string());
    LogPrintf("Using at most %i connections (%i file descriptors available)\n", nMaxConnections, nFD);
    std::ostringstream strErrors;
#ifdef ENABLE_I2PSAM
    if( !GetBoolArg("-stfu", false) && !IsBehindDarknet() )
        InitWarning( _("Anoncoin is running on clearnet!") );
#endif
#if defined(USE_SSE2)
    scrypt_detect_sse2();
#endif
    LogPrintf("Using %u threads for script verification\n", nScriptCheckThreads);
    if (nScriptCheckThreads) {
        for (int i=0; i<nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
    }

    /**
     * Start the RPC server already.  It will be started in "warmup" mode
     * and not really process calls already (but it will signify connections
     * that the server is there and will be ready later).  Warmup mode will
     * be disabled when initialization is finished.
     */
    if (fServer)
    {
        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        RPCServer::OnStopped(&OnRPCStopped);
        RPCServer::OnPreCommand(&OnRPCPreCommand);
        StartRPCThreads();
    }

    int64_t nStart;

    // ********************************************************* Step 5: verify wallet database integrity
#ifdef ENABLE_WALLET
    if (fDisableWallet) {
        pwalletMain = NULL;
        LogPrintf("Wallet disabled!\n");
    } else {
        LogPrintf("Using wallet %s\n", strWalletFile);
        uiInterface.InitMessage(_("Verifying wallet..."));

        if (!bitdb.Open(GetDataDir()))
        {
            // try moving the database env out of the way
            boost::filesystem::path pathDatabase = GetDataDir() / "database";
            boost::filesystem::path pathDatabaseBak = GetDataDir() / strprintf("database.%d.bak", GetTime());
            try {
                boost::filesystem::rename(pathDatabase, pathDatabaseBak);
                LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string(), pathDatabaseBak.string());
            } catch(boost::filesystem::filesystem_error &error) {
                 // failure is ok (well, not really, but it's not worse than what we started with)
            }

            // try again
            if (!bitdb.Open(GetDataDir())) {
                // if it still fails, it probably means we can't even create the database env
                string msg = strprintf(_("Error initializing wallet database environment %s!"), strDataDir);
                return InitError(msg);
            }
        }

        if (GetBoolArg("-salvagewallet", false))
        {
            // Recover readable keypairs:
            if (!CWalletDB::Recover(bitdb, strWalletFile, true))
                return false;
        }

        if (boost::filesystem::exists(GetDataDir() / strWalletFile))
        {
            CDBEnv::VerifyResult r = bitdb.Verify(strWalletFile, CWalletDB::Recover);
            if (r == CDBEnv::RECOVER_OK)
            {
                string msg = strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                         " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                         " your balance or transactions are incorrect you should"
                                         " restore from a backup."), strDataDir);
                InitWarning(msg);
            }
            if (r == CDBEnv::RECOVER_FAIL)
                return InitError(_("wallet.dat corrupt, salvage failed"));
        }
    }



#endif // ENABLE_WALLET
    // ********************************************************* Step 6: network initialization

    RegisterNodeSignals(GetNodeSignals());

#ifdef ENABLE_I2PSAM
    // No config setup, means the i2p enable parameter gets set false, for the 1st time.
    // We need it to have been created and set for sure, in order to process -onlynet options for this node.
    if( SoftSetBoolArg("-i2p.options.enabled", false) )         // Returns true if the param was undefined, in that case it will be created and value you requested assigned.
        LogPrintf("AppInit2 : required parameter: -i2p.options.enabled=0 -> unless specifically set to 1, it is assumed no I2P router is available.\n");
    // At this point we can now know the parameter has been defined and has a value, now we need to fetch its 'real' value
    bool fI2pEnabled = GetBoolArg("-i2p.options.enabled", false);
#else
    if( !SoftSetBoolArg("-i2p.options.enabled", false) )
        LogPrintf("AppInit2 : invalid parameter: -i2p.options.enabled=0 -> hard set.  This build does not support an I2P router.\n");
    HardSetBoolArg( "-i2p.options.enabled", false );
    bool fI2pEnabled = false;
#endif // ENABLE_I2PSAM

    if (mapArgs.count("-onlynet")) {
        std::set<enum Network> nets;
        BOOST_FOREACH(std::string snet, mapMultiArgs["-onlynet"]) {
            enum Network net = ParseNetwork(snet);
            if( net == NET_I2P ) {
                if( fI2pEnabled ) {
                    // Who knows what the user wants, we'll assume they have nothing set for this
                    // and disable upnp, turn listening and discovery off.  The rest of our code
                    // now does not care what these settings are and should work with any values.
                    // for I2P only and no clearnet interaction with the outside IP world is a good start.
#ifdef USE_UPNP
                    if( SoftSetBoolArg("-upnp", false) )
                        LogPrintf("AppInit2 : parameter interaction: -onlynet=i2p -> setting -upnp=0\n");
#endif
                    if( SoftSetBoolArg("-listen",false) )
                        LogPrintf("AppInit2 : parameter interaction: -onlynet=i2p -> setting -listen=0\n");
                    if( SoftSetBoolArg("-discover",false) )
                        LogPrintf("AppInit2 : parameter interaction: -onlynet=i2p -> setting -discover=0\n");
                } else {
                    LogPrintf("AppInit2 : You can not use onlynet=i2p, without first having warmed up the i2p router and enabled it.\n");
                    return InitError( _("Set enabled=1 in the [i2p.options] section of your anoncoin.conf file to use onlynet=i2p") );
                }
            }
            if (net == NET_UNROUTABLE)
                return InitError(strprintf(_("Unknown network specified in -onlynet: '%s'"), snet));
            nets.insert(net);
        }
        // When onlynet has been specified, all other network types, not specified become limited
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetLimited(net);
        }

        // At this point, if the I2P network has been limited out, we need to override & disable the i2p enabled parameter, if it was enabled.
        if( fI2pEnabled && !nets.count(NET_I2P) ) {
            LogPrintf("AppInit2 : parameter interaction: -onlynet != i2p while -i2p.options.enabled=1 -> hard setting -i2p.options.enabled=0\n");
            fI2pEnabled = false;
            HardSetBoolArg( "-i2p.options.enabled", false );
        }
    }
    // If we made it this far, and i2p is not enabled, but the limited flag is still showing as not set, we need to make SURE that it is cleared!
    // Really important, otherwise the software will start trying i2p addresses to connect with.  Later on, if a router session is created
    // we set the reachability flag based on the results of trying to create that new session.
    if( !fI2pEnabled && !IsLimited( NET_I2P ) )
        SetLimited( NET_I2P );

    if (mapArgs.count("-whitelist")) {
        BOOST_FOREACH(const std::string& net, mapMultiArgs["-whitelist"]) {
            CSubNet subnet(net);
            if (!subnet.IsValid())
                return InitError(strprintf(_("Invalid netmask specified in -whitelist: '%s'"), net));
            CNode::AddWhitelistedRange(subnet);
        }
    }

    CService addrProxy;
    bool fProxy = false;
    if (mapArgs.count("-proxy")) {
        addrProxy = CService(mapArgs["-proxy"], 9050);
        if (!addrProxy.IsValid())
            return InitError(strprintf(_("Invalid -proxy address: '%s'"), mapArgs["-proxy"]));

        if (!IsLimited(NET_IPV4))
            SetProxy(NET_IPV4, addrProxy);
        if (!IsLimited(NET_IPV6))
            SetProxy(NET_IPV6, addrProxy);
        SetNameProxy(addrProxy);
        fProxy = true;
    }

    // -onion can override normal proxy, -noonion disables tor entirely
    if (!(mapArgs.count("-onion") && mapArgs["-onion"] == "0") &&
        (fProxy || mapArgs.count("-onion"))) {
        CService addrOnion;
        if (!mapArgs.count("-onion"))
            addrOnion = addrProxy;
        else
            addrOnion = CService(mapArgs["-onion"], 9050);
        if (!addrOnion.IsValid())
            return InitError(strprintf(_("Invalid -onion address: '%s'"), mapArgs["-onion"]));
        SetProxy(NET_TOR, addrOnion);
        SetReachable(NET_TOR);
    }

#ifdef ENABLE_I2PSAM
    // All we need to do for -generatei2pdestination, is have 2 other parameters set correctly and we will automatically create a new dynamic destination,
    // while executing the normal code, near the end of the initialization cycle, after a session opened (if possible), printout the key results values,
    // and gracefully exit the program.
    // To make it easier on the user, we hard set static off, if it has been left on in the configuration file.
    bool fGenI2pDest = GetBoolArg("-generatei2pdestination", false);
    if( fGenI2pDest ) {                                             // Hard set these 2 values
        if( SoftSetBoolArg("-i2p.mydestination.static", false) )    // Returns true if the param was undefined and setting its value was possible
            LogPrintf( "AppInit2 : parameter interaction: -generatei2pdestination -> setting -i2p.mydestination.static=0\n");
        else {
            HardSetBoolArg("-i2p.mydestination.static", false);
            LogPrintf( "AppInit2 : parameter interaction: -generatei2pdestination -> hard setting -i2p.mydestination.static=0\n");
        }

        // At this point if the user has the correct configuration set, we can continue, just one more detail to check, and error out if its not setup correctly.
        if( !fI2pEnabled )  {
            LogPrintf( "AppInit2 : To use -generatei2pdestination, the i2p router must be warmed up. Include [i2p.options] enabled=1 in your anoncoin.conf,\n" );
            LogPrintf( "           any [i2p.mydestination] static= value will be hard set internally to static=0, while running this command.\n" );
            LogPrintf( "AppInit2 : Another option, without any configuration file at all, you can add -i2p.options.enabled=1 on the command line,\n" );
            LogPrintf( "         : the I2P SAM module will try to create a session with default values, to access the i2p router.\n" );
            return InitError(_("Unable to run -generatei2pdestination, see the debug.log for possible solutions to fix the problem." ) );
        }

    }

    // Initialize some stuff here a early, so the values are available later on, if i2p is  enabled or not, GenI2pDest is run etc...
    bool fI2pSessionValid = false;
    bool fI2pStaticDest;
    SAM::FullDestination myI2pKeys;
    string b32doti2p;


    // We still need the keys and b32.i2p address information setup for anoncoin-qt, if any details exist in the config file
    // use them, if not create the Args and set them to null strings.
    if( SoftSetBoolArg("-i2p.mydestination.static", true) )    // Returns true if the param was undefined and setting its value was possible
        LogPrintf( "AppInit2 : required parameter: -i2p.mydestination.static=1 -> setting defined.\n");
    fI2pStaticDest = GetBoolArg("-i2p.mydestination.static", true);   // Now we can get a local copy of whatever the real value is set to

    if( SoftSetArg("-i2p.mydestination.privatekey", "") ){           // Returns true if the param was undefined and setting its value was possible
        LogPrintf("I2P mydestination privatekey in anoncoin.conf is undefined");
        boost::filesystem::path pathI2PKeydat = GetDataDir() / "i2pkey.dat";
            if (boost::filesystem::exists(pathI2PKeydat)) {
                FILE *file = fopen(pathI2PKeydat.string().c_str(), "r");
                if (fscanf(file, "%s",I2PKeydat) == 1)  //read the I2PKeydat from the file i2pkey.dat
                {   fclose(file);
                    LogPrintf("... I2P privatekey read from file i2pkey.dat\n");
                    myI2pKeys.priv = I2PKeydat;
                    fI2pStaticDest = true;
                }
            } else {
                LogPrintf("... and there is no file i2pkey.dat present.\n");
                LogPrintf( "AppInit2 : required parameter: -i2p.mydestination.privatekey= -> setting defined and set to <null>.\n");
                myI2pKeys.priv = GetArg("-i2p.mydestination.privatekey", ""); // myI2pKeys.priv will be NULL in this case, preparing for DYN
            }
        } else {
        LogPrintf("I2P mydestination privatekey in anoncoin.conf is defined, we do not need to check for file i2pkey.dat\n");
        myI2pKeys.priv = GetArg("-i2p.mydestination.privatekey", ""); // Now we can get a local copy of whatever the real value is set to
    }

    // Now we can validate the privkey, if it's a non-zero string we can use it to set the public and b32.i2p addresses for this node as well.
    // ToDo: We could do a more exhaustive test on the key, to confirm that it is correct
    // This needs to be done with or without I2pEnabled, because Anoncoin-qt will display whatever is set under settings, if there
    // is something, we want the information available to the user.
    if( myI2pKeys.priv.size() > NATIVE_I2P_DESTINATION_SIZE && isValidI2pAddress(myI2pKeys.priv) ) {
        myI2pKeys.pub = myI2pKeys.priv.substr(0, NATIVE_I2P_DESTINATION_SIZE);
        b32doti2p = B32AddressFromDestination(myI2pKeys.pub);
         // Its not an error to have static false, even though the private key is valid
    } else if( fI2pStaticDest ) {                // Dam, static is wrong, this will never work, we must hard set it to false and continue dynamic
        if( myI2pKeys.priv.size() ) {            // Only do this if the private key was set to junk
            LogPrintf( "AppInit2 : parameter error: -i2p.mydestination.privatekey= -> key is not valid, hard setting the value to <null>.\n");
            HardSetArg( "-i2p.mydestination.privatekey", "" );
            myI2pKeys.priv = "";
        }
        // note: the public and b32doti2p values defaulted to null strings when initialized above, which is what we want, only the static is wrong
        LogPrintf( "AppInit2 : parameter error: -i2p.mydestination.privatekey invalid -> hard setting -i2p.mydestination.static=0\n");
        HardSetBoolArg("-i2p.mydestination.static", false);
        fI2pStaticDest = false;
    }

    // Previous v9 builds may have public/b32.i2p addresses left in their configuration file, now we need to hard set those values and make sure
    // they match the 'real' private key value we would use if starting a new router session.
    if( !SoftSetArg("-i2p.mydestination.publickey", myI2pKeys.pub) ) {
        LogPrintf( "AppInit2 : PLEASE REMOVE -i2p.mydestination.publickey= FROM YOUR CONFIGURATION FILE - It is no longer set by the user.\n" );
        LogPrintf( "AppInit2 : parameter interaction: -i2p.mydestination.privatekey -> hard setting -i2p.mydestination.publickey=%s\n", myI2pKeys.pub);
        HardSetArg("-i2p.mydestination.publickey", myI2pKeys.pub);
    }
    // Now do the same for the base32key parameter
    if( !SoftSetArg("-i2p.mydestination.base32key", b32doti2p) ) {
        LogPrintf( "AppInit2 : PLEASE REMOVE -i2p.mydestination.base32key= FROM YOUR CONFIGURATION FILE - It is no longer set by the user.\n" );
        LogPrintf( "AppInit2 : parameter interaction: -i2p.mydestination.privatekey -> hard setting -i2p.mydestination.base32key=%s\n", b32doti2p);
        HardSetArg("-i2p.mydestination.base32key", b32doti2p);
    }

    if( fI2pEnabled ) {                                                 // If I2P is enabled, we have allot of work to do...
        // Does the user want to have a shared destination?
        // If we're not running i2p enabled, having it undefined is ok too, defaults to same as static setting here though,
        // as it will be used as soon as an i2p node is created in an outbound connection, or upon processing a version message from an inbound connection.
        //if( SoftSetBoolArg("-i2p.mydestination.shareaddr", fI2pStaticDest) )
          //  LogPrintf( "AppInit2 : parameter interaction: -i2p.mydestination.static -> setting -i2p.mydestination.shareaddr=%s\n", fI2pStaticDest ? "1" : "0" );
        //CSlave changed to allow sharing of every I2P address whether dynamic and static per default

        if( SoftSetBoolArg("-i2p.mydestination.shareaddr", true) )
            LogPrintf( "AppInit2 : parameter interaction: -i2p.mydestination.static -> setting -i2p.mydestination.shareaddr=1\n");

        // bool fI2pSharedAddr = GetBoolArg("-i2p.mydestination.shareaddr", false);

        // Many more settings to do, moved them into i2pwrapper.cpp, we make sure all our parameters are loaded into
        // configuration space, set to default and logged if any parameter interaction was required.
        // Here now we pass the critical value for our destination field to the be used for opening the session,
        // its special if we are not using a static i2p destination
        myI2pKeys.isGenerated = !fI2pStaticDest;
        InitializeI2pSettings( myI2pKeys.isGenerated );

        // Finally ready to inform the user we're about to start the connection
        uiInterface.InitMessage(_("Connecting to the I2P Router..."));

        LogPrintf( "AppInit2 : Attempting to create an I2P SAM session..." );
        // This creates the session for the 1st time, and tries to open the i2psam socket and say hello, if that fails, we're done.
        if( !I2PSession::Instance().isSick() ) {
            // Now we can either use a static destination address, taken from anoncoin.conf values to create a Stream Session, or
            // generate a dynamic new one and initiate an I2P session stream that way...
            if( fI2pStaticDest ) {          // If everything was setup correctly we can try running static mode
                LogPrintf( "With a static destination.\n" );
                SAM::FullDestination retI2pKeys;                                   // Something we can us to compare our results with
                retI2pKeys = I2PSession::Instance().getMyDestination();
            LogPrintf("Running with an I2P mydestination publickey: %s\n", myI2pKeys.pub);

                if( retI2pKeys.priv == myI2pKeys.priv && retI2pKeys.pub == myI2pKeys.pub && retI2pKeys.isGenerated == false ) {
                    myI2pKeys = retI2pKeys;             // Store them outside this one step, so they can be used later
                    fI2pSessionValid = true;
                } else
                    LogPrintf( "AppInit2 : Error - Router Destination does not match the static Destination set here.  Result: ShutDown.\n" );
            } else {                                                // Generate new destination keys/address
                LogPrintf( "With a dynamic destination.\n" );
                myI2pKeys = I2PSession::Instance().getMyDestination();
                if( isValidI2pAddress(myI2pKeys.priv) && isValidI2pAddress(myI2pKeys.pub) && myI2pKeys.isGenerated == true )
                    fI2pSessionValid = true;
                else
                    LogPrintf( "AppInit2 : Error - Unable to generate a valid I2P destination.  Result: ShutDown.\n" );
            }
        } else
            LogPrintf( "Failed.  Router does not appear to be available\n" );

        if( fI2pSessionValid ) {
            b32doti2p = B32AddressFromDestination(myI2pKeys.pub);
            // At this point, we could try to figure out if this hardset on the parameters is really needed or not,
            // instead we just always do it, and make sure all the values are always what we are using for this
            // session.  After this point, they are all just primarily used for informational purposes, still they
            // should not be wrong and we can set them.
            HardSetArg( "-i2p.mydestination.privatekey", myI2pKeys.priv );
            HardSetArg( "-i2p.mydestination.publickey", myI2pKeys.pub );
            HardSetArg( "-i2p.mydestination.base32key", b32doti2p );

            SetReachable(NET_I2P);                           // It's now been proven the router is available.
            LogPrintf( "AppInit2 : Your I2P Keys have been set. Using SAM module version %s\n", FormatI2PNativeFullVersion());
            LogPrintf( "AppInit2 : Created a new SAM Session ID:%s, connected to Router.\n", I2PSession::Instance().getSessionID() );
            // This is not needed, unless debuggging...as the b32.i2p address will be printed in the log shortly while binding
            // if( !fGenI2pDest )                   // Unless we're just about to give all the details away anyway, lets tell the user what we've done in the debug.log file
                // LogPrintf( "AppInit2 : Your I2P Destination for this session will be:\ni2p.mydestination.publickey=[%s]\ni2p.mydestination.base32key=%s\n", myI2pKeys.pub, b32doti2p );
        } else {
            SetLimited( NET_I2P );                           // Don't use any i2p information
            // Do NOT hard set the i2p options enabled flag false, without first looking at the shutdown process, make sure it will still work.
            // HardSetBoolArg("-i2p.options.enabled", false);
            // We're wiped out, bail and exit initialization in failure
            return InitError( _("Unable to create I2P SAM session") );
        }
    } // else ...I2P was not enabled, atm we have nothing more that need be done, except deal with GenI2pDest

    if( fGenI2pDest ) {
        if( fI2pSessionValid ) {
            boost::filesystem::path pathI2PKeydat = GetDataDir() / "i2pkey.dat";
            FILE* file = fopen(pathI2PKeydat.string().c_str(), "a");           //create the i2pkey.dat as the write below will not create it anew
            if (file) fclose(file);
            if (boost::filesystem::exists(pathI2PKeydat)) {
                FILE *file = fopen(pathI2PKeydat.string().c_str(), "w+");
                fprintf(file, "%s\n",myI2pKeys.priv.c_str());                 //write the I2PKeydat to the file i2pkey.dat
                fclose(file);
            }
            string msg = GenerateI2pDestinationMessage( myI2pKeys.pub, myI2pKeys.priv, b32doti2p, GetConfigFile().string() );
            unsigned int style = CClientUIInterface::ICON_INFORMATION |
                                 CClientUIInterface::NOSHOWGUI |
                                 CClientUIInterface::BTN_APPLY |
                                 CClientUIInterface::BTN_ABORT |
                                 CClientUIInterface::MODAL;
            bool fResult = uiInterface.ThreadSafeMessageBox(msg, _("Generated I2P Destination"), style );
            // LogPrintf( "MessageBox returned %s\n", fResult ? "true" : "false" );
            if( !fResult )
                return false;
            // This way anoncoind always shuts down, as noui_ThreadSafeMessageBox returns false,
            // for the anoncoin-qt user, they can continue if they want to, by selecting the BTN_APPLY button.
        } else
            return InitError(_("Unable to obtain I2P SAM Session for the -generatei2pdestination command") );
    }   // fGenI2pDest
#endif  // ENABLE_I2PSAM


    // see Step 2: parameter interactions for more information about these
    fListen = GetBoolArg("-listen", DEFAULT_LISTEN);
    fDiscover = GetBoolArg("-discover", true);
    fNameLookup = GetBoolArg("-dns", true);

    bool fBound = false;
#ifdef ENABLE_I2PSAM
    // Regardless of users choice on binding, listening, discover or dns,
    // if the I2P session is valid, we always try to bind our node & accept
    // inbound peer connections to it over i2p
    if( fI2pSessionValid )
        fBound = BindListenNativeI2P();
#endif
    if (fListen) {
        if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
            BOOST_FOREACH(std::string strBind, mapMultiArgs["-bind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                    return InitError(strprintf(_("Cannot resolve -bind address: '%s'"), strBind));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR));
            }
            BOOST_FOREACH(std::string strBind, mapMultiArgs["-whitebind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, 0, false))
                    return InitError(strprintf(_("Cannot resolve -whitebind address: '%s'"), strBind));
                if (addrBind.GetPort() == 0)
                    return InitError(strprintf(_("Need to specify a port with -whitebind: '%s'"), strBind));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR | BF_WHITELIST));
            }
        }
#ifdef ENABLE_I2PSAM
        // If the user selected any binds above, they will be done, otherwise no bind was explicitly
        // given by the user, so the default is to bind with any ipv6 and ipv4 address the system can
        // support, UNLESS we are already bound to I2P for listening, and don't WANT any clearnet binds
        // then we do not execute this code.
        else if( !IsI2POnly() ) {
#else                               // original code
        else {
#endif
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
            fBound |= Bind(CService(in6addr_any, GetListenPort()), BF_NONE);
            fBound |= Bind(CService(inaddr_any, GetListenPort()), !fBound ? BF_REPORT_ERROR : BF_NONE);
        }
        if (!fBound)
            return InitError(_("Failed to listen on any port. Use -listen=0 if you want this."));
    }

    if (mapArgs.count("-externalip")) {
        BOOST_FOREACH(string strAddr, mapMultiArgs["-externalip"]) {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }

    BOOST_FOREACH(string strDest, mapMultiArgs["-seednode"])
        AddOneShot(strDest);

    //! ********************************************************* Step 7: load block chain
    //!
    //! Final value selection for Anoncoin retarget controller P-I and D terms are set here
#define PID_PROPORTIONALGAIN "1.7"
#define PID_INTEGRATORTIME "172800"
#define PID_INTEGRATORGAIN "5"
#define PID_DERIVATIVEGAIN "0"


    double dProportionalGainIn; //! The Proportional gain of the control loop
    int64_t nIntegrationTimeIn; //! The Integration period in seconds.
    double dIntegratorGainIn;   //! The Integration gain of the control loop
    double dDerivativeGainIn;   //! The Derivative gain of the control loop
#if defined( HARDFORK_BLOCK )
    //! Before we do any block details, create a retargetpid so we can calculate next-work-required
    //! Testnet requires the user to be able to change some settings. so here we handle that too.
    if( isMainNetwork() ) {
    dProportionalGainIn = boost::lexical_cast<float>( PID_PROPORTIONALGAIN );
    nIntegrationTimeIn = boost::lexical_cast<int>( PID_INTEGRATORTIME );
    dIntegratorGainIn = boost::lexical_cast<float>( PID_INTEGRATORGAIN );
    dDerivativeGainIn = boost::lexical_cast<float>( PID_DERIVATIVEGAIN );
    } else {
    std::string dProportionalGainInGetArg = GetArg("-retargetpid.proportionalgain", PID_PROPORTIONALGAIN );
    dProportionalGainIn = boost::lexical_cast<float>( dProportionalGainInGetArg );
    std::string nIntegrationTimeInGetArg = GetArg("-retargetpid.integrationtime", PID_INTEGRATORTIME );
    nIntegrationTimeIn = boost::lexical_cast<int>( nIntegrationTimeInGetArg );
    std::string dIntegratorGainInGetArg = GetArg("-retargetpid.integratorgain", PID_INTEGRATORGAIN );
    dIntegratorGainIn = boost::lexical_cast<float>( dIntegratorGainInGetArg );
    std::string dDerivativeGainInGetArg = GetArg("-retargetpid.derivativegain", PID_DERIVATIVEGAIN );
    dDerivativeGainIn = boost::lexical_cast<float>( dDerivativeGainInGetArg );

    LogPrint("retarget", "dProportionalGainInGetArg=%s and dProportionalGainIn=%f and PID_PROPORTIONALGAIN=%f\n",dProportionalGainInGetArg.c_str(), dProportionalGainIn, PID_PROPORTIONALGAIN);
    LogPrint("retarget", "nIntegrationTimeInGetArg=%s and nIntegrationTimeIn=%d and PID_INTEGRATORTIME=%f\n",nIntegrationTimeInGetArg.c_str(), nIntegrationTimeIn, PID_INTEGRATORTIME);
    LogPrint("retarget", "dIntegratorGainInGetArg=%s and dIntegratorGainIn=%f and PID_INTEGRATORGAIN=%f\n",dIntegratorGainInGetArg.c_str(), dIntegratorGainIn, PID_INTEGRATORGAIN);
    LogPrint("retarget", "dDerivativeGainInGetArg=%s and dDerivativeGainIn=%f and PID_DERIVATIVEGAIN=%f\n",dDerivativeGainInGetArg.c_str(), dDerivativeGainIn, PID_DERIVATIVEGAIN);
    }
#else
    // Before the hardfork build, we allow programmable settings on both mainnet and testnets
    // CSlave: Here "boost::lexical_cast<float>" is used instead of "atof"; and "boost::lexical_cast<int>" is used instead of "atoi"
    // for otherwise it did not read the dot spaced decimal value for PID settings correctly in anoncoin.conf, and truncated them
    // at the dot on certain system that use the comma as a separator in regional settings.

    std::string dProportionalGainInGetArg = GetArg("-retargetpid.proportionalgain", PID_PROPORTIONALGAIN );
    dProportionalGainIn = boost::lexical_cast<float>( dProportionalGainInGetArg );
    std::string nIntegrationTimeInGetArg = GetArg("-retargetpid.integrationtime", PID_INTEGRATORTIME );
    nIntegrationTimeIn = boost::lexical_cast<int>( nIntegrationTimeInGetArg );
    std::string dIntegratorGainInGetArg = GetArg("-retargetpid.integratorgain", PID_INTEGRATORGAIN );
    dIntegratorGainIn = boost::lexical_cast<float>( dIntegratorGainInGetArg );
    std::string dDerivativeGainInGetArg = GetArg("-retargetpid.derivativegain", PID_DERIVATIVEGAIN );
    dDerivativeGainIn = boost::lexical_cast<float>( dDerivativeGainInGetArg );

    LogPrint("retarget", "dProportionalGainInGetArg=%s and dProportionalGainIn=%f and PID_PROPORTIONALGAIN=%f\n",dProportionalGainInGetArg.c_str(), dProportionalGainIn, PID_PROPORTIONALGAIN);
    LogPrint("retarget", "nIntegrationTimeInGetArg=%s and nIntegrationTimeIn=%d and PID_INTEGRATORTIME=%f\n",nIntegrationTimeInGetArg.c_str(), nIntegrationTimeIn, PID_INTEGRATORTIME);
    LogPrint("retarget", "dIntegratorGainInGetArg=%s and dIntegratorGainIn=%f and PID_INTEGRATORGAIN=%f\n",dIntegratorGainInGetArg.c_str(), dIntegratorGainIn, PID_INTEGRATORGAIN);
    LogPrint("retarget", "dDerivativeGainInGetArg=%s and dDerivativeGainIn=%f and PID_DERIVATIVEGAIN=%f\n",dDerivativeGainInGetArg.c_str(), dDerivativeGainIn, PID_DERIVATIVEGAIN);

#endif
    pRetargetPid = new CRetargetPidController( dProportionalGainIn, nIntegrationTimeIn, dIntegratorGainIn, dDerivativeGainIn );

    fReindex = GetBoolArg("-reindex", false);

    //! While upgrading older versions, hard-links to the old blknnnn.dat files can be used.
    //! This indexes them into valid /blocks/
    boost::filesystem::path blocksDir = GetDataDir() / "blocks";
    if (!boost::filesystem::exists(blocksDir))
    {
        boost::filesystem::create_directories(blocksDir);
        bool linked = false;
        for (unsigned int i = 1; i < 10000; i++) {
            boost::filesystem::path source = GetDataDir() / strprintf("blk%04u.dat", i);
            if (!boost::filesystem::exists(source)) break;
            boost::filesystem::path dest = blocksDir / strprintf("blk%05u.dat", i-1);
            try {
                boost::filesystem::create_hard_link(source, dest);
                LogPrintf("Hardlinked %s -> %s\n", source.string(), dest.string());
                linked = true;
            } catch (boost::filesystem::filesystem_error & e) {
                // Note: hardlink creation failing is not a disaster, it just means
                // blocks will get re-downloaded from peers.
                LogPrintf("Error hardlinking blk%04u.dat : %s\n", i, e.what());
                break;
            }
        }
        if (linked)
        {
            fReindex = true;
        }
    }

    // cache size calculations
    int64_t nTotalCache = (GetArg("-dbcache", nDefaultDbCache) << 20);
    nTotalCache = std::max(nTotalCache, nMinDbCache << 20); // total cache cannot be less than nMinDbCache
    nTotalCache = std::min(nTotalCache, nMaxDbCache << 20); // total cache cannot be greater than nMaxDbcache
    int64_t nBlockTreeDBCache = nTotalCache / 8;
    if (nBlockTreeDBCache > (1 << 21) && !GetBoolArg("-txindex", true))
        nBlockTreeDBCache = (1 << 21); // block tree db cache shouldn't be larger than 2 MiB
    nTotalCache -= nBlockTreeDBCache;
    int64_t nCoinDBCache = nTotalCache / 2; // use half of the remaining cache for coindb cache
    nTotalCache -= nCoinDBCache;
    nCoinCacheSize = nTotalCache / 300; // coins in memory require around 300 bytes

    bool fLoaded = false;
    while (!fLoaded) {
        bool fReset = fReindex;
        std::string strLoadError;

        uiInterface.InitMessage(_("Loading block index..."));

        nStart = GetTimeMillis();
        do {
            try {
                UnloadBlockIndex();
                delete pcoinsTip;
                delete pcoinsdbview;
                delete pcoinscatcher;
                delete pblocktree;

                pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReindex);
                pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReindex);
                pcoinscatcher = new CCoinsViewErrorCatcher(pcoinsdbview);
                pcoinsTip = new CCoinsViewCache(pcoinscatcher);

                if (fReindex)
                    pblocktree->WriteReindexing(true);

                if (!LoadBlockIndex()) {
                    strLoadError = _("Error loading block database");
                    break;
                }

                // If the loaded chain has a wrong genesis, bail out immediately
                // (we're likely using a testnet datadir, or the other way around).
                if (!mapBlockIndex.empty() && mapBlockIndex.count(Params().HashGenesisBlock()) == 0)
                    return InitError(_("Incorrect or no genesis block found. Wrong datadir for network?"));

                // Initialize the block index (no-op if non-empty database was already loaded)
                if (!InitBlockIndex()) {
                    strLoadError = _("Error initializing block database");
                    break;
                }

                // Check for changed -txindex state
                if (fTxIndex != GetBoolArg("-txindex", false)) {
                    strLoadError = _("You need to rebuild the database using -reindex to change -txindex");
                    break;
                }

                uiInterface.InitMessage(_("Verifying latest blocks..."));
                if (!VerifyDB(GetArg("-checklevel", 3),
                              GetArg("-checkblocks", 980))) {
                    strLoadError = _("Corrupted block database detected");
                    break;
                }
            } catch(std::exception &e) {
                if (fDebug) LogPrintf("%s\n", e.what());
                strLoadError = _("Error opening block database");
                break;
            }

            fLoaded = true;
        } while(false);

        if (!fLoaded) {
            // first suggest a reindex
            if (!fReset) {
                bool fRet = uiInterface.ThreadSafeMessageBox(
                    strLoadError + ".\n\n" + _("Do you want to rebuild the block database now?"),
                    "", CClientUIInterface::MSG_ERROR | CClientUIInterface::BTN_ABORT);
                if (fRet) {
                    fReindex = true;
                    fRequestShutdown = false;
                } else {
                    LogPrintf("Aborted block database rebuild. Exiting.\n");
                    return false;
                }
            } else {
                return InitError(strLoadError);
            }
        }
    }

    // As LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill the GUI during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown)
    {
        LogPrintf("Shutdown requested. Exiting.\n");
        return false;
    }
    LogPrintf(" block index %15dms\n", GetTimeMillis() - nStart);

    boost::filesystem::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
    CAutoFile est_filein(fopen(est_path.string().c_str(), "rb"), SER_DISK, CLIENT_VERSION);
    // Allowed to fail as this file IS missing on first startup.
    if (!est_filein.IsNull())
        mempool.ReadFeeEstimates(est_filein);
    fFeeEstimatesInitialized = true;

#if defined( DONT_COMPILE )
    if (GetBoolArg("-printblockindex", false) || GetBoolArg("-printblocktree", false))
    {
        PrintBlockTree();
        return false;
    }
#endif
    if (mapArgs.count("-printblock"))
    {
        string strMatch = mapArgs["-printblock"];
        int nFound = 0;
        for (BlockMap::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = (*mi).second;
                CBlock block;
                ReadBlockFromDisk(block, pindex);
                block.BuildMerkleTree();
                LogPrintf("%s\n", block.ToString() );
                nFound++;
            }
        }
        if (nFound == 0)
            LogPrintf("No blocks matching %s were found\n", strMatch);
        return false;
    }

    // ********************************************************* Step 8: load wallet
#ifdef ENABLE_WALLET
    if (fDisableWallet) {
        pwalletMain = NULL;
        LogPrintf("Wallet disabled!\n");
    } else {
        // needed to restore wallet transaction meta data after -zapwallettxes
        std::vector<CWalletTx> vWtx;

        if (GetBoolArg("-zapwallettxes", false)) {
            uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

            pwalletMain = new CWallet(strWalletFile);
            DBErrors nZapWalletRet = pwalletMain->ZapWalletTx(vWtx);
            if (nZapWalletRet != DB_LOAD_OK) {
                uiInterface.InitMessage(_("Error loading wallet.dat: Wallet corrupted"));
                return false;
            }

            delete pwalletMain;
            pwalletMain = NULL;
        }

        uiInterface.InitMessage(_("Loading wallet..."));

        nStart = GetTimeMillis();
        bool fFirstRun = true;
        pwalletMain = new CWallet(strWalletFile);
        DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
        if (nLoadWalletRet != DB_LOAD_OK)
        {
            if (nLoadWalletRet == DB_CORRUPT)
                strErrors << _("Error loading wallet.dat: Wallet corrupted") << "\n";
            else if (nLoadWalletRet == DB_NONCRITICAL_ERROR)
            {
                string msg(_("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                             " or address book entries might be missing or incorrect."));
                InitWarning(msg);
            }
            else if (nLoadWalletRet == DB_TOO_NEW)
                strErrors << _("Error loading wallet.dat: Wallet requires newer version of Anoncoin Core") << "\n";
            else if (nLoadWalletRet == DB_NEED_REWRITE)
            {
                strErrors << _("Wallet needed to be rewritten: restart Anoncoin Core to complete") << "\n";
                LogPrintf("%s", strErrors.str());
                return InitError(strErrors.str());
            }
            else
                strErrors << _("Error loading wallet.dat") << "\n";
        }

        if (GetBoolArg("-upgradewallet", fFirstRun))
        {
            int nMaxVersion = GetArg("-upgradewallet", 0);
            if (nMaxVersion == 0) // the -upgradewallet without argument case
            {
                LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
                nMaxVersion = CLIENT_VERSION;
                pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
            }
            else
                LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
            if (nMaxVersion < pwalletMain->GetVersion())
                strErrors << _("Cannot downgrade wallet") << "\n";
            pwalletMain->SetMaxVersion(nMaxVersion);
        }

        if (fFirstRun)
        {
            // Create new keyUser and set as default key
            RandAddSeedPerfmon();

            CPubKey newDefaultKey;
            if (pwalletMain->GetKeyFromPool(newDefaultKey)) {
                pwalletMain->SetDefaultKey(newDefaultKey);
                if (!pwalletMain->SetAddressBook(pwalletMain->vchDefaultKey.GetID(), "", "receive"))
                    strErrors << _("Cannot write default address") << "\n";
            }

            pwalletMain->SetBestChain(chainActive.GetLocator());
        }

        LogPrintf("%s", strErrors.str());
        LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

        RegisterValidationInterface(pwalletMain);

        CBlockIndex *pindexRescan = chainActive.Tip();
        if (GetBoolArg("-rescan", false))
            pindexRescan = chainActive.Genesis();
        else
        {
            CWalletDB walletdb(strWalletFile);
            CBlockLocator locator;
            if (walletdb.ReadBestBlock(locator))
                pindexRescan = FindForkInGlobalIndex(chainActive, locator);
            else
                pindexRescan = chainActive.Genesis();
        }
        if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
        {
            uiInterface.InitMessage(_("Rescanning..."));
            LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
            nStart = GetTimeMillis();
            pwalletMain->ScanForWalletTransactions(pindexRescan, true);
            LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
            pwalletMain->SetBestChain(chainActive.GetLocator());
            nWalletDBUpdated++;

            // Restore wallet transaction metadata after -zapwallettxes=1
            if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2")
            {
                CWalletDB walletdb(strWalletFile);

                BOOST_FOREACH(const CWalletTx& wtxOld, vWtx)
                {
                    uint256 hash = wtxOld.GetHash();
                    std::map<uint256, CWalletTx>::iterator mi = pwalletMain->mapWallet.find(hash);
                    if (mi != pwalletMain->mapWallet.end())
                    {
                        const CWalletTx* copyFrom = &wtxOld;
                        CWalletTx* copyTo = &mi->second;
                        copyTo->mapValue = copyFrom->mapValue;
                        copyTo->vOrderForm = copyFrom->vOrderForm;
                        copyTo->nTimeReceived = copyFrom->nTimeReceived;
                        copyTo->nTimeSmart = copyFrom->nTimeSmart;
                        copyTo->fFromMe = copyFrom->fFromMe;
                        copyTo->strFromAccount = copyFrom->strFromAccount;
                        copyTo->nOrderPos = copyFrom->nOrderPos;
                        copyTo->WriteToDisk(&walletdb);
                    }
                }
            }
        }
    } // (!fDisableWallet)
#else // ENABLE_WALLET
    LogPrintf("No wallet compiled in!\n");
#endif // !ENABLE_WALLET
    // ********************************************************* Step 9: import blocks

    if (mapArgs.count("-blocknotify"))
        uiInterface.NotifyBlockTip.connect(BlockNotifyCallback);

    // scan for better chains in the block chain database, that are not yet connected in the active best chain
    CValidationState state;
    if (!ActivateBestChain(state))
        strErrors << "Failed to connect best block";

    std::vector<boost::filesystem::path> vImportFiles;
    if (mapArgs.count("-loadblock"))
    {
        BOOST_FOREACH(string strFile, mapMultiArgs["-loadblock"])
            vImportFiles.push_back(strFile);
    }
    threadGroup.create_thread(boost::bind(&ThreadImport, vImportFiles));
    if (chainActive.Tip() == NULL) {
        LogPrintf("Waiting for genesis block to be imported...\n");
        while (!fRequestShutdown && chainActive.Tip() == NULL)
            MilliSleep(10);
    }

    // ********************************************************* Step 10: start node

    if (!CheckDiskSpace())
        return false;

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

    RandAddSeedPerfmon();

    //// debug print
    LogPrintf("mapBlockIndex.size() = %u\n",   mapBlockIndex.size());
    LogPrintf("nBestHeight = %d\n",                   chainActive.Height());
#ifdef ENABLE_WALLET
    LogPrintf("setKeyPool.size() = %u\n",      pwalletMain ? pwalletMain->setKeyPool.size() : 0);
    LogPrintf("mapWallet.size() = %u\n",       pwalletMain ? pwalletMain->mapWallet.size() : 0);
    LogPrintf("mapAddressBook.size() = %u\n",  pwalletMain ? pwalletMain->mapAddressBook.size() : 0);
#endif

    StartNode(threadGroup);

#ifdef ENABLE_WALLET
    // Generate coins in the background
    if (pwalletMain && !fGenerateInitialTestNetState )
        GenerateAnoncoins(GetBoolArg("-gen", false), pwalletMain, GetArg("-genproclimit", -1));
#endif

    // ********************************************************* Step 11: finished
    // Even before this message makes it into the log, we can be accepting inbound connections from other
    // peers, so make sure everything has been done and setup correctly during your initialization before
    // those version messages start getting processed!
    SetRPCWarmupFinished();                         // Let rpc clients in too...
    uiInterface.InitMessage(_("Done loading"));

#ifdef ENABLE_WALLET
    if (pwalletMain) {
        // Add wallet transactions that aren't already in a block to mapTransactions
        pwalletMain->ReacceptWalletTransactions();

        // Run a thread to flush wallet periodically
        threadGroup.create_thread(boost::bind(&ThreadFlushWalletDB, boost::ref(pwalletMain->strWalletFile)));
    }
#endif

    if( fGenerateInitialTestNetState && !fRequestShutdown ) {
        // Run it in a separate thread
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "testnetinit", &InitializeTestNetBlocks));
        fGenerateInitialTestNetState = false;
    }

    return !fRequestShutdown;
}
