// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h"

#include "addrman.h"
#include "clientversion.h"
#include "chainparams.h"
#include "transaction.h"
#include "ui_interface.h"

#ifdef ENABLE_I2PSAM
#include "i2pwrapper.h"
#endif

#ifdef WIN32
#include <string.h>
#else
#include <fcntl.h>
#endif

#ifdef USE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

// Dump addresses to peers.dat every 15 minutes (900s)
#define DUMP_ADDRESSES_INTERVAL 900

#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

// Fix for ancient MinGW versions, that don't have defined these in ws2tcpip.h.
// Todo: Can be removed when our pull-tester is upgraded to a modern MinGW version.
#ifdef WIN32
#ifndef PROTECTION_LEVEL_UNRESTRICTED
#define PROTECTION_LEVEL_UNRESTRICTED 10
#endif
#ifndef IPV6_PROTECTION_LEVEL
#define IPV6_PROTECTION_LEVEL 23
#endif
#endif

using namespace std;

//! Only used for the initial protocol version during a node object creation, increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 209;

//! Constants found in this source codes header(.h)
/** Time between pings automatically sent out for latency probing and keepalive (in seconds). */
const int PING_INTERVAL = 2 * 60;
/** Time after which to disconnect, after waiting for a ping response (or inactivity). */
const int TIMEOUT_INTERVAL = 20 * 60;
/** The maximum number of entries in an 'inv' protocol message */
const unsigned int MAX_INV_SZ = 50000;
/** The maximum number of new addresses to accumulate before announcing. */
const unsigned int MAX_ADDR_TO_SEND = 1000;
/** Maximum length of incoming protocol messages (no message over 2 MiB is currently acceptable). */
const unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 2 * 1024 * 1024;
/** -listen default */
const bool DEFAULT_LISTEN = true;
/** -upnp default */
#ifdef USE_UPNP
const bool DEFAULT_UPNP = USE_UPNP;
#else
const bool DEFAULT_UPNP = false;
#endif
/** The maximum number of entries in mapAskFor */
const size_t MAPASKFOR_MAX_SZ = MAX_INV_SZ;

namespace {
    const int MAX_OUTBOUND_CONNECTIONS = 16;

    struct ListenSocket {
        SOCKET socket;
        bool whitelisted;

        ListenSocket(SOCKET socket, bool whitelisted) : socket(socket), whitelisted(whitelisted) {}
    };
}

//
// Global state variables
//
bool fDiscover = true;
bool fListen = true;

/**
 * Default nLocalServices Definition:
 *
 * Starting with protocol 70008 the nLocalServices NODE_I2P bit is always set as the standard we support for our address space
 * model.
 * Anoncoin Core will send/receive to all peers with a greatly enlarged address/object.
 * NODE_I2P indicates that this node is available for exchanging those i2p addresses, by enlarging the CNetAddr data structure.
 * Over 546 bytes/address verses ~30 for clearnet only IP's.   It's critical to understand how this effects the message exchange
 * we conduct with peers, either on clearnet or over the i2p network.  Over i2p its easy, we always exchange them and NODE_I2P
 * must be set, or we can not handshake destinations.
 * Clearnet is more complicated.  Something we do not know until after the version message has been exchanged, so we start out
 * by exchanging only IP addresses.  If the peer in question also supports NODE_I2P, we switch stream types to our fully supported
 * address space model.  The purpose of that service bit in for cases where they do not support i2p destinations, and we do not
 * wish to make it mandatory for Anoncoin.  So, the one case where we do not switch our streamtype is for over clearnet and for
 * a peer that does not have NODE_I2P set.  Our software then reverts to exchanging in the classical sense, only the [ip] portion
 * of our address space model.  In this manner, we can still connect, exchange blocks, transactions and even IP addresses with
 * those peers.
 * Allowing for innovation and development work of many different types of products and services is good for Anoncoin, even
 * though a peice of software may not initially be able to support the i2p network, that could change that with success and
 * determination.
 * Starting with the release of protocol 70009 we make many past issues and problem go away, our software now is deterministic
 * in how it behaves with addresses over clearnet.  Peers that can not support NODE_I2P are welcome, but have limited access to
 * our full and more secure ongoing network operations.
 *
 * NODE_NETWORK and NODE_BLOOM service bits.  Consensus seems to be forming that NODE_BLOOM can be assumed if NODE_NETWORK is
 * defined as true, so we have turned it off as well.  Newer services like NODE_PREFIX appear to be planned for.  We support
 * NODE_BLOOM, but no longer have the* bit turned on.  NODE_NETWORK is on because we have a full copy of the blockchain and can
 * support requests for blocks as has always been the case. Apparently some see bloom filters as a possible source for attacks,
 * we need to add this ToDo: as a decision the team makes together as to if we want to keep the code in place or remove it.
 */
static bool vfReachable[NET_MAX] = {};
static bool vfLimited[NET_MAX] = {};
static CNode* pnodeLocalHost = NULL;
static std::vector<ListenSocket> vhListenSocket;

uint64_t nLocalServices = NODE_NETWORK | NODE_I2P; // Add the I2P protocol(.h) bit to our local services list
CCriticalSection cs_mapLocalHost;
map<CNetAddr, LocalServiceInfo> mapLocalHost;
uint64_t nLocalHostNonce = 0;
// CAddrMan addrman; is moved to netbase.cpp so it can be used for b32.i2p address lookup
int nMaxConnections = 125;
bool fAddressesInitialized = false;

#ifdef ENABLE_I2PSAM
/**
 * For i2p we do use real OS sockets, created for us after accept has been issued to the I2P SAM module.
 * Initially we have only one, and know exactly what our destination address is.
 * As soon as initialized we immediately start 'accepting' other nodes that want to talk through it to us,
 * as well as advertise it frequently to other peers that we are connected to via the I2P network.
 * This differs greatly from the IP world and how the rest of the code here works for clearnet. vhI2PListenSocket is currently
 * used to initialize a new socket for another inbound peer, after the destination address has been received, but before
 * we actually start talking with read/write of data to it.  This is from the v8 design era, and plans are in the
 * works to remove it in the future, it is not really needed.
 */
static std::vector<SOCKET> vhI2PListenSocket;                   // We maintain a separate vector for I2P network SOCKET's
#endif

vector<CNode*> vNodes;
CCriticalSection cs_vNodes;
map<CInv, CDataStream> mapRelay;
deque<pair<int64_t, CInv> > vRelayExpiration;
CCriticalSection cs_mapRelay;
limitedmap<CInv, int64_t> mapAlreadyAskedFor(MAX_INV_SZ);

static deque<string> vOneShots;
CCriticalSection cs_vOneShots;

set<CNetAddr> setservAddNodeAddresses;
CCriticalSection cs_setservAddNodeAddresses;

vector<std::string> vAddedNodes;
CCriticalSection cs_vAddedNodes;

NodeId nLastNodeId = 0;
CCriticalSection cs_nLastNodeId;

static CSemaphore *semOutbound = NULL;

// Signals for message handling
static CNodeSignals n_signals;
CNodeSignals& GetNodeSignals() { return n_signals; }

void AddOneShot(string strDest)
{
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort()
{
    return (unsigned short)(GetArg("-port", Params().GetDefaultPort()));
}

// find 'best' local address for a particular peer
bool GetLocal(CService& addr, const CNetAddr *paddrPeer)
{
    // LogPrintf( "Finding best local address for %s\n", paddrPeer->ToString() );
    if( !paddrPeer->IsI2P() && !fListen )
        return false;

    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (map<CNetAddr, LocalServiceInfo>::iterator it = mapLocalHost.begin(); it != mapLocalHost.end(); it++)
        {
            int nScore = (*it).second.nScore;
            int nReachability = (*it).first.GetReachabilityFrom(paddrPeer);

            // LogPrintf( "Reachability from %s is %d with a score of %d\n", (*it).first.ToString(), nReachability, nScore );
            if (nReachability > nBestReachability || (nReachability == nBestReachability && nScore > nBestScore))
            {
                addr = CService((*it).first, (*it).second.nPort);
                // addr.print();
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return nBestScore >= 0;
}

// get best local address for a particular peer as a CAddress
// Otherwise, return the unroutable 0.0.0.0 but filled in with
// the normal parameters, since the IP may be changed to a useful
// one by discovery.
// If the peer is I2P, GetLocal() returns true as long as the i2p
// router has been enabled and warmed up, so we have a local i2p
// destination set. otherwise it could be false.  ToDo: We may
// want the CAddress object to be ALL zeros in that case, although
// the code seems unnecessary atm to this developer GR.
CAddress GetLocalAddress(const CNetAddr *paddrPeer)
{
    CAddress ret(CService("0.0.0.0",GetListenPort()),0);
    CService addr;
    if( (!paddrPeer->IsI2P() || IsMyDestinationShared()) && GetLocal(addr, paddrPeer) )
    {
        ret = CAddress(addr);
    }
    ret.nServices = nLocalServices;
    ret.nTime = GetAdjustedTime();
    if( paddrPeer->IsI2P() ) ret.SetPort(0);      //! For i2p destinations, the port should always be zero
    return ret;
}

bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    while (true)
    {
        char c;
        int nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0)
        {
            if (c == '\n')
                continue;
            if (c == '\r')
                return true;
            strLine += c;
            if (strLine.size() >= 9000)
                return true;
        }
        else if (nBytes <= 0)
        {
            boost::this_thread::interruption_point();
            if (nBytes < 0)
            {
                int nErr = WSAGetLastError();
                if (nErr == WSAEMSGSIZE)
                    continue;
                if (nErr == WSAEWOULDBLOCK || nErr == WSAEINTR || nErr == WSAEINPROGRESS)
                {
                    MilliSleep(10);
                    continue;
                }
            }
            if (!strLine.empty())
                return true;
            if (nBytes == 0)
            {
                // socket closed
                LogPrint("net", "socket closed\n");
                return false;
            }
            else
            {
                // socket error
                int nErr = WSAGetLastError();
                LogPrint("net", "recv failed: %s\n", NetworkErrorString(nErr));
                return false;
            }
        }
    }
}

int GetnScore(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    if (mapLocalHost.count(addr) == LOCAL_NONE)
        return 0;
    return mapLocalHost[addr].nScore;
}

// Is our peer's addrLocal potentially useful as an external IP source?
bool IsPeerAddrLocalGood(CNode *pnode)
{
    return fDiscover && pnode->addr.IsRoutable() && pnode->addrLocal.IsRoutable() &&
           !IsLimited(pnode->addrLocal.GetNetwork());
}

// pushes our own address to a peer
void AdvertizeLocal(CNode *pnode)
{
    if (pnode->fSuccessfullyConnected)
    {
        // If its an i2p destination more than likely the node has the correct destination,
        // even if it was one we created dynamically, or the connection could not be maintained.
        // If it is dynamic, we still do not want to advertize it.  If its a clearnet peer,
        // we don't advertize it if we are not listening.
        // And finally it does have to be routable before we push it
        if( !pnode->addr.IsI2P() && fListen ) {
            CAddress addrLocal = GetLocalAddress(&pnode->addr);                 // Compute the best local address for this peer.
            // If discovery is enabled, sometimes give our peer the address it
            // tells us that it sees us as in case it has a better idea of our
            // address than we do.  If this node is on clearnet
            if( !pnode->addr.IsI2P() ) {
                if (IsPeerAddrLocalGood(pnode) && (!addrLocal.IsRoutable() ||
                     GetRand((GetnScore(addrLocal) > LOCAL_MANUAL) ? 8:2) == 0))
                {
                    addrLocal.SetIP(pnode->addrLocal);
                }
            }
            if( addrLocal.IsRoutable() )
            {
                pnode->PushAddress(addrLocal);
            }
        }
    }
}

// used when scores of local addresses may have changed
// pushes better local address to peers
void AdvertizeLocal()
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        AdvertizeLocal( pnode );
}

void SetReachable(enum Network net, bool fFlag)
{
    LOCK(cs_mapLocalHost);
    vfReachable[net] = fFlag;
    if (net == NET_IPV6 && fFlag)
        vfReachable[NET_IPV4] = true;
}

// learn a new local address
bool AddLocal(const CService& addr, int nScore)
{
    if (!addr.IsRoutable())
        return false;
        // { LogPrintf( "Failed to AddLocal Reason 1\n" ); return false; }

    if( !addr.IsI2P() && !fDiscover && nScore < LOCAL_MANUAL )
        return false;
        // { LogPrintf( "Failed to AddLocal Reason 2\n" ); return false; }

    if ( IsLimited(addr))
        return false;
        // { LogPrintf( "Failed to AddLocal Reason 3\n" ); return false; }

    {
        LOCK(cs_mapLocalHost);
        bool fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo &info = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort = addr.GetPort();
        }
        SetReachable(addr.GetNetwork());
        if( !fAlready )
            LogPrintf( "AddLocal(%s,%i)\n", addr.ToString(), nScore );
    }

    return true;
}

bool AddLocal(const CNetAddr &addr, int nScore)
{
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

/** Make a particular network entirely off-limits (no automatic connects to it) */
void SetLimited(enum Network net, bool fLimited)
{
    if (net == NET_UNROUTABLE)
        return;
    LOCK(cs_mapLocalHost);
    vfLimited[net] = fLimited;
}

bool IsLimited(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfLimited[net];
}

bool IsLimited(const CNetAddr &addr)
{
    return IsLimited(addr.GetNetwork());
}

/** vote for a local address */
bool SeenLocal(const CService& addr)
{
    {
        LOCK(cs_mapLocalHost);
        if (mapLocalHost.count(addr) == 0)
            return false;
        mapLocalHost[addr].nScore++;
    }
    return true;
}

/** check whether a given address is potentially local */
bool IsLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

/** check whether a given network is one we can probably connect to */
bool IsReachable(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfReachable[net] && !vfLimited[net];
}

/** check whether a given address is in a network we can probably connect to */
bool IsReachable(const CNetAddr& addr)
{
    enum Network net = addr.GetNetwork();
    return IsReachable(net);
}

bool GetMyExternalIP2(const CService& addrConnect, const char* pszGet, const char* pszKeyword, CNetAddr& ipRet)
{
    SOCKET hSocket;
    if (!ConnectSocket(addrConnect, hSocket, DEFAULT_CONNECT_TIMEOUT, NULL))
        return error("GetMyExternalIP() : connection to %s failed", addrConnect.ToString());

    if( !SetSocketNonBlocking(hSocket, false) )
        return error("GetMyExternalIP() : error restoring socket to blocking mode for %s", addrConnect.ToString());

    send(hSocket, pszGet, strlen(pszGet), MSG_NOSIGNAL);

    string strLine;
    while (RecvLine(hSocket, strLine))
    {
        if (strLine.empty()) // HTTP response is separated from headers by blank line
        {
            while (true)
            {
                if (!RecvLine(hSocket, strLine))
                {
                    CloseSocket(hSocket);
                    return false;
                }
                if (pszKeyword == NULL)
                    break;
                if (strLine.find(pszKeyword) != string::npos)
                {
                    strLine = strLine.substr(strLine.find(pszKeyword) + strlen(pszKeyword));
                    break;
                }
            }
            CloseSocket(hSocket);
            if (strLine.find("<") != string::npos)
                strLine = strLine.substr(0, strLine.find("<"));
            strLine = strLine.substr(strspn(strLine.c_str(), " \t\n\r"));
            while (strLine.size() > 0 && isspace(strLine[strLine.size()-1]))
                strLine.resize(strLine.size()-1);
            CService addr(strLine,0,true);
            LogPrintf("GetMyExternalIP() received [%s] %s\n", strLine, addr.ToString());
            if (!addr.IsValid() || !addr.IsRoutable())
                return false;
            ipRet.SetIP(addr);
            return true;
        }
    }
    CloseSocket(hSocket);
    return error("GetMyExternalIP() : connection closed");
}

bool GetMyExternalIP(CNetAddr& ipRet)
{
    CService addrConnect;
    const char* pszGet;
    const char* pszKeyword;

    for (int nLookup = 0; nLookup <= 1; nLookup++)
        for (int nHost = 1; nHost <= 2; nHost++) {
            // We should be phasing out our use of sites like these. If we need
            // replacements, we should ask for volunteers to put this simple
            // php file on their web server that prints the client IP:
            //  <?php echo $_SERVER["REMOTE_ADDR"]; ?>
            switch(nHost) {
                case 1 :
                    addrConnect = CService("66.171.248.178", 80); // whatismyipaddress.com has a bot too
                    if (nLookup == 1) {
                        CService addrIP("bot.whatismyipaddress.com", 80, true);
                        if (addrIP.IsValid())
                            addrConnect = addrIP;
                    }
                    pszGet = "GET / HTTP/1.1\r\n"
                             "Host: bot.whatismyipaddress.com\r\n"
                             "User-Agent: Mozilla/4.0\r\n"
                             "Connection: close\r\n"
                             "\r\n";
                    pszKeyword = NULL;
                break;
                case 2 :
                    addrConnect = CService("216.146.43.70", 80); // checkip.dyndns.org
                    if (nLookup == 1) {
                        CService addrIP("checkip.dyndns.org", 80, true);
                        if (addrIP.IsValid())
                            addrConnect = addrIP;
                    }
                    pszGet = "GET / HTTP/1.1\r\n"
                             "Host: checkip.dyndns.org\r\n"
                             "User-Agent: Mozilla/4.0\r\n"
                             "Connection: close\r\n"
                             "\r\n";

                    pszKeyword = "Address:";
                break;
            }
            if (GetMyExternalIP2(addrConnect, pszGet, pszKeyword, ipRet))
                return true;
        }
    return false;
}

void ThreadGetMyExternalIP()
{
    CNetAddr addrLocalHost;
    if (GetMyExternalIP(addrLocalHost))
    {
        LogPrintf("GetMyExternalIP() returned %s\n", addrLocalHost.ToStringIP());
        AddLocal(addrLocalHost, LOCAL_HTTP);
    }
}

void AddressCurrentlyConnected(const CService& addr)
{
    addrman.Connected(addr);
}


uint64_t CNode::nTotalBytesRecv = 0;
uint64_t CNode::nTotalBytesSent = 0;
CCriticalSection CNode::cs_totalBytesRecv;
CCriticalSection CNode::cs_totalBytesSent;

CNode* FindNode(const CNetAddr& ip)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        if ((CNetAddr)pnode->addr == ip)
            return (pnode);
    return NULL;
}

CNode* FindNode(const std::string& addrName)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        if (pnode->addrName == addrName)
            return (pnode);
    return NULL;
}

CNode* FindNode(const CService& addr)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
        if ((CService)pnode->addr == addr)
            return (pnode);
    return NULL;
}

CNode* ConnectNode(CAddress addrConnect, const char *pszDest)
{
    if (pszDest == NULL) {
        if (IsLocal(addrConnect))
            return NULL;

        // Look for an existing connection
        CNode* pnode = FindNode((CService)addrConnect);
        if (pnode)
        {
            pnode->AddRef();
            return pnode;
        }
    }

    /// debug print
    LogPrint("net", "trying connection %s lastseen=%.1fhrs\n",
        pszDest ? pszDest : addrConnect.ToString(),
        pszDest ? 0.0 : (double)(GetAdjustedTime() - addrConnect.nTime)/3600.0);

    // Connect
    SOCKET hSocket;
    bool proxyConnectionFailed;         //! Success will be assumed and set at the start of both the ConnectSocket calls
    int nPort = pszDest && isStringI2pDestination(string(pszDest)) ? 0 : Params().GetDefaultPort();
    if (pszDest ? ConnectSocketByName(addrConnect, hSocket, pszDest, nPort, nConnectTimeout, &proxyConnectionFailed) :
                  ConnectSocket(addrConnect, hSocket, nConnectTimeout, &proxyConnectionFailed))
    {
        //! Regardless of the outcome, while trying to connect, mark it as an Attempt
        //! This will not work however until ConnectSocketByName has been called for i2p addresses,
        //! during an addnode or any command calling ConnectNode with a string.
        //! So first we mark it here, now that a socket was successfully created.  Later on, if the
        //! version handshake happens addrman.Add/Good will get called to finish updates on the address (or not)
        // ToDo: Investigate how an issue here should best be solved.  When addnode xxx onetry is executed, and its not a full i2p destination
        // there will be no entry found in addrman for that connection attempt, if the address is not one already there.
        // The attempt fails, and returns false.
        // GR Update: Do not think this is still a problem, testing is underway or I would delete that comment as its been fixed?
        addrman.Attempt(addrConnect);

        LogPrint("net", "connected %s\n", pszDest ? pszDest : addrConnect.ToString());

        // Add node
        CNode* pnode = new CNode(hSocket, addrConnect, pszDest ? pszDest : "", false);
        pnode->AddRef();

        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }

        pnode->nTimeConnected = GetTime();

        return pnode;
    } else if (!proxyConnectionFailed) {
        //! If connecting to the node failed, and failure is not caused by a problem connecting to the proxy, mark this as an attempt.
        //! This next line of code is missing from v9 builds, and causes allot of problems with addrman working correctly because it
        //! was forgotten.  Figured it out independently while developing the b32.i2p address lookup system for I2P destinations.
        //! This recording of an attempt works for I2P destinations too.  Once ConnectSocketByName has been called for i2p addresses,
        //! such as during an addnode entered by the user, the destination will have been created in addrman, if it was a full base64
        //! or if it was b32.i2p and the lookup was successful, either by the i2p router or because it was found to already be on file.
        //! This logic can be hard to find and understand. It appears hidden, yet that was not the intention.  During ConnectSocketByName()
        //! a CNetAddr object will have been created based on that string name given, when that happens, all of the above is done
        //! automatically by the software in netbase.cpp  If ConnectSocket() is called, because we are simply given a CAddress object
        //! here to work with, and no string, it will also set the proxyConnectionFailed flag false always for i2p destinations.
        addrman.Attempt(addrConnect);
    }

    return NULL;
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;
    if (hSocket != INVALID_SOCKET)
    {
        LogPrint("net", "disconnecting peer %s\n", GetPeerLogStr(this));
        CloseSocket(hSocket);
    }

    // in case this fails, we'll empty the recv buffer when the CNode is deleted
    TRY_LOCK(cs_vRecvMsg, lockRecv);
    if (lockRecv)
        vRecvMsg.clear();
}

void CNode::PushVersion()
{
    int nBestHeight = n_signals.GetHeight().get_value_or(0);

    /// when NTP implemented, change to just nTime = GetAdjustedTime()
    int64_t nTime = (fInbound ? GetAdjustedTime() : GetTime());
    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(CService("0.0.0.0",0)));
    CAddress addrMe = GetLocalAddress(&addr);
    GetRandBytes((unsigned char*)&nLocalHostNonce, sizeof(nLocalHostNonce));

    LogPrint( "net", "send version message: version %d, blocks=%d, ", PROTOCOL_VERSION, nBestHeight );
    if (fLogIPs)
        LogPrint("net", "us=%s, them=%s, peer=%s\n", addrMe.ToString(), addrYou.ToString(), addr.ToString());
    else
        LogPrint("net", "peer=%d\n", id);
    PushMessage("version", PROTOCOL_VERSION, nLocalServices, nTime, addrYou, addrMe,
                nLocalHostNonce, FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>()), nBestHeight, true);
}





std::map<CNetAddr, int64_t> CNode::setBanned;
CCriticalSection CNode::cs_setBanned;

void CNode::ClearBanned()
{
    setBanned.clear();
}

bool CNode::IsBanned(CNetAddr ip)
{
    bool fResult = false;
    {
        LOCK(cs_setBanned);
        std::map<CNetAddr, int64_t>::iterator i = setBanned.find(ip);
        if (i != setBanned.end())
        {
            int64_t t = (*i).second;
            if (GetTime() < t)
                fResult = true;
        }
    }
    return fResult;
}

bool CNode::Ban(const CNetAddr &addr) {
    int64_t banTime = GetTime()+GetArg("-bantime", 60*60*24);  // Default 24-hour ban
    {
        LOCK(cs_setBanned);
        if (setBanned[addr] < banTime)
            setBanned[addr] = banTime;
    }
    return true;
}


std::vector<CSubNet> CNode::vWhitelistedRange;
CCriticalSection CNode::cs_vWhitelistedRange;

bool CNode::IsWhitelistedRange(const CNetAddr &addr) {
    LOCK(cs_vWhitelistedRange);
    BOOST_FOREACH(const CSubNet& subnet, vWhitelistedRange) {
        if (subnet.Match(addr))
            return true;
    }
    return false;
}

void CNode::AddWhitelistedRange(const CSubNet &subnet) {
    LOCK(cs_vWhitelistedRange);
    vWhitelistedRange.push_back(subnet);
}

#undef X
#define X(name) stats.name = name
void CNode::copyStats(CNodeStats &stats)
{
    stats.nodeid = this->GetId();
    X(nServices);
    X(nLastSend);
    X(nLastRecv);
    X(nTimeConnected);
    X(addrName);
    X(nVersion);
    X(cleanSubVer);
    X(fInbound);
    X(nStartingHeight);
    X(nSendBytes);
    X(nRecvBytes);
    X(fWhitelisted);

    // It is common for nodes with good ping times to suddenly become lagged,
    // due to a new block arriving or other large transfer.
    // Merely reporting pingtime might fool the caller into thinking the node was still responsive,
    // since pingtime does not update until the ping is complete, which might take a while.
    // So, if a ping is taking an unusually long time in flight,
    // the caller can immediately detect that this is happening.
    int64_t nPingUsecWait = 0;
    if ((0 != nPingNonceSent) && (0 != nPingUsecStart)) {
        nPingUsecWait = GetTimeMicros() - nPingUsecStart;
    }

    // Raw ping time is in microseconds, but show it to user as whole seconds (Anoncoin users should be well used to small numbers with many decimal places by now :)
    stats.dPingTime = (((double)nPingUsecTime) / 1e6);
    stats.dPingWait = (((double)nPingUsecWait) / 1e6);

    // Leave string empty if addrLocal invalid (not filled in yet)
    stats.addrLocal = addrLocal.IsValid() ? addrLocal.ToString() : "";
}
#undef X

// requires LOCK(cs_vRecvMsg)
bool CNode::ReceiveMsgBytes(const char *pch, unsigned int nBytes)
{
    while (nBytes > 0) {

        // get current incomplete message, or create a new one
        if (vRecvMsg.empty() ||
            vRecvMsg.back().complete())
            vRecvMsg.push_back(CNetMessage(nRecvStreamType, nRecvVersion));

        CNetMessage& msg = vRecvMsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
            handled = msg.readHeader(pch, nBytes);
        else
            handled = msg.readData(pch, nBytes);

        if (handled < 0)
                return false;

        if (msg.in_data && msg.hdr.nMessageSize > MAX_PROTOCOL_MESSAGE_LENGTH) {
            LogPrint("net", "Oversized message from %s, disconnecting", GetPeerLogStr(this));
            return false;
        }

        pch += handled;
        nBytes -= handled;

        if (msg.complete())
            msg.nTime = GetTimeMicros();
    }

    return true;
}

int CNetMessage::readHeader(const char *pch, unsigned int nBytes)
{
    // copy data to temporary parsing buffer
    unsigned int nRemaining = 24 - nHdrPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    memcpy(&hdrbuf[nHdrPos], pch, nCopy);
    nHdrPos += nCopy;

    // if header incomplete, exit
    if (nHdrPos < 24)
        return nCopy;

    // deserialize to CMessageHeader
    try {
        hdrbuf >> hdr;
    }
    catch (const std::exception &) {
        return -1;
    }

    // reject messages larger than MAX_SIZE
    if (hdr.nMessageSize > MAX_SIZE)
            return -1;

    // switch state to reading message data
    in_data = true;

    return nCopy;
}

int CNetMessage::readData(const char *pch, unsigned int nBytes)
{
    unsigned int nRemaining = hdr.nMessageSize - nDataPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    if (vRecv.size() < nDataPos + nCopy) {
        // Allocate up to 256 KiB ahead, but never more than the total message size.
        vRecv.resize(std::min(hdr.nMessageSize, nDataPos + nCopy + 256 * 1024));
    }

    memcpy(&vRecv[nDataPos], pch, nCopy);
    nDataPos += nCopy;

    return nCopy;
}









// requires LOCK(cs_vSend)
void SocketSendData(CNode *pnode)
{
    std::deque<CSerializeData>::iterator it = pnode->vSendMsg.begin();

    while (it != pnode->vSendMsg.end()) {
        const CSerializeData &data = *it;
        assert(data.size() > pnode->nSendOffset);
        int nBytes = send(pnode->hSocket, &data[pnode->nSendOffset], data.size() - pnode->nSendOffset, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (nBytes > 0) {
            pnode->nLastSend = GetTime();
            pnode->nSendBytes += nBytes;
            pnode->nSendOffset += nBytes;
            pnode->RecordBytesSent(nBytes);
            if (pnode->nSendOffset == data.size()) {
                pnode->nSendOffset = 0;
                pnode->nSendSize -= data.size();
                it++;
            } else {
                // could not send full message; stop sending more
                break;
            }
        } else {
            if (nBytes < 0) {
                // error
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                {
                    LogPrintf("socket send error %s\n", NetworkErrorString(nErr));
                    pnode->CloseSocketDisconnect();
                }
            }
            // couldn't send anything at all
            break;
        }
    }

    if (it == pnode->vSendMsg.end()) {
        assert(pnode->nSendOffset == 0);
        assert(pnode->nSendSize == 0);
    }
    pnode->vSendMsg.erase(pnode->vSendMsg.begin(), it);
}

static list<CNode*> vNodesDisconnected;

#ifdef ENABLE_I2PSAM
/**
 * Helper function, used below when adding a new I2P node, ported directly from 0.8.5.6 code
 */
static void AddIncomingI2pConnection(SOCKET hSocket, const CAddress& addr)
{
    int nInbound = 0;

    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (pnode->fInbound)
                nInbound++;
    }

    if (hSocket == INVALID_SOCKET)
    {
        int nErr = WSAGetLastError();
        if (nErr != WSAEWOULDBLOCK)
            LogPrintf("I2P socket error accept failed: %d\n", nErr);
    }
    else if (nInbound >= GetArg("-maxconnections", 125) - MAX_OUTBOUND_CONNECTIONS)
    {
        {
            LOCK(cs_setservAddNodeAddresses);
            if (!setservAddNodeAddresses.count(addr))
                CloseSocket(hSocket);
        }
    }
    else if (CNode::IsBanned(addr))
    {
        LogPrintf("connection from %s dropped (banned)\n", addr.ToString().c_str());
        CloseSocket(hSocket);
    }
    else
    {
        // At this point, the CAddress object has been correctly setup, the garlic field installed, the port zero'd out
        // and the native i2p destination address verified.  So we can simply add a new node and know that the correct stream
        // type and associated logic will all work correctly.
        LogPrintf("accepted connection %s\n", addr.ToString().c_str());
        CNode* pnode = new CNode(hSocket, addr, "", true);
        pnode->AddRef();
        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }
    }
}
#endif // ENABLE_I2PSAM

 //! Main Thread that handles socket's & their housekeeping...
void ThreadSocketHandler()
{
    unsigned int nPrevNodeCount = 0;
    while (true)
    {
        //
        // Disconnect nodes
        //
        {
            LOCK(cs_vNodes);
            // Disconnect unused nodes
            vector<CNode*> vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
            {
                if (pnode->fDisconnect ||
                    (pnode->GetRefCount() <= 0 && pnode->vRecvMsg.empty() && pnode->nSendSize == 0 && pnode->ssSend.empty()))
                {
                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // release outbound grant (if any)
                    pnode->grantOutbound.Release();

                    // close socket and cleanup
                    pnode->CloseSocketDisconnect();

                    // hold in disconnected pool until all refs are released
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }
        }
        {
            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            BOOST_FOREACH(CNode* pnode, vNodesDisconnectedCopy)
            {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0)
                {
                    bool fDelete = false;
                    {
                        TRY_LOCK(pnode->cs_vSend, lockSend);
                        if (lockSend)
                        {
                            TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                            if (lockRecv)
                            {
                                TRY_LOCK(pnode->cs_inventory, lockInv);
                                if (lockInv)
                                    fDelete = true;
                            }
                        }
                    }
                    if (fDelete)
                    {
                        vNodesDisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
        }

        // This is a really big event for Anoncoin-QT programmers,
        // fires off lots of activity to update the node count in client subsections.
        // Reduced allot of complexity we had, which tracked node counds for i2p as
        // well as ip based nodes, now all that is done in QT directly, all it needs
        // here is to know the count changed.
        if(vNodes.size() != nPrevNodeCount) {
            nPrevNodeCount = vNodes.size();
            uiInterface.NotifyNumConnectionsChanged(nPrevNodeCount);
        }

        //
        // Find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vSend

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;
        bool have_fds = false;

#ifdef ENABLE_I2PSAM
        BOOST_FOREACH(SOCKET hI2PListenSocket, vhI2PListenSocket) {
            if (hI2PListenSocket != INVALID_SOCKET) {
                FD_SET(hI2PListenSocket, &fdsetRecv);
                hSocketMax = max(hSocketMax, hI2PListenSocket);
                have_fds = true;
            }
        }
#endif // ENABLE_I2PSAM

        BOOST_FOREACH(const ListenSocket& hListenSocket, vhListenSocket) {
            FD_SET(hListenSocket.socket, &fdsetRecv);
            hSocketMax = max(hSocketMax, hListenSocket.socket);
            have_fds = true;
        }

        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
            {
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                FD_SET(pnode->hSocket, &fdsetError);
                hSocketMax = max(hSocketMax, pnode->hSocket);
                have_fds = true;

                // Implement the following logic:
                // * If there is data to send, select() for sending data. As this only
                //   happens when optimistic write failed, we choose to first drain the
                //   write buffer in this case before receiving more. This avoids
                //   needlessly queueing received data, if the remote peer is not themselves
                //   receiving data. This means properly utilizing TCP flow control signalling.
                // * Otherwise, if there is no (complete) message in the receive buffer,
                //   or there is space left in the buffer, select() for receiving data.
                // * (if neither of the above applies, there is certainly one message
                //   in the receiver buffer ready to be processed).
                // Together, that means that at least one of the following is always possible,
                // so we don't deadlock:
                // * We send some data.
                // * We wait for data to be received (and disconnect after timeout).
                // * We process a message in the buffer (message handler thread).
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend && !pnode->vSendMsg.empty()) {
                        FD_SET(pnode->hSocket, &fdsetSend);
                        continue;
                    }
                }
                {
                    TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                    if (lockRecv && (
                        pnode->vRecvMsg.empty() || !pnode->vRecvMsg.front().complete() ||
                        pnode->GetTotalRecvSize() <= ReceiveFloodSize()))
                        FD_SET(pnode->hSocket, &fdsetRecv);
                }
            }
        }

        int nSelect = select(have_fds ? hSocketMax + 1 : 0,
                             &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        boost::this_thread::interruption_point();

        if (nSelect == SOCKET_ERROR)
        {
            if (have_fds)
            {
                int nErr = WSAGetLastError();
                LogPrintf("socket select error %s\n", NetworkErrorString(nErr));
                for (unsigned int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            MilliSleep(timeout.tv_usec/1000);
        }

        //
        // Accept new connections
        //
        if( !IsI2POnly() ) {    //If I2P is the onlynet, we do not execute listen code for clearnet
        BOOST_FOREACH(const ListenSocket& hListenSocket, vhListenSocket)
        {
            if (hListenSocket.socket != INVALID_SOCKET && FD_ISSET(hListenSocket.socket, &fdsetRecv))
            {
                struct sockaddr_storage sockaddr;
                socklen_t len = sizeof(sockaddr);
                SOCKET hSocket = accept(hListenSocket.socket, (struct sockaddr*)&sockaddr, &len);
                CAddress addr;
                int nInbound = 0;

                if (hSocket != INVALID_SOCKET)
                    if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
                        LogPrintf("Warning: Unknown socket family\n");

                bool whitelisted = hListenSocket.whitelisted || CNode::IsWhitelistedRange(addr);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH(CNode* pnode, vNodes)
                        if (pnode->fInbound)
                            nInbound++;
                }

                if (hSocket == INVALID_SOCKET)
                {
                    int nErr = WSAGetLastError();
                    if (nErr != WSAEWOULDBLOCK)
                        LogPrintf("socket error accept failed: %s\n", NetworkErrorString(nErr));
                }
                else if (nInbound >= nMaxConnections - MAX_OUTBOUND_CONNECTIONS)
                {
                    CloseSocket(hSocket);
                }
                else if (CNode::IsBanned(addr) && !whitelisted)
                {
                    LogPrintf("connection from %s dropped (banned)\n", addr.ToString());
                    CloseSocket(hSocket);
                }
                else
                {
                    CNode* pnode = new CNode(hSocket, addr, "", true);
                    pnode->AddRef();
                    pnode->fWhitelisted = whitelisted;
                    LogPrint("net", "accepted connection for %s\n", GetPeerLogStr(pnode));
                    {
                        LOCK(cs_vNodes);
                        vNodes.push_back(pnode);
                    }
                }
            }
        }
        }
#ifdef ENABLE_I2PSAM
        //
        // Accept new incoming I2P connections
        //
        {
            std::vector<SOCKET>::iterator it = vhI2PListenSocket.begin();       // Start at the beginning of the list of listening sockets
            // As long as the router is still up & there are some sockets to listen on, keep trying to accept new connections
            while( !IsLimited( NET_I2P ) &&  it != vhI2PListenSocket.end() )
            {
                SOCKET& hI2PListenSocket = *it;
                if( hI2PListenSocket == INVALID_SOCKET ) {
                    it = vhI2PListenSocket.erase(it);
                    if( !BindListenNativeI2P() )
                        break;
                    it = vhI2PListenSocket.begin();             // Start over from the beginning
                    continue;
                }
                // At this point we have a valid socket setup to accept inbound connections, lets see if anyone is knocking...
                if (FD_ISSET(hI2PListenSocket, &fdsetRecv))
                {
                    const size_t bufSize = 1024;            // Same as i2pd has set on the other end
                    char pchBuf[bufSize];
                    memset(pchBuf, 0, bufSize);             // Yap someone is trying, lets find out who
                    int nBytes = recv(hI2PListenSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                    if (nBytes > 0)
                    {
                        // When a '/n' return shows up we got their destination identity
                        // See this url for destination specifications https://geti2p.net/en/docs/spec/common-structures#struct_Destination
                        // Although over I2P Sam we get it as a base64 string.
                        char *pNewLine = strchr( pchBuf, '\n' );
                        if( pNewLine ) {                     // Yap the address is all here, pNewLine would be null otherwise.
                            // Lets make sure it looks correct as a base64 i2p destination string
                            // We've been waiting for a valid base64 string, followed by a '\n' character, we got that.
                            if( strlen(pchBuf) == NATIVE_I2P_DESTINATION_SIZE + 1 )
                            {
                                // Fantastic if it checks out, we have another node!
                                // The socket will be bound to that and used for message communications
                                std::string incomingAddr(pchBuf, pchBuf + NATIVE_I2P_DESTINATION_SIZE);
                                CAddress addr;
                                if( addr.SetI2pDestination(incomingAddr) )
                                    AddIncomingI2pConnection(hI2PListenSocket, addr);
                                else {
                                    LogPrintf("WARNING - Invalid incoming destination address, unable to setup node.  Received (%s)\n", incomingAddr.c_str());
                                    CloseSocket(hI2PListenSocket);
                                }
                            } else {
                                LogPrintf("WARNING - Destination size mismatch. Only 516 chars+newline allowed. Received (%d) bytes or limit(1024), the string in []:\n[%s]", nBytes, pchBuf);
                                CloseSocket(hI2PListenSocket);
                            }
                        } else {
                            LogPrintf("WARNING - No eol found in destination address from router, size & data received (%d) [%s]\n", nBytes, pchBuf);
                            CloseSocket(hI2PListenSocket);
                        }
                    }
                    else if (nBytes == 0)
                    {
                        // socket closed gracefully, but why?  This shouldn't have happened
                        LogPrintf("WARNING - I2P listen socket was closed unexpectedly, with no data received.  Will attempt to open a new one.\n");
                        CloseSocket(hI2PListenSocket);
                    }
                    else if (nBytes < 0)
                    {
                        // error
                        const int nErr = WSAGetLastError();
                        if (nErr == WSAEWOULDBLOCK || nErr == WSAEMSGSIZE || nErr == WSAEINTR || nErr == WSAEINPROGRESS)
                            continue;

                        LogPrintf("WARNING - I2P listen socket recv error %d, Will attempt to open a new one.\n", nErr);
                        CloseSocket(hI2PListenSocket);
                    }
                    // We now need to invalidate that socket from accepting new inbound connections,
                    // it was either closed do to an error, or hopefully a new node was added to our peers list as inbound.
                    // It will be sweep away and erased, then a new acceptor added later
                    *it++ = INVALID_SOCKET;
                } else                                      // Just keep looking
                    it++;
            }
        }
#endif  // ENABLE_I2PSAM

        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->AddRef();
        }
        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            boost::this_thread::interruption_point();

            //
            // Receive
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetRecv) || FD_ISSET(pnode->hSocket, &fdsetError))
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            if (!pnode->ReceiveMsgBytes(pchBuf, nBytes))
                                pnode->CloseSocketDisconnect();
                            pnode->nLastRecv = GetTime();
                            pnode->nRecvBytes += nBytes;
                            pnode->RecordBytesRecv(nBytes);
                        }
                        else if (nBytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                                LogPrint("net", "socket closed\n");
                            pnode->CloseSocketDisconnect();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                if (!pnode->fDisconnect)
                                    LogPrintf("socket recv error %s\n", NetworkErrorString(nErr));
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetSend))
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SocketSendData(pnode);
            }

            //
            // Inactivity checking
            //
            int64_t nTime = GetTime();
            if (nTime - pnode->nTimeConnected > 60)
            {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0)
                {
                    LogPrint("net", "socket no message in first 60 seconds, %d %d from %s\n", pnode->nLastRecv != 0, pnode->nLastSend != 0, GetPeerLogStr(pnode));
                    pnode->fDisconnect = true;
                }
                else if (nTime - pnode->nLastSend > TIMEOUT_INTERVAL)
                {
                    LogPrintf("socket sending timeout: %is\n", nTime - pnode->nLastSend);
                    pnode->fDisconnect = true;
                }
                else if (nTime - pnode->nLastRecv > TIMEOUT_INTERVAL)
                {
                    LogPrintf("socket receive timeout: %is\n", nTime - pnode->nLastRecv);
                    pnode->fDisconnect = true;
                }
                else if (pnode->nPingNonceSent && pnode->nPingUsecStart + TIMEOUT_INTERVAL * 1000000 < GetTimeMicros())
                {
                    LogPrintf("ping timeout: %fs\n", 0.000001 * (GetTimeMicros() - pnode->nPingUsecStart));
                    pnode->fDisconnect = true;
                }
            }
        }
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }
    }
}









#ifdef USE_UPNP
void ThreadMapPort()
{
    std::string port = strprintf("%u", GetListenPort());
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    struct UPNPDev * devlist = 0;
    char lanaddr[64];

#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);
#else
    /* miniupnpc 1.6 */
    int error = 0;
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if(r != UPNPCOMMAND_SUCCESS)
                LogPrintf("UPnP: GetExternalIPAddress() returned %d\n", r);
            else
            {
                if(externalIPAddress[0])
                {
                    LogPrintf("UPnP: ExternalIPAddress = %s\n", externalIPAddress);
                    AddLocal(CNetAddr(externalIPAddress), LOCAL_UPNP);
                }
                else
                    LogPrintf("UPnP: GetExternalIPAddress failed.\n");
            }
        }

        string strDesc = "Anoncoin " + FormatFullVersion();

        try {
            while (true) {
#ifndef UPNPDISCOVER_SUCCESS
                /* miniupnpc 1.5 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
                /* miniupnpc 1.6 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

                if(r!=UPNPCOMMAND_SUCCESS)
                    LogPrintf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                        port, port, lanaddr, r, strupnperror(r));
                else
                    LogPrintf("UPnP Port Mapping successful.\n");;

                MilliSleep(20*60*1000); // Refresh every 20 minutes
            }
        }
        catch (boost::thread_interrupted)
        {
            r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);
            LogPrintf("UPNP_DeletePortMapping() returned : %d\n", r);
            freeUPNPDevlist(devlist); devlist = 0;
            FreeUPNPUrls(&urls);
            throw;
        }
    } else {
        LogPrintf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist); devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&urls);
    }
}

void MapPort(bool fUseUPnP)
{
    static boost::thread* upnp_thread = NULL;

    if (fUseUPnP)
    {
        if (upnp_thread) {
            upnp_thread->interrupt();
            upnp_thread->join();
            delete upnp_thread;
        }
        upnp_thread = new boost::thread(boost::bind(&TraceThread<void (*)()>, "upnp", &ThreadMapPort));
    }
    else if (upnp_thread) {
        upnp_thread->interrupt();
        upnp_thread->join();
        delete upnp_thread;
        upnp_thread = NULL;
    }
}

#else
void MapPort(bool)
{
    // Intentionally left blank.
}
#endif



/**
 * Process DNS seed data from chainparams
 */
void ThreadDNSAddressSeed()
{
    // goal: only query DNS seeds if address need is acute, give the network at least 120 seconds to have 2 peers connected
    if ((addrman.size() > 0) &&
        (!GetBoolArg("-forcednsseed", false))) {
        // MilliSleep(11 * 1000);
        MilliSleep(120 * 1000);

        LOCK(cs_vNodes);
        if (vNodes.size() >= 2) {
            LogPrintf("P2P peers available. Skipped DNS seeding.\n");
            return;
        }
    }

    const int nOneDay = 24*3600;
    int found = 0;
    LogPrintf("Loading addresses from DNS seeds (could take a while)\n");
//    LogPrintf("DNS seeds temporary disabled\n");
//    return;

#ifdef ENABLE_I2PSAM
    // If I2P is enabled, we need to process those too...
    if( IsI2PEnabled() ) {
        LogPrintf("Loading b32.i2p destination seednodes...\n");
        const vector<CDNSSeedData> &i2pvSeeds = Params().i2pDNSSeeds();
        vector<CAddress> vAdd;
        BOOST_FOREACH(const CDNSSeedData &seed, i2pvSeeds) {
            CAddress addrSeed;
            // Lookup the b32.i2p destination, if it returns true, then the full Base64 destination is loaded,
            // the GarlicCat field is set, & the port=0, the seed was on file locally or looked up over the router.
            if( addrSeed.SetSpecial( seed.host ) ) {
                addrSeed.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
                vAdd.push_back(addrSeed);
                found++;
            }   // If the seed could not be found, Skip it
        }
        // Set the source to anything other than the same b32.i2p destination (as is done in chainparams.cpp,
        // doing that will cause addrman to think the services are correct, and alter them to these incorrect values,
        // if its already on file.
        addrman.Add( vAdd, CNetAddr("127.0.0.1") );
    }

    // ToDo: Figure if if this should be I2P only or ??, clearnet seeds will do us no good in various cases...
    if( IsDarknetOnly() )                                                           // Do what we normally would do on clearnet
        LogPrintf("Clearnet DNS seeds disabled, Running Darknet only mode.\n");
    else
#endif
    {
        const vector<CDNSSeedData> &vSeeds = Params().DNSSeeds();
        LogPrintf("Loading clearnet DNS seed addresses...\n");
        BOOST_FOREACH(const CDNSSeedData &seed, vSeeds) {
            if (HaveNameProxy()) {
                AddOneShot(seed.host);
            } else {
                vector<CNetAddr> vIPs;
                vector<CAddress> vAdd;
                if (LookupHost(seed.host.c_str(), vIPs))
                {
                    BOOST_FOREACH(CNetAddr& ip, vIPs)
                    {
                        int nOneDay = 24*3600;
                        CAddress addr = CAddress(CService(ip, Params().GetDefaultPort()));
                        addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay); // use a random age between 3 and 7 days old
                        vAdd.push_back(addr);
                        found++;
                    }
                }
                addrman.Add(vAdd, CNetAddr(seed.name, true));
            }
        }
    }

    LogPrintf("%d addresses found from DNS seeds\n", found);
}












void DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

    CAddrDB adb;
    adb.Write(addrman);

    LogPrint("net", "Flushed %d addresses to peers.dat  %dms\n",
           addrman.size(), GetTimeMillis() - nStart);
}

void static ProcessOneShot()
{
    string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty())
            return;
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        if (!OpenNetworkConnection(addr, &grant, strDest.c_str(), true))
            AddOneShot(strDest);
    }
}

void ThreadOpenConnections()
{
    // Connect to specific addresses
    size_t nConnectsMulti = mapMultiArgs["-connect"].size();
    size_t nConnectsMap = mapArgs.count("-connect");
    if( nConnectsMap && nConnectsMulti > 0 )
    // if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0)
    {
        //! Special case: a single connect="" parameter specified.
        //! where we completely exit this thread. no outbound connections.
        //! Means no oneshot processing, its up to you to insure listen=1
        //! to receive inbound connections.
        if( nConnectsMap == 1 && nConnectsMulti < 2 && mapArgs["-connect"] == "" )
            return;

        for (int64_t nLoop = 0;; nLoop++)
        {
            ProcessOneShot();
            BOOST_FOREACH(string strAddr, mapMultiArgs["-connect"])
            {
                CAddress addr;
                OpenNetworkConnection(addr, NULL, strAddr.c_str());
                for (int i = 0; i < 10 && i < nLoop; i++)
                {
                    MilliSleep(500);
                }
            }
            MilliSleep(30000);          // Only thy every 30 seconds
        }
    }

    // Initiate network connections
    int64_t nStart = GetTime();
    while (true)
    {
        ProcessOneShot();

        MilliSleep(500);

        CSemaphoreGrant grant(*semOutbound);
        boost::this_thread::interruption_point();

        // Add seed nodes right away, b32.i2p address lookup takes a long time, and we have full
        // destination base64 addresses in the code, put them in addrman, so it can start looking.
        // As we have no clearnet fixed seeds, this is pointless unless the i2p network is available
#ifdef ENABLE_I2PSAM
        if( addrman.size() < 10 && IsI2PEnabled() ) {
            static bool donei2pseeds = false;
            if (!donei2pseeds) {
                LogPrintf("Adding fixed i2p destination seednodes...\n");
                addrman.Add(Params().FixedI2PSeeds(), CNetAddr("127.0.0.1"));
                donei2pseeds = true;
            }
        }
#endif

        if( (GetTime() - nStart > 60) && !IsDarknetOnly() ) {
        // Add seed nodes if DNS seeds are all down (an infrastructure attack?).
            static bool doneDNSseeds = false;
            if (!doneDNSseeds) {
                LogPrintf("Adding fixed seed nodes as DNS doesn't seem to be available.\n");
                addrman.Add(Params().FixedSeeds(), CNetAddr("127.0.0.1"));
                doneDNSseeds = true;
            }
        }

        // Note on I2P seed nodes: They are stored now in Chainparams, and loaded based on what Network we are running.
        // No special code needs to be introduced here, as was the case in 0.8.5.6 builds.

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        // Only connect out to one peer per network group (/16 for IPv4).
        // Do this here so we don't have to critsect vNodes inside mapAddresses critsect.
        int nOutbound = 0;
        set<vector<unsigned char> > setConnected;
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes) {
                if (!pnode->fInbound) {
                    setConnected.insert(pnode->addr.GetGroup());
                    nOutbound++;
                }
            }
        }

        int64_t nANow = GetAdjustedTime();

        int nTries = 0;
        while (true)
        {
            // use an nUnkBias between 10 (no outgoing connections), and we need to try to connect to good nodes,
            // that have been tried before and found to be likely good connections, out to 90% where almost every
            // node tried will be a new and untested node address.
            // At min pass 10(%) to Select when no outbound connections, if we're at the target outbound level,
            // we want to pass pass 90(%) to the select() as the bias amount.
            // Where addrman will mostly always try connections that are new and have not been tried before.
            // In order to correctly allow the programmer to change the MAX_OUTBOUND_CONNECTIONS from 8 to 20 or whatever
            // value, we need better math here to scale the value passed to addrman select.
            // The programmed target range is now 10..90% using fast integer math, other than that point, any
            // value can be now set and this will approximate the correct percentage.
            // int nNewBias = (nOutbound * 80) / MAX_OUTBOUND_CONNECTIONS;
            // Keep the max value less than 100%, as the routine expects
            // nNewBias = min( nNewBias, 99 );
            // CSlave removed
            CAddrInfo addr = addrman.Select();

            // if we selected an invalid address, restart
            if (!addr.IsValid() || setConnected.count(addr.GetGroup()) || IsLocal(addr))
                break;

            // If we didn't find an appropriate destination after trying 100 addresses fetched from addrman,
            // stop this loop, and let the outer loop run again (which sleeps, adds seed nodes, recalculates
            // already-connected network ranges, ...) before trying new addrman addresses.
            nTries++;
            if (nTries > 100)
                break;

            if (IsLimited(addr))
                continue;

            // only consider very recently tried nodes after 30 failed attempts
            if (nANow - addr.nLastTry < 180 && nTries < 30)
                continue;

            // do not allow non-default ports, unless after 50 invalid addresses selected already
            if( !addr.IsI2P() && addr.GetPort() != Params().GetDefaultPort() && nTries < 50 )
                continue;

            addrConnect = addr;
            break;
        }
        if (addrConnect.IsValid())
            OpenNetworkConnection(addrConnect, &grant);
    }
}

void ThreadOpenAddedConnections()
{
    {
        LOCK(cs_vAddedNodes);
        vAddedNodes = mapMultiArgs["-addnode"];
    }

    if (HaveNameProxy()) {
        while(true) {
            list<string> lAddresses(0);
            {
                LOCK(cs_vAddedNodes);
                BOOST_FOREACH(string& strAddNode, vAddedNodes)
                    lAddresses.push_back(strAddNode);
            }
            BOOST_FOREACH(string& strAddNode, lAddresses) {
                CAddress addr;
                CSemaphoreGrant grant(*semOutbound);
                OpenNetworkConnection(addr, &grant, strAddNode.c_str());
                MilliSleep(500);
            }
            MilliSleep(120000); // Retry every 2 minutes
        }
    }

    for (unsigned int i = 0; true; i++)
    {
        list<string> lAddresses(0);
        {
            LOCK(cs_vAddedNodes);
            BOOST_FOREACH(string& strAddNode, vAddedNodes)
                lAddresses.push_back(strAddNode);
        }

        list<vector<CService> > lservAddressesToAdd(0);
        BOOST_FOREACH(string& strAddNode, lAddresses)
        {
            vector<CService> vservNode(0);
            if( Lookup(strAddNode.c_str(), vservNode, isStringI2pDestination(strAddNode) ? 0 : Params().GetDefaultPort(), fNameLookup, 0) )
            {
                lservAddressesToAdd.push_back(vservNode);
                {
                    LOCK(cs_setservAddNodeAddresses);
                    BOOST_FOREACH(CService& serv, vservNode)
                        setservAddNodeAddresses.insert(serv);
                }
            }
        }
        // Attempt to connect to each IP for each addnode entry until at least one is successful per addnode entry
        // (keeping in mind that addnode entries can have many IPs if fNameLookup)
        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodes)
                for (list<vector<CService> >::iterator it = lservAddressesToAdd.begin(); it != lservAddressesToAdd.end(); it++)
                    BOOST_FOREACH(CService& addrNode, *(it))
                        if (pnode->addr == addrNode)
                        {
                            it = lservAddressesToAdd.erase(it);
                            it--;
                            break;
                        }
        }
        BOOST_FOREACH(vector<CService>& vserv, lservAddressesToAdd)
        {
            CSemaphoreGrant grant(*semOutbound);
            OpenNetworkConnection(CAddress(vserv[i % vserv.size()]), &grant);
            MilliSleep(500);
        }
        MilliSleep(120000); // Retry every 2 minutes
    }
}

// if successful, this moves the passed grant to the constructed node
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant *grantOutbound, const char *strDest, bool fOneShot)
{
    //
    // Initiate outbound network connection
    //
    boost::this_thread::interruption_point();
    string sBase32OrIP = "";                // If a strDest pointer is give, we store the modified result here before connecting a new node.

    if (!strDest) {                         // If a string was not given, just do what we normally would given a CAddress object
        if( IsLocal(addrConnect) || FindNode((CNetAddr)addrConnect) || CNode::IsBanned(addrConnect) || FindNode(addrConnect.ToStringIPPort().c_str()) )
            return false;
    }
    else {                                  // A string was given
        // Before we print out a string (ConnectNode() and create the node, for any given string, we need to clean up the input, if for example it's in square brackets
        // or if its a full base64 string we need to make it a b32.i2p address before creating the node address string for comparison in FindNode even.
        // One big problem is when strDest is a full base64 address, we don't want to do a reverse lookup of a b32.i2p address for it over the network, if we already know
        // the value. The best solution is to create a CAdress object and store the result in addrman, that way it and we know about it, and can always find that b32.i2p address
        // again right away, which we'll soon need to open a new network connection.  After all this, we can continue on, with a strDest value that is the b32.i2p address of that
        // given input string, and the rest of the code will not know the difference.

        std::string strHost(strDest);   // Remove any square brackets now, right away before they cause us any comparison errors
        if( boost::algorithm::starts_with(strHost, "[") && boost::algorithm::ends_with(strHost, "]") )
            strHost = strHost.substr(1, strHost.size() - 2);

        if( isValidI2pAddress( strHost ) ) {                        // In this case we have a full base64 i2p destination given to us
            // When addr object below is created, it will cause LookupHost->LookupIntern->SetSpecial to happen
            // for a full i2p destination string, no lookup will be done for this case, and we actually add a new entry into addrman
            // if it wasn't there already.
            CAddress addr( CService( CNetAddr(strHost), 0) );
            // Now we create a hash map entry for this destination object, the source must have been from us locally here, if we're given a string
            // All sources of an OpenNetworkConnection with a string value would be from us in configuration, console entry, or an rpc client.
            // Locally 127.0.0.1 is MUCH easier to use as the source addr, than using any i2p address, a long base64 destination or in the case
            // of a b32.i2p address, it will cause a lookup when the CNetAddr object is created.  Complicated.
            // Anyway, no time penalty need be applied in the addrman parameter options, which is default when called.
            //
            // addnode onetry calls this routine directly, the rest of the addnode commands place entries into a 'vAddedNode' vector, and create
            // CAddress objects directly from the values, bypassing this code from running as expected.
            // ToDo: Figure out and fix any problems there if needed.
            addrman.Add( addr, CNetAddr("127.0.0.1") );
            // Our goal here is to ALWAYS be giving the FindNode/ConnectNode commands b32.i2p destinations, so that they work as expected.
            sBase32OrIP = B32AddressFromDestination( strHost );
        } else
            sBase32OrIP = strHost;                                     // Regardless of any modifications, ipxx or b32.i2p we now need the new non-const variable setup.
    }

    if( sBase32OrIP.size() && FindNode( sBase32OrIP.c_str() ) )
        return false;

    CNode* pnode = ConnectNode( addrConnect, sBase32OrIP.size() ? sBase32OrIP.c_str() : NULL );
    boost::this_thread::interruption_point();

    if (!pnode)
        return false;
    if (grantOutbound)
        grantOutbound->MoveTo(pnode->grantOutbound);
    pnode->fNetworkNode = true;
    if (fOneShot)
        pnode->fOneShot = true;

    return true;
}


void ThreadMessageHandler()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    while (true)
    {
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            BOOST_FOREACH(CNode* pnode, vNodesCopy) {
                pnode->AddRef();
            }
        }

        // Poll the connected nodes for messages
        CNode* pnodeTrickle = NULL;
        if (!vNodesCopy.empty())
            pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];

        bool fSleep = true;

        BOOST_FOREACH(CNode* pnode, vNodesCopy)
        {
            if (pnode->fDisconnect)
                continue;

            // Receive messages
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    if (!n_signals.ProcessMessages(pnode))
                        pnode->CloseSocketDisconnect();

                    if (pnode->nSendSize < SendBufferSize())
                    {
                        if (!pnode->vRecvGetData.empty() || (!pnode->vRecvMsg.empty() && pnode->vRecvMsg[0].complete()))
                        {
                            fSleep = false;
                        }
                    }
                }
            }
            boost::this_thread::interruption_point();

            // Send messages
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    n_signals.SendMessages(pnode, pnode == pnodeTrickle || pnode->fWhitelisted);
            }
            boost::this_thread::interruption_point();
        }

        {
            LOCK(cs_vNodes);
            BOOST_FOREACH(CNode* pnode, vNodesCopy)
                pnode->Release();
        }

        if (fSleep)
            MilliSleep(100);
    }
}






bool BindListenPort(const CService &addrBind, string& strError, bool fWhitelisted)
{
    strError = "";
    int nOne = 1;

    // Create socket for listening for incoming connections
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len))
    {
        strError = strprintf("Error: Bind address family for %s not supported", addrBind.ToString());
        LogPrintf("%s\n", strError);
        return false;
    }

    SOCKET hListenSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        strError = strprintf("Error: Couldn't open socket for incoming connections (socket returned error %s)", NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        return false;
    }

#ifndef WIN32
#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted. Not an issue on windows!
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int));
#endif

    // Set to non-blocking, incoming connections will also inherit this
    if (!SetSocketNonBlocking(hListenSocket, true)) {
        strError = strprintf("BindListenPort: Setting listening socket to non-blocking failed, error %s\n", NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        return false;
    }

    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
#ifdef WIN32
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&nOne, sizeof(int));
#else
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&nOne, sizeof(int));
#endif
#endif
#ifdef WIN32
        int nProtLevel = PROTECTION_LEVEL_UNRESTRICTED;
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL, (const char*)&nProtLevel, sizeof(int));
#endif
    }

    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to %s on this computer. Anoncoin Core is probably already running."), addrBind.ToString());
        else
            strError = strprintf(_("Unable to bind to %s on this computer (bind returned error %s)"), addrBind.ToString(), NetworkErrorString(nErr));
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }
    LogPrintf("Bound to %s\n", addrBind.ToString());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf(_("Error: Listening for incoming connections failed (listen returned error %s)"), NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }

    vhListenSocket.push_back(ListenSocket(hListenSocket, fWhitelisted));

    if (addrBind.IsRoutable() && fDiscover && !fWhitelisted)
        AddLocal(addrBind, LOCAL_BIND);

    return true;
}


#ifdef ENABLE_I2PSAM
/**
 * I2P Specific socket listen binding functions
 */
bool BindListenNativeI2P()
{
    SOCKET hNewI2PListenSocket = INVALID_SOCKET;
    if (!BindListenNativeI2P(hNewI2PListenSocket))
        return false;
    vhI2PListenSocket.push_back(hNewI2PListenSocket);
    return true;
}

bool BindListenNativeI2P(SOCKET& hSocket)
{
    if( !IsLimited( NET_I2P ) ) {
        hSocket = I2PSession::Instance().accept(false);
        if (SetSocketNonBlocking(hSocket, true)) { // Set to non-blocking
            string sDest = GetArg( "-i2p.mydestination.publickey", "" );
            CService addrBind( sDest, 0 );
            return AddLocal( addrBind, LOCAL_BIND );
        } else
            LogPrintf( "ERROR - Unable to set I2P Socket options to non-blocking, after I2P accept was issued.\n" );
    }
    else
        LogPrintf( "ERROR - Unexpected I2P BIND request. Ignored, network access is limited.\n" );

}
#endif // ENABLE_I2PSAM

void static Discover(boost::thread_group& threadGroup)
{
    if (!fDiscover)
        return;

#ifdef WIN32
    // Get local host IP
    char pszHostName[256] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR)
    {
        vector<CNetAddr> vaddr;
        if (LookupHost(pszHostName, vaddr))
        {
            BOOST_FOREACH (const CNetAddr &addr, vaddr)
            {
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s : %s - %s\n", __func__, pszHostName, addr.ToString());
            }
        }
    }
#else
    // Get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs* ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_flags & IFF_UP) == 0) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0) continue;
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                CNetAddr addr(s4->sin_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s : IPv4 %s: %s\n", __func__, ifa->ifa_name, addr.ToString());
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                CNetAddr addr(s6->sin6_addr);
                if (AddLocal(addr, LOCAL_IF))
                    LogPrintf("%s : IPv6 %s: %s\n", __func__, ifa->ifa_name, addr.ToString());
            }
        }
        freeifaddrs(myaddrs);
    }
#endif

    // Don't use external IPv4 discovery, when -onlynet="IPv6"
    // This should work fine as well for -onlynet="i2p" or other combinations, if NET_IPV4 is
    // not limited at this point, we proceed normally to detect our external IP
    if (!IsLimited(NET_IPV4))
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "ext-ip", &ThreadGetMyExternalIP));
}

void StartNode(boost::thread_group& threadGroup)
{
    uiInterface.InitMessage(_("Loading addresses..."));
    // Load addresses for peers.dat
    int64_t nStart = GetTimeMillis();
    {
        CAddrDB adb;
        if (!adb.Read(addrman))
            LogPrintf("Invalid or missing peers.dat; recreating\n");
    }
#ifdef ENABLE_I2PSAM
    LogPrintf("Loaded %i addresses from peers.dat in %dms and setup a %d entry address book for b32.i2p destinations.\n",
           addrman.size(), GetTimeMillis() - nStart, addrman.b32HashTableSize() );
#endif
    fAddressesInitialized = true;

    if (semOutbound == NULL) {
        // initialize semaphore
        int nMaxOutbound = min(MAX_OUTBOUND_CONNECTIONS, nMaxConnections);
        semOutbound = new CSemaphore(nMaxOutbound);
    }

    if (pnodeLocalHost == NULL)
        pnodeLocalHost = new CNode(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), nLocalServices));

    Discover(threadGroup);

    //
    // Start threads
    //

    if (!GetBoolArg("-dnsseed", true))
        LogPrintf("DNS seeding disabled\n");
    else
        threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "dnsseed", &ThreadDNSAddressSeed));

    // Map ports with UPnP
    MapPort(GetBoolArg("-upnp", DEFAULT_UPNP));

    // Send and receive from sockets, accept connections
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "net", &ThreadSocketHandler));

    // Initiate outbound connections from -addnode
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "addcon", &ThreadOpenAddedConnections));

    // Initiate outbound connections
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "opencon", &ThreadOpenConnections));

    // Process messages
    threadGroup.create_thread(boost::bind(&TraceThread<void (*)()>, "msghand", &ThreadMessageHandler));

    // Dump network addresses
    threadGroup.create_thread(boost::bind(&LoopForever<void (*)()>, "dumpaddr", &DumpAddresses, DUMP_ADDRESSES_INTERVAL * 1000));
}

bool StopNode()
{
    LogPrintf("StopNode()\n");
    MapPort(false);
    if (semOutbound)
        for (int i=0; i<MAX_OUTBOUND_CONNECTIONS; i++)
            semOutbound->post();

    if (fAddressesInitialized)
    {
        DumpAddresses();
        fAddressesInitialized = false;
    }

    return true;
}

class CNetCleanup
{
public:
    CNetCleanup() {}

    ~CNetCleanup()
    {
        // Close sockets
        BOOST_FOREACH(CNode* pnode, vNodes)
            if (pnode->hSocket != INVALID_SOCKET)
                CloseSocket(pnode->hSocket);
        BOOST_FOREACH(ListenSocket& hListenSocket, vhListenSocket)
            if (hListenSocket.socket != INVALID_SOCKET)
                if (!CloseSocket(hListenSocket.socket))
                    LogPrintf("CloseSocket(hListenSocket) failed with error %s\n", NetworkErrorString(WSAGetLastError()));
#ifdef ENABLE_I2PSAM
        BOOST_FOREACH(SOCKET& hI2PListenSocket, vhI2PListenSocket)
            if (hI2PListenSocket != INVALID_SOCKET)
                if( !CloseSocket(hI2PListenSocket) )
                    LogPrintf("I2P closesocket(hI2PListenSocket) failed with error %d\n", WSAGetLastError());
#endif // ENABLE_I2PSAM

        // clean up some globals (to help leak detection)
        BOOST_FOREACH(CNode *pnode, vNodes)
            delete pnode;
        BOOST_FOREACH(CNode *pnode, vNodesDisconnected)
            delete pnode;
        vNodes.clear();
        vNodesDisconnected.clear();
        vhListenSocket.clear();
        delete semOutbound;
        semOutbound = NULL;
        delete pnodeLocalHost;
        pnodeLocalHost = NULL;

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
}
instance_of_cnetcleanup;







void RelayTransaction(const CTransaction& tx)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, ss);
}

void RelayTransaction(const CTransaction& tx, const CDataStream& ss)
{
    CInv inv(MSG_TX, tx.GetHash());
    {
        LOCK(cs_mapRelay);
        // Expire old relay messages
        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < GetTime())
        {
            mapRelay.erase(vRelayExpiration.front().second);
            vRelayExpiration.pop_front();
        }

        // Save original serialized message so newer versions are preserved
        mapRelay.insert(std::make_pair(inv, ss));
        vRelayExpiration.push_back(std::make_pair(GetTime() + 15 * 60, inv));
    }
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if(!pnode->fRelayTxes)
            continue;
        LOCK(pnode->cs_filter);
        if (pnode->pfilter)
        {
            if (pnode->pfilter->IsRelevantAndUpdate(tx))
                pnode->PushInventory(inv);
        } else
            pnode->PushInventory(inv);
    }
}

void CNode::RecordBytesRecv(uint64_t bytes)
{
    LOCK(cs_totalBytesRecv);
    nTotalBytesRecv += bytes;
}

void CNode::RecordBytesSent(uint64_t bytes)
{
    LOCK(cs_totalBytesSent);
    nTotalBytesSent += bytes;
}

uint64_t CNode::GetTotalBytesRecv()
{
    LOCK(cs_totalBytesRecv);
    return nTotalBytesRecv;
}

uint64_t CNode::GetTotalBytesSent()
{
    LOCK(cs_totalBytesSent);
    return nTotalBytesSent;
}

void CNode::Fuzz(int nChance)
{
    if (!fSuccessfullyConnected) return; // Don't fuzz initial handshake
    if (GetRand(nChance) != 0) return; // Fuzz 1 of every nChance messages

    switch (GetRand(3))
    {
    case 0:
        // xor a random byte with a random value:
        if (!ssSend.empty()) {
            CDataStream::size_type pos = GetRand(ssSend.size());
            ssSend[pos] ^= (unsigned char)(GetRand(256));
        }
        break;
    case 1:
        // delete a random byte:
        if (!ssSend.empty()) {
            CDataStream::size_type pos = GetRand(ssSend.size());
            ssSend.erase(ssSend.begin()+pos);
        }
        break;
    case 2:
        // insert a random byte at a random position
        {
            CDataStream::size_type pos = GetRand(ssSend.size());
            char ch = (char)GetRand(256);
            ssSend.insert(ssSend.begin()+pos, ch);
        }
        break;
    }
    // Chance of more than one change half the time:
    // (more changes exponentially less likely):
    Fuzz(2);
}

//
// CAddrDB
//

CAddrDB::CAddrDB()
{
    pathAddr = GetDataDir() / "peers.dat";
}

bool CAddrDB::Write(const CAddrMan& addr)
{
    // Generate random temporary filename
    unsigned short randv = 0;
    GetRandBytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("peers.dat.%04x", randv);

    // serialize addresses, checksum data up to that point, then append csum
    CDataStream ssPeers(SER_DISK, CLIENT_VERSION);
    ssPeers << FLATDATA(Params().MessageStart());
    ssPeers << addr;
    uint256 hash = Hash(ssPeers.begin(), ssPeers.end());
    ssPeers << hash;

    // open temp output file, and associate with CAutoFile
    boost::filesystem::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fopen(pathTmp.string().c_str(), "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("%s : Failed to open file %s", __func__, pathTmp.string());

    // Write and commit header, data
    try {
        fileout << ssPeers;
    }
    catch (std::exception &e) {
        return error("%s : Serialize or I/O error - %s", __func__, e.what());
    }
    FileCommit(fileout.Get());
    fileout.fclose();

    // replace existing peers.dat, if any, with new peers.dat.XXXX
    if (!RenameOver(pathTmp, pathAddr))
        return error("%s : Rename-into-place failed", __func__);

    return true;
}

bool CAddrDB::Read(CAddrMan& addr)
{
    // open input file, and associate with CAutoFile
    FILE *file = fopen(pathAddr.string().c_str(), "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("%s : Failed to open file %s", __func__, pathAddr.string());

    // use file size to size memory buffer
    int fileSize = boost::filesystem::file_size(pathAddr);
    int dataSize = fileSize - sizeof(uint256);
    // Don't try to resize to a negative number if file is small
    if (dataSize < 0)
        dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }
    filein.fclose();

    CDataStream ssPeers(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssPeers.begin(), ssPeers.end());
    if (hashIn != hashTmp)
        return error("%s : Checksum mismatch, data corrupted", __func__);

    unsigned char pchMsgTmp[4];
    try {
        // de-serialize file header (network specific magic number) and ..
        ssPeers >> FLATDATA(pchMsgTmp);

        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
            return error("%s : Invalid network magic number", __func__);

        // de-serialize address data into one CAddrMan object
        ssPeers >> addr;
    }
    catch (std::exception &e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

unsigned int ReceiveFloodSize() { return 1000*GetArg("-maxreceivebuffer", 5*1000); }
unsigned int SendBufferSize() { return 1000*GetArg("-maxsendbuffer", 1*1000); }

//! As i2p addrs are MUCH larger than ip addresses, we're reducing the most-recently-used(mru) setAddrKnown to 1250, to have a smaller memory profile per node.
CNode::CNode(SOCKET hSocketIn, CAddress addrIn, std::string addrNameIn, bool fInboundIn) :
    ssSend(SER_NETWORK, INIT_PROTO_VERSION),
    addrKnown(1250, 0.001, insecure_rand())
    {
    //! Protocol 70009 changes the node creation process so it is deterministic.
    //! Every node starts out with an IP only stream type, except for I2P addresses, they are set immediately to a full size address space.
    //! During the version/verack processing cycle the stream type is evaluated based on information obtained from the peer.  Older protocols lie
    //! and may incorrectly report services depending on how they were built.  Some send full version address messages, and must have the
    //! stream type changed on the fly, before processing.  70008 has a bug, where the stream type is full size, but only contains an ip address
    //! over clearnet.  Protocol 70006 builds for clearnet lie about their support of the NODE_I2P service, while in fact the address object size
    //! is only large enough for an ip address.
    //! If the address given to us is a non-null string, the caller needs to have evaluated and setup that string correctly before calling this.
    //! It is no longer meaningful, and should be abandoned as an input parameter, it only serves to confusion and cloud the many issues involved
    //! in the node creation process.  Here it is assigned to the addrName field, which maybe useful in debugging problems, if it shows up incorrectly.
    //! Also note, all the subclasses vRecv, ssSend & hdrbuf get defined here  the same stream type during this initialization.
#ifdef ENABLE_I2PSAM
    nStreamType = SER_NETWORK | (addrIn.IsNativeI2P() ? 0 : SER_IPADDRONLY);
#else
    nStreamType = SER_NETWORK;
#endif
    SetSendStreamType( nStreamType );
    SetRecvStreamType( nStreamType );
    ssSend.SetType( nStreamType );
    nServices = 0;
    hSocket = hSocketIn;
    nRecvVersion = INIT_PROTO_VERSION;
    nLastSend = 0;
    nLastRecv = 0;
    nSendBytes = 0;
    nRecvBytes = 0;
    nTimeConnected = GetTime();
    addr = addrIn;
    addrName = addrNameIn.size() ? addrNameIn : addr.ToStringIPPort();
    nVersion = 0;
    strSubVer = "";
    fWhitelisted = false;
    fOneShot = false;
    fClient = false; // set by version message
    fInbound = fInboundIn;
    fNetworkNode = false;
    fSuccessfullyConnected = false;
    fDisconnect = false;
    nRefCount = 0;
    nSendSize = 0;
    nSendOffset = 0;
    hashContinue = 0;
    nStartingHeight = -1;
    fGetAddr = false;
    fRelayTxes = false;
    setInventoryKnown.max_size(SendBufferSize() / 1000);
    pfilter = new CBloomFilter();
    nPingNonceSent = 0;
    nPingUsecStart = 0;
    nPingUsecTime = 0;
    fPingQueued = false;

    {
        LOCK(cs_nLastNodeId);
        id = nLastNodeId++;
    }
    LogPrint("net", "Added connection to ");
    if (fLogIPs)
        LogPrint("net", "%s peer=%d\n", addrName, id);
    else
        LogPrint("net", "peer id=%d\n", id);

    // Be shy and don't send version until we hear, unless its valid and outbound, then we go ahead and push our version...
    if (hSocket != INVALID_SOCKET && !fInbound)
        PushVersion();

    GetNodeSignals().InitializeNode(GetId(), this);
}

CNode::~CNode()
{
    CloseSocket(hSocket);

    if (pfilter)
        delete pfilter;

    GetNodeSignals().FinalizeNode(GetId());
}

void CNode::AskFor(const CInv& inv)
{
    if (mapAskFor.size() > MAPASKFOR_MAX_SZ)
        return;
    // We're using mapAskFor as a priority queue,
    // the key is the earliest time the request can be sent
    int64_t nRequestTime;
    limitedmap<CInv, int64_t>::const_iterator it = mapAlreadyAskedFor.find(inv);
    if (it != mapAlreadyAskedFor.end())
        nRequestTime = it->second;
    else
        nRequestTime = 0;
    LogPrint("net", "askfor %s  %d (%s) from %s\n", inv.ToString(), nRequestTime, DateTimeStrFormat("%H:%M:%S", nRequestTime/1000000), GetPeerLogStr(this));

    // Make sure not to reuse time indexes to keep things in the same order
    int64_t nNow = GetTimeMicros() - 1000000;
    static int64_t nLastTime;
    ++nLastTime;
    nNow = std::max(nNow, nLastTime);
    nLastTime = nNow;

    // Each retry is 2 minutes after the last
    nRequestTime = std::max(nRequestTime + 2 * 60 * 1000000, nNow);
    if (it != mapAlreadyAskedFor.end())
        mapAlreadyAskedFor.update(it, nRequestTime);
    else
        mapAlreadyAskedFor.insert(std::make_pair(inv, nRequestTime));
    mapAskFor.insert(std::make_pair(nRequestTime, inv));
}

void CNode::BeginMessage(const char* pszCommand) EXCLUSIVE_LOCK_FUNCTION(cs_vSend)
{
    ENTER_CRITICAL_SECTION(cs_vSend);
    assert(ssSend.size() == 0);
    ssSend << CMessageHeader(pszCommand, 0);
    LogPrint( "net", "sending: %s ", SanitizeString(pszCommand) );
}

void CNode::AbortMessage() UNLOCK_FUNCTION(cs_vSend)
{
    ssSend.clear();

    LEAVE_CRITICAL_SECTION(cs_vSend);

    LogPrint("net", "(aborted)\n");
}

void CNode::EndMessage() UNLOCK_FUNCTION(cs_vSend)
{
    // The -*messagestest options are intentionally not documented in the help message,
    // since they are only used during development to debug the networking code and are
    // not intended for end-users.
    if (mapArgs.count("-dropmessagestest") && GetRand(GetArg("-dropmessagestest", 2)) == 0)
    {
        LogPrint("net", "dropmessages DROPPING SEND MESSAGE\n");
        AbortMessage();
        return;
    }
    if (mapArgs.count("-fuzzmessagestest"))
        Fuzz(GetArg("-fuzzmessagestest", 10));

    if (ssSend.size() == 0)
        return;

    // Set the size
    unsigned int nSize = ssSend.size() - CMessageHeader::HEADER_SIZE;
    memcpy((char*)&ssSend[CMessageHeader::MESSAGE_SIZE_OFFSET], &nSize, sizeof(nSize));

    // Set the checksum
    uint256 hash = Hash(ssSend.begin() + CMessageHeader::HEADER_SIZE, ssSend.end());
    unsigned int nChecksum = 0;
    memcpy(&nChecksum, &hash, sizeof(nChecksum));
    assert(ssSend.size () >= CMessageHeader::CHECKSUM_OFFSET + sizeof(nChecksum));
    memcpy((char*)&ssSend[CMessageHeader::CHECKSUM_OFFSET], &nChecksum, sizeof(nChecksum));

    LogPrint( "net", "(%d bytes) to %s\n", nSize, GetPeerLogStr(this) );

    std::deque<CSerializeData>::iterator it = vSendMsg.insert(vSendMsg.end(), CSerializeData());
    ssSend.GetAndClear(*it);
    nSendSize += (*it).size();

    // If write queue empty, attempt "optimistic write"
    if (it == vSendMsg.begin())
        SocketSendData(this);

    LEAVE_CRITICAL_SECTION(cs_vSend);
}

string GetPeerLogStr( const CNode* pfrom )
{
    string remoteAddr = _("peer");

    if( pfrom != NULL ) {
        remoteAddr += strprintf( "=%d", pfrom->id );
        if( fLogIPs ) {
            remoteAddr += ", ";
            remoteAddr += _("peeraddr");
            remoteAddr += strprintf( "=%s", pfrom->addr.ToString() );
        }
    } else
        remoteAddr += "=??? ERROR invalid Node Ptr, please notify dev team";

    return remoteAddr;
}
