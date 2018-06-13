// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netbase.h"

#include "addrman.h"        // For looking up b32.i2p addresses as base64 i2p destinations
#include "hash.h"
#include "sync.h"
#include "ui_interface.h"
#include "uint256.h"
#include "util.h"

#ifdef HAVE_GETADDRINFO_A
#include <netdb.h>
#endif

#ifndef WIN32
#if HAVE_INET_PTON
#include <arpa/inet.h>
#endif
#include <fcntl.h>
#endif

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/thread.hpp>
#include <openssl/sha.h>

#if !defined(HAVE_MSG_NOSIGNAL) && !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

using namespace std;

//! Constants found in this source codes header(.h)
/** -timeout default */
const int32_t DEFAULT_CONNECT_TIMEOUT = 20000;
//! ToDo: Further analysis from debugging i2p connections may reveal that this setting needs more adjustment.
//!       Connected peer table entries show ping times >13000ms  20000 sets this number's default
//!       to 4 times what btc had (5000) .. Or possibly we need to add a new variable for I2p specifically.

// Settings
static proxyType proxyInfo[NET_MAX];
static CService nameProxy;
static CCriticalSection cs_proxyInfos;
int nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;      //! See netbase.h for setting value, i2p requires setting this higher
bool fNameLookup = false;
CAddrMan addrman;               //! This must be here, not in net.cpp so that lib_common can contain CAddrman code, as well as timedata

static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };

// Need ample time for negotiation for very slow proxies such as Tor (milliseconds)
static const int SOCKS5_RECV_TIMEOUT = 20 * 1000;

enum Network ParseNetwork(std::string net) {
    boost::to_lower(net);
    if (net == "ipv4") return NET_IPV4;
    if (net == "ipv6") return NET_IPV6;
    if (net == "tor" || net == "onion")  return NET_TOR;
    if (net == "i2p") return NET_I2P;
    return NET_UNROUTABLE;
}

std::string GetNetworkName(enum Network net) {
    switch(net)
    {
    case NET_IPV4: return "ipv4";
    case NET_IPV6: return "ipv6";
    case NET_TOR : return "tor";
    case NET_I2P: return "i2p";

    default: return "???";
    }
}

void SplitHostPort(std::string in, int &portOut, std::string &hostOut) {
    size_t colon = in.find_last_of(':');
    // if a : is found, and it either follows a [...], or no other : is in the string, treat it as port separator
    bool fHaveColon = colon != in.npos;
    bool fBracketed = fHaveColon && (in[0]=='[' && in[colon-1]==']'); // if there is a colon, and in[0]=='[', colon is not 0, so in[colon-1] is safe
    bool fMultiColon = fHaveColon && (in.find_last_of(':',colon-1) != in.npos);
    if (fHaveColon && (colon==0 || fBracketed || !fMultiColon)) {
        int32_t n;
        if (ParseInt32(in.substr(colon + 1), &n) && n > 0 && n < 0x10000) {
            in = in.substr(0, colon);
            portOut = n;
        }
    }
    if (in.size()>0 && in[0] == '[' && in[in.size()-1] == ']')
        hostOut = in.substr(1, in.size()-2);
    else
        hostOut = in;
}

bool static LookupIntern(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions, bool fAllowLookup)
{
    vIP.clear();

    {
        CNetAddr addr;
        std::string strName( pszName );
        if (addr.SetSpecial(strName)) {
            vIP.push_back(addr);
            return true;
        }
        // The problem is:  if SetSpecial returns false, we don't know why it failed or what happen
        else if( isStringI2pDestination( strName ) ) {
            // LogPrintf( "...." );  so SetSpecial now has extensive logging support of errors, need more put it here
            return false;   // we're done here, a dns seed node could not be found or any I2P type destination address failed to be found
        }
    }

#ifdef HAVE_GETADDRINFO_A
    struct in_addr ipv4_addr;
#ifdef HAVE_INET_PTON
    if (inet_pton(AF_INET, pszName, &ipv4_addr) > 0) {
        vIP.push_back(CNetAddr(ipv4_addr));
        return true;
    }

    struct in6_addr ipv6_addr;
    if (inet_pton(AF_INET6, pszName, &ipv6_addr) > 0) {
        vIP.push_back(CNetAddr(ipv6_addr));
        return true;
    }
#else
    ipv4_addr.s_addr = inet_addr(pszName);
    if (ipv4_addr.s_addr != INADDR_NONE) {
        vIP.push_back(CNetAddr(ipv4_addr));
        return true;
    }
#endif
#endif

    struct addrinfo aiHint;
    memset(&aiHint, 0, sizeof(struct addrinfo));
    aiHint.ai_socktype = SOCK_STREAM;
    aiHint.ai_protocol = IPPROTO_TCP;
    aiHint.ai_family = AF_UNSPEC;
#ifdef WIN32
    aiHint.ai_flags = fAllowLookup ? 0 : AI_NUMERICHOST;
#else
    aiHint.ai_flags = fAllowLookup ? AI_ADDRCONFIG : AI_NUMERICHOST;
#endif

    struct addrinfo *aiRes = NULL;
#ifdef HAVE_GETADDRINFO_A
    struct gaicb gcb, *query = &gcb;
    memset(query, 0, sizeof(struct gaicb));
    gcb.ar_name = pszName;
    gcb.ar_request = &aiHint;
    int nErr = getaddrinfo_a(GAI_NOWAIT, &query, 1, NULL);
    if (nErr)
        return false;

    do {
        // Should set the timeout limit to a resonable value to avoid
        // generating unnecessary checking call during the polling loop,
        // while it can still response to stop request quick enough.
        // 2 seconds looks fine in our situation.
        struct timespec ts = { 2, 0 };
        gai_suspend(&query, 1, &ts);
        boost::this_thread::interruption_point();

        nErr = gai_error(query);
        if (0 == nErr)
            aiRes = query->ar_result;
    } while (nErr == EAI_INPROGRESS);
#else
    int nErr = getaddrinfo(pszName, NULL, &aiHint, &aiRes);
#endif
    if (nErr)
        return false;

    struct addrinfo *aiTrav = aiRes;
    while (aiTrav != NULL && (nMaxSolutions == 0 || vIP.size() < nMaxSolutions))
    {
        if (aiTrav->ai_family == AF_INET)
        {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in));
            vIP.push_back(CNetAddr(((struct sockaddr_in*)(aiTrav->ai_addr))->sin_addr));
        }

        if (aiTrav->ai_family == AF_INET6)
        {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in6));
            vIP.push_back(CNetAddr(((struct sockaddr_in6*)(aiTrav->ai_addr))->sin6_addr));
        }

        aiTrav = aiTrav->ai_next;
    }

    freeaddrinfo(aiRes);

    return (vIP.size() > 0);
}

bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions, bool fAllowLookup)
{
    std::string strHost(pszName);
    if (strHost.empty())
        return false;
    if (boost::algorithm::starts_with(strHost, "[") && boost::algorithm::ends_with(strHost, "]"))
    {
        strHost = strHost.substr(1, strHost.size() - 2);
    }

    return LookupIntern(strHost.c_str(), vIP, nMaxSolutions, fAllowLookup);
}

bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault, bool fAllowLookup, unsigned int nMaxSolutions)
{
    if (pszName[0] == 0)
        return false;
    int port = portDefault;
    std::string hostname = "";
    SplitHostPort(std::string(pszName), port, hostname);            // SplitHostPort also removes leading and trailing [] brackets

    std::vector<CNetAddr> vIP;
    bool fRet = LookupIntern(hostname.c_str(), vIP, nMaxSolutions, fAllowLookup);
    if (!fRet)
        return false;
    vAddr.resize(vIP.size());
    for (unsigned int i = 0; i < vIP.size(); i++)
        vAddr[i] = CService(vIP[i], port);
    return true;
}

bool Lookup(const char *pszName, CService& addr, int portDefault, bool fAllowLookup)
{
    std::vector<CService> vService;
    bool fRet = Lookup(pszName, vService, portDefault, fAllowLookup, 1);
    if (!fRet)
        return false;
    addr = vService[0];
    return true;
}

bool LookupNumeric(const char *pszName, CService& addr, int portDefault)
{
    return Lookup(pszName, addr, portDefault, false);
}

/**
 * Convert milliseconds to a struct timeval for select.
 */
struct timeval static MillisToTimeval(int64_t nTimeout)
{
    struct timeval timeout;
    timeout.tv_sec  = nTimeout / 1000;
    timeout.tv_usec = (nTimeout % 1000) * 1000;
    return timeout;
}

/**
 * Read bytes from socket. This will either read the full number of bytes requested
 * or return False on error or timeout.
 * This function can be interrupted by boost thread interrupt.
 *
 * @param data Buffer to receive into
 * @param len  Length of data to receive
 * @param timeout  Timeout in milliseconds for receive operation
 *
 * @note This function requires that hSocket is in non-blocking mode.
 */
bool static InterruptibleRecv(char* data, size_t len, int timeout, SOCKET& hSocket)
{
    int64_t curTime = GetTimeMillis();
    int64_t endTime = curTime + timeout;
    // Maximum time to wait in one select call. It will take up until this time (in millis)
    // to break off in case of an interruption.
    const int64_t maxWait = 1000;
    while (len > 0 && curTime < endTime) {
        ssize_t ret = recv(hSocket, data, len, 0); // Optimistically try the recv first
        if (ret > 0) {
            len -= ret;
            data += ret;
        } else if (ret == 0) { // Unexpected disconnection
            return false;
        } else { // Other error or blocking
            int nErr = WSAGetLastError();
            if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK || nErr == WSAEINVAL) {
                struct timeval tval = MillisToTimeval(std::min(endTime - curTime, maxWait));
                fd_set fdset;
                FD_ZERO(&fdset);
                FD_SET(hSocket, &fdset);
                int nRet = select(hSocket + 1, &fdset, NULL, NULL, &tval);
                if (nRet == SOCKET_ERROR) {
                    return false;
                }
            } else {
                return false;
            }
        }
        boost::this_thread::interruption_point();
        curTime = GetTimeMillis();
    }
    return len == 0;
}

struct ProxyCredentials
{
    std::string username;
    std::string password;
};

/** Connect using SOCKS5 (as described in RFC1928) */
// static bool Socks5(const std::string& strDest, int port, const ProxyCredentials *auth, SOCKET& hSocket)
bool static Socks5(string strDest, int port, SOCKET& hSocket)
{
    ProxyCredentials* auth = NULL;                  // Do no yet support authentication ToDo:

    LogPrintf("SOCKS5 connecting %s\n", strDest);
    if (strDest.size() > 255) {
        CloseSocket(hSocket);
        return error("Hostname too long");
    }
    // Accepted authentication methods
    std::vector<uint8_t> vSocks5Init;
    vSocks5Init.push_back(0x05);
    if (auth) {
        vSocks5Init.push_back(0x02); // # METHODS
        vSocks5Init.push_back(0x00); // X'00' NO AUTHENTICATION REQUIRED
        vSocks5Init.push_back(0x02); // X'02' USERNAME/PASSWORD (RFC1929)
    } else {
        vSocks5Init.push_back(0x01); // # METHODS
        vSocks5Init.push_back(0x00); // X'00' NO AUTHENTICATION REQUIRED
    }
    ssize_t ret = send(hSocket, (const char*)begin_ptr(vSocks5Init), vSocks5Init.size(), MSG_NOSIGNAL);
    if (ret != (ssize_t)vSocks5Init.size()) {
        CloseSocket(hSocket);
        return error("Error sending to proxy");
    }
    char pchRet1[2];
    if (!InterruptibleRecv(pchRet1, 2, SOCKS5_RECV_TIMEOUT, hSocket)) {
        CloseSocket(hSocket);
        return error("Error reading proxy response");
    }
    if (pchRet1[0] != 0x05) {
        CloseSocket(hSocket);
        return error("Proxy failed to initialize");
    }
    if (pchRet1[1] == 0x02 && auth) {
        // Perform username/password authentication (as described in RFC1929)
        std::vector<uint8_t> vAuth;
        vAuth.push_back(0x01);
        if (auth->username.size() > 255 || auth->password.size() > 255)
            return error("Proxy username or password too long");
        vAuth.push_back(auth->username.size());
        vAuth.insert(vAuth.end(), auth->username.begin(), auth->username.end());
        vAuth.push_back(auth->password.size());
        vAuth.insert(vAuth.end(), auth->password.begin(), auth->password.end());
        ret = send(hSocket, (const char*)begin_ptr(vAuth), vAuth.size(), MSG_NOSIGNAL);
        if (ret != (ssize_t)vAuth.size()) {
            CloseSocket(hSocket);
            return error("Error sending authentication to proxy");
        }
        LogPrint("proxy", "SOCKS5 sending proxy authentication %s:%s\n", auth->username, auth->password);
        char pchRetA[2];
        if (!InterruptibleRecv(pchRetA, 2, SOCKS5_RECV_TIMEOUT, hSocket)) {
            CloseSocket(hSocket);
            return error("Error reading proxy authentication response");
        }
        if (pchRetA[0] != 0x01 || pchRetA[1] != 0x00) {
            CloseSocket(hSocket);
            return error("Proxy authentication unsuccesful");
        }
    } else if (pchRet1[1] == 0x00) {
        // Perform no authentication
    } else {
        CloseSocket(hSocket);
        return error("Proxy requested wrong authentication method %02x", pchRet1[1]);
    }
    std::vector<uint8_t> vSocks5;
    vSocks5.push_back(0x05); // VER protocol version
    vSocks5.push_back(0x01); // CMD CONNECT
    vSocks5.push_back(0x00); // RSV Reserved
    vSocks5.push_back(0x03); // ATYP DOMAINNAME
    vSocks5.push_back(strDest.size()); // Length<=255 is checked at beginning of function
    vSocks5.insert(vSocks5.end(), strDest.begin(), strDest.end());
    vSocks5.push_back((port >> 8) & 0xFF);
    vSocks5.push_back((port >> 0) & 0xFF);
    ret = send(hSocket, (const char*)begin_ptr(vSocks5), vSocks5.size(), MSG_NOSIGNAL);
    if (ret != (ssize_t)vSocks5.size()) {
        CloseSocket(hSocket);
        return error("Error sending to proxy");
    }
    char pchRet2[4];
    if (!InterruptibleRecv(pchRet2, 4, SOCKS5_RECV_TIMEOUT, hSocket)) {
        CloseSocket(hSocket);
        return error("Error reading proxy response");
    }
    if (pchRet2[0] != 0x05) {
        CloseSocket(hSocket);
        return error("Proxy failed to accept request");
    }
    if (pchRet2[1] != 0x00) {
        CloseSocket(hSocket);
        switch (pchRet2[1])
        {
            case 0x01: return error("Proxy error: general failure");
            case 0x02: return error("Proxy error: connection not allowed");
            case 0x03: return error("Proxy error: network unreachable");
            case 0x04: return error("Proxy error: host unreachable");
            case 0x05: return error("Proxy error: connection refused");
            case 0x06: return error("Proxy error: TTL expired");
            case 0x07: return error("Proxy error: protocol error");
            case 0x08: return error("Proxy error: address type not supported");
            default:   return error("Proxy error: unknown");
        }
    }
    if (pchRet2[2] != 0x00) {
        CloseSocket(hSocket);
        return error("Error: malformed proxy response");
    }
    char pchRet3[256];
    switch (pchRet2[3])
    {
        case 0x01: ret = InterruptibleRecv(pchRet3, 4, SOCKS5_RECV_TIMEOUT, hSocket); break;
        case 0x04: ret = InterruptibleRecv(pchRet3, 16, SOCKS5_RECV_TIMEOUT, hSocket); break;
        case 0x03:
        {
            ret = InterruptibleRecv(pchRet3, 1, SOCKS5_RECV_TIMEOUT, hSocket);
            if (!ret) {
                CloseSocket(hSocket);
                return error("Error reading from proxy");
            }
            int nRecv = pchRet3[0];
            ret = InterruptibleRecv(pchRet3, nRecv, SOCKS5_RECV_TIMEOUT, hSocket);
            break;
        }
        default: CloseSocket(hSocket); return error("Error: malformed proxy response");
    }
    if (!ret) {
        CloseSocket(hSocket);
        return error("Error reading from proxy");
    }
    if (!InterruptibleRecv(pchRet3, 2, SOCKS5_RECV_TIMEOUT, hSocket)) {
        CloseSocket(hSocket);
        return error("Error reading from proxy");
    }
    LogPrintf("SOCKS5 connected %s\n", strDest);
    return true;
}

bool static ConnectSocketDirectly(const CService &addrConnect, SOCKET& hSocketRet, int nTimeout)
{
    hSocketRet = INVALID_SOCKET;

    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrConnect.GetSockAddr((struct sockaddr*)&sockaddr, &len)) {
        LogPrintf("Cannot connect to %s: unsupported network\n", addrConnect.ToString());
        return false;
    }

    SOCKET hSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hSocket == INVALID_SOCKET)
        return false;

#ifdef SO_NOSIGPIPE
    int set = 1;
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif

    // Set to non-blocking
    if (!SetSocketNonBlocking(hSocket, true))
        return error("ConnectSocketDirectly: Setting socket to non-blocking failed, error %s\n", NetworkErrorString(WSAGetLastError()));

    if (connect(hSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        // WSAEINVAL is here because some legacy version of winsock uses it
        if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK || nErr == WSAEINVAL)
        {
            struct timeval timeout = MillisToTimeval(nTimeout);
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(hSocket, &fdset);
            int nRet = select(hSocket + 1, NULL, &fdset, NULL, &timeout);
            if (nRet == 0)
            {
                LogPrint("net", "connection to %s timeout\n", addrConnect.ToString());
                CloseSocket(hSocket);
                return false;
            }
            if (nRet == SOCKET_ERROR)
            {
                LogPrintf("select() for %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
                CloseSocket(hSocket);
                return false;
            }
            socklen_t nRetSize = sizeof(nRet);
#ifdef WIN32
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, (char*)(&nRet), &nRetSize) == SOCKET_ERROR)
#else
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR, &nRet, &nRetSize) == SOCKET_ERROR)
#endif
            {
                LogPrintf("getsockopt() for %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
                CloseSocket(hSocket);
                return false;
            }
            if (nRet != 0)
            {
                LogPrintf("connect() to %s failed after select(): %s\n", addrConnect.ToString(), NetworkErrorString(nRet));
                CloseSocket(hSocket);
                return false;
            }
        }
#ifdef WIN32
        else if (WSAGetLastError() != WSAEISCONN)
#else
        else
#endif
        {
            LogPrintf("connect() to %s failed: %s\n", addrConnect.ToString(), NetworkErrorString(WSAGetLastError()));
            CloseSocket(hSocket);
            return false;
        }
    }

    hSocketRet = hSocket;
    return true;
}

bool SetProxy(enum Network net, CService addrProxy) {
    assert(net >= 0 && net < NET_MAX);
    if (!addrProxy.IsValid())
        return false;
    LOCK(cs_proxyInfos);
    proxyInfo[net] = addrProxy;
    return true;
}

bool GetProxy(enum Network net, proxyType &proxyInfoOut) {
    assert(net >= 0 && net < NET_MAX);
    LOCK(cs_proxyInfos);
    if (!proxyInfo[net].IsValid())
        return false;
    proxyInfoOut = proxyInfo[net];
    return true;
}

bool SetNameProxy(CService addrProxy) {
    if (!addrProxy.IsValid())
        return false;
    LOCK(cs_proxyInfos);
    nameProxy = addrProxy;
    return true;
}

bool GetNameProxy(CService &nameProxyOut) {
    LOCK(cs_proxyInfos);
    if(!nameProxy.IsValid())
        return false;
    nameProxyOut = nameProxy;
    return true;
}

bool HaveNameProxy() {
    LOCK(cs_proxyInfos);
    return nameProxy.IsValid();
}

bool IsProxy(const CNetAddr &addr) {
    LOCK(cs_proxyInfos);
    for (int i = 0; i < NET_MAX; i++) {
        if (addr == (CNetAddr)proxyInfo[i])
            return true;
    }
    return false;
}

#if defined( DONT_COMPILE )
static bool ConnectThroughProxy(const proxyType &proxy, const std::string& strDest, int port, SOCKET& hSocketRet, int nTimeout, bool *outProxyConnectionFailed)
{
    SOCKET hSocket = INVALID_SOCKET;
    // first connect to proxy server
    if (!ConnectSocketDirectly(proxy.proxy, hSocket, nTimeout)) {
        if (outProxyConnectionFailed)
            *outProxyConnectionFailed = true;
        return false;
    }
    // do socks negotiation
    if (proxy.randomize_credentials) {
        ProxyCredentials random_auth;
        random_auth.username = strprintf("%i", insecure_rand());
        random_auth.password = strprintf("%i", insecure_rand());
        if (!Socks5(strDest, (unsigned short)port, &random_auth, hSocket))
            return false;
    } else {
        if (!Socks5(strDest, (unsigned short)port, 0, hSocket))
            return false;
    }

    hSocketRet = hSocket;
    return true;
}
#endif // defined

bool ConnectSocket(const CService &addrDest, SOCKET& hSocketRet, int nTimeout, bool *outProxyConnectionFailed)
{
    proxyType proxy;                                                    //! Not needed by i2p, but might be for clearnet or tor

    if (outProxyConnectionFailed) *outProxyConnectionFailed = false;    //! Assume success
#ifdef ENABLE_I2PSAM
    if( addrDest.IsI2P() ) {
        assert( addrDest.IsNativeI2P() );
        SOCKET streamSocket = I2PSession::Instance().connect(addrDest.GetI2pDestination(), false/*, streamSocket*/);
        if (SetSocketNonBlocking(streamSocket, true)) {                 //! Set to non-blocking
            hSocketRet = streamSocket;
            return true;
        }
        hSocketRet = INVALID_SOCKET;
        return false;
    }
#else
    LogPrintf( "This Build does NOT support I2P Communications, network connection failed for: %s\n", addrDest.ToB32String() );
    if (outProxyConnectionFailed) *outProxyConnectionFailed = true;
    hSocketRet = INVALID_SOCKET;
    return false;
#endif
    // no proxy needed (none set for target network)
    if (!GetProxy(addrDest.GetNetwork(), proxy))
        return ConnectSocketDirectly(addrDest, hSocketRet, nTimeout);

    SOCKET hSocket = INVALID_SOCKET;

    // first connect to proxy server
    if (!ConnectSocketDirectly(proxy, hSocket, nTimeout)) {
        if (outProxyConnectionFailed)
            *outProxyConnectionFailed = true;
        return false;
    }
    // do socks negotiation
    if (!Socks5(addrDest.ToStringIP(), addrDest.GetPort(), hSocket))
        return false;

    hSocketRet = hSocket;
    return true;
}

bool ConnectSocketByName(CService &addr, SOCKET& hSocketRet, const char *pszDest, int portDefault, int nTimeout, bool *outProxyConnectionFailed)
{
    string strDest;
    int port = portDefault;

    if (outProxyConnectionFailed)
        *outProxyConnectionFailed = false;                  //! Assume success

    SplitHostPort(string(pszDest), port, strDest);          //! Also strips off any leading and trailing [ ] characters

    SOCKET hSocket = INVALID_SOCKET;

    CService nameProxy;
    GetNameProxy(nameProxy);

    CService addrResolved(CNetAddr(strDest, fNameLookup && !HaveNameProxy()), port);
    if (addrResolved.IsValid()) {
        addr = addrResolved;
        return ConnectSocket(addr, hSocketRet, nTimeout);
    }

    addr = CService("0.0.0.0:0");

    if (!HaveNameProxy())
        return false;
    // first connect to name proxy server
    if (!ConnectSocketDirectly(nameProxy, hSocket, nTimeout)) {
        if (outProxyConnectionFailed)
            *outProxyConnectionFailed = true;
        return false;
    }
    // do socks negotiation
    if (!Socks5(strDest, (unsigned short)port, hSocket))
        return false;

    hSocketRet = hSocket;
    return true;
}

void CNetAddr::Init()
{
    memset(ip, 0, sizeof(ip));
    memset(i2pDest, 0, I2P_DESTINATION_STORE);
}

void CNetAddr::SetIP(const CNetAddr& ipIn)
{
    memcpy(ip, ipIn.ip, sizeof(ip));
    memcpy(i2pDest, ipIn.i2pDest, I2P_DESTINATION_STORE);
}

void CNetAddr::SetRaw(Network network, const uint8_t *ip_in)
{
    switch(network)
    {
        case NET_IPV4:
            memcpy(ip, pchIPv4, 12);
            memcpy(ip+12, ip_in, 4);
            break;
        case NET_IPV6:
            memcpy(ip, ip_in, 16);
            break;
        default:
            assert(!"invalid network");
    }
    memset(i2pDest, 0, I2P_DESTINATION_STORE);
}

static const unsigned char pchOnionCat[] = {0xFD,0x87,0xD8,0x7E,0xEB,0x43};

/** \brief
    Implementing support for i2p addresses by using a similar idea as tor(aka onion addrs) so based on the ideas found here:
    https://www.cypherpunk.at/onioncat_trac/wiki/GarliCat
    we're going to use the beginning bytes of the ip address to indicate i2p destination is the payload here.
 *
 */
static const unsigned char pchGarlicCat[] = { 0xFD,0x60, 0xDB,0x4D, 0xDD,0xB5 };        // A /48 ip6 prefix for I2P destinations...

/**
 * Returns TRUE if the address name can be looked up and resolved
 SetSpecial is the workhorse routine which handles the I2P b32.i2p and full base64 destination addresses.  If dns is true, base32 address lookups are
 allowed and done as well.  New protocol 70009+ layer definition introduces the concept of a pchGarlicCat address held in the ip storage area, as the
 new primary way to know this address is FOR the i2p network, and not some other destination.  That is built here as well.
 */
bool CNetAddr::SetSpecial(const std::string &strName)
{
    // Any address that ends in b32.i2p should be valid here, as the router itself is used to lookup the base64 destination, it returns without an address
    // if the string can not be found, if it's a base64 address, we can use it as is, to setup a new correctly formated CNetAddr object
    if( isStringI2pDestination( strName ) )                                      // Perhaps we've been given a I2P address, they come in 2 different ways
    {                                                                       // We're given a possible valid .b32.i2p address or a native I2P destination
        string addr;
        if( isValidI2pB32( strName ) ) {
            // 1st try our new local address book for the lookup....
            // NOTE: Adding this line of code was extremely expensive for the developer, it broke the build system, which could no longer link
            // and create the Anoncoin-cli executable.  It requires that you link in the addrman.cpp/h module, as its now included here in netbase,
            // This had  never been done until now.  You will be required to upgrade your code to support the chainparamsbase.cpp/h module concept,
            // and remove all references to chainparams.cpp/h in your build script, if your going to try and just add this one line of code.
            //  Upgrading your source base to v10 technology was the solution, but very time consuming, expensive and stressful.  It may appear to
            // be just one line of code, but it is not nearly as simple as that when your starting from and working with a v9 source code base.
            // ...GR
            addr = addrman.GetI2pBase64Destination( strName );
            if( IsI2PEnabled() && fNameLookup ) {                           // Check for dns set, we should at least log the error, if not.
                int64_t iNow = GetTime();
                if( !addr.size() )                                          // If we couldn't find it, much more to do..
#ifdef ENABLE_I2PSAM
                    addr = I2PSession::Instance().namingLookup(strName);    // Expensive, but lets try, this could take a very long while...
#else
                    LogPrintf( "This Build does NOT support I2P Communications, network lookup failed for: %s\n", strName );
#endif
                else
                    LogPrintf( "The i2p destination %s was found locally.\n", strName );
                // If the address returned is a non-zero length string, the lookup was successful
                if( !isValidI2pAddress( addr ) ) {                          // Not sure why, but that shouldn't happen, could be a 'new' destination type we can't yet handle
                    LogPrintf( "After %d secs looking, even the i2p router was unable to locate %s\n", GetTime() - iNow, strName );
                    return false;                                           // Not some thing we can use
                } // else  // Otherwise the AddrMan or I2P router was able to find an I2P destination for this address, and it's now stored in 'addr' as a base64 string
                    // LogPrintf( "AddrMan or I2P Router lookup found [%s] address as destination\n[%s]\n", strName, addr );
            } else {                                                        // Log should tell the user they have DNS turned off, so this can't work
                LogPrintf( "Unable to locate %s, no i2p router enabled or dns=0\n", strName );
                return false;
            }
        } else                                                              // It was a native I2P address to begin with
            addr = strName;                                                 // Prep for memcpy()
        // If we make it here 'addr' has i2p destination address as a base 64 string...
        // Now we can build the output array of bytes as we need for protocol 70009+ by using the concept of a IP6 string we call pchGarlicCat
        memcpy(ip, pchGarlicCat, sizeof(pchGarlicCat));
        memcpy(i2pDest, addr.c_str(), I2P_DESTINATION_STORE);         // So now copy it to our CNetAddr object variable
        return true;                                                        // Special handling taken care of
    }

    if (strName.size()>6 && strName.substr(strName.size() - 6, 6) == ".onion") {
        std::vector<unsigned char> vchAddr = DecodeBase32(strName.substr(0, strName.size() - 6).c_str());
        if (vchAddr.size() != 16-sizeof(pchOnionCat))
            return false;
        memcpy(ip, pchOnionCat, sizeof(pchOnionCat));
        for (unsigned int i=0; i<16-sizeof(pchOnionCat); i++)
            ip[i + sizeof(pchOnionCat)] = vchAddr[i];
        return true;
    }
    return false;
}

CNetAddr::CNetAddr()
{
    Init();
}

CNetAddr::CNetAddr(const struct in_addr& ipv4Addr)
{
    SetRaw(NET_IPV4, (const uint8_t*)&ipv4Addr);
}

CNetAddr::CNetAddr(const struct in6_addr& ipv6Addr)
{
    SetRaw(NET_IPV6, (const uint8_t*)&ipv6Addr);
}

CNetAddr::CNetAddr(const char *pszIp, bool fAllowLookup)
{
    Init();
    std::vector<CNetAddr> vIP;
    if (LookupHost(pszIp, vIP, 1, fAllowLookup))
        *this = vIP[0];
}

CNetAddr::CNetAddr(const std::string &strIp, bool fAllowLookup)
{
    Init();
    std::vector<CNetAddr> vIP;
    // LookupHost->LookupIntern->SetSpecial happens for i2p destinations
    if (LookupHost(strIp.c_str(), vIP, 1, fAllowLookup))
        *this = vIP[0];
}

unsigned int CNetAddr::GetByte(int n) const
{
    return ip[15-n];
}

bool CNetAddr::IsIPv4() const
{
    return (memcmp(ip, pchIPv4, sizeof(pchIPv4)) == 0);
}

bool CNetAddr::IsIPv6() const
{
    return (!IsIPv4() && !IsTor() && !IsI2P());
}

bool CNetAddr::IsRFC1918() const
{
    return IsIPv4() && (
        GetByte(3) == 10 ||
        (GetByte(3) == 192 && GetByte(2) == 168) ||
        (GetByte(3) == 172 && (GetByte(2) >= 16 && GetByte(2) <= 31)));
}

bool CNetAddr::IsRFC2544() const
{
    return IsIPv4() && GetByte(3) == 198 && (GetByte(2) == 18 || GetByte(2) == 19);
}

bool CNetAddr::IsRFC3927() const
{
    return IsIPv4() && (GetByte(3) == 169 && GetByte(2) == 254);
}

bool CNetAddr::IsRFC6598() const
{
    return IsIPv4() && GetByte(3) == 100 && GetByte(2) >= 64 && GetByte(2) <= 127;
}

bool CNetAddr::IsRFC5737() const
{
    return IsIPv4() && ((GetByte(3) == 192 && GetByte(2) == 0 && GetByte(1) == 2) ||
        (GetByte(3) == 198 && GetByte(2) == 51 && GetByte(1) == 100) ||
        (GetByte(3) == 203 && GetByte(2) == 0 && GetByte(1) == 113));
}

bool CNetAddr::IsRFC3849() const
{
    return GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0x0D && GetByte(12) == 0xB8;
}

bool CNetAddr::IsRFC3964() const
{
    return (GetByte(15) == 0x20 && GetByte(14) == 0x02);
}

bool CNetAddr::IsRFC6052() const
{
    static const unsigned char pchRFC6052[] = {0,0x64,0xFF,0x9B,0,0,0,0,0,0,0,0};
    return (memcmp(ip, pchRFC6052, sizeof(pchRFC6052)) == 0);
}

bool CNetAddr::IsRFC4380() const
{
    return (GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0 && GetByte(12) == 0);
}

bool CNetAddr::IsRFC4862() const
{
    static const unsigned char pchRFC4862[] = {0xFE,0x80,0,0,0,0,0,0};
    return (memcmp(ip, pchRFC4862, sizeof(pchRFC4862)) == 0);
}

bool CNetAddr::IsRFC4193() const
{
    return ((GetByte(15) & 0xFE) == 0xFC);
}

bool CNetAddr::IsRFC6145() const
{
    static const unsigned char pchRFC6145[] = {0,0,0,0,0,0,0,0,0xFF,0xFF,0,0};
    return (memcmp(ip, pchRFC6145, sizeof(pchRFC6145)) == 0);
}

bool CNetAddr::IsRFC4843() const
{
    return (GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0x00 && (GetByte(12) & 0xF0) == 0x10);
}

bool CNetAddr::IsTor() const
{
    return (memcmp(ip, pchOnionCat, sizeof(pchOnionCat)) == 0);
}

bool CNetAddr::IsI2P() const
{
    return (memcmp(ip, pchGarlicCat, sizeof(pchGarlicCat)) == 0);
}

bool CNetAddr::IsNativeI2P() const
{
    static const unsigned char pchAAAA[] = {'A','A','A','A'};
    // For unsigned char [] it's quicker here to just do a memory comparison .verses. conversion to a string.
    // In order for this to work however, it's important that the memory has been cleared when this object
    // was created.
    // ToDo: More work could be done here to confirm it will never mistakenly see a valid native i2p address.
    return (i2pDest[0] != 0) && (memcmp(i2pDest + I2P_DESTINATION_STORE - sizeof(pchAAAA), pchAAAA, sizeof(pchAAAA)) == 0);
}

std::string CNetAddr::GetI2pDestination() const
{
    return IsNativeI2P() ? std::string(i2pDest, i2pDest + I2P_DESTINATION_STORE) : std::string();
}

/** \brief Checks for a valid i2p destination, if the garlic field is not set correctly, it makes sure that field is set properly
 *
 * \param void
 * \return bool true if the address object was changed
 *
 */
bool CNetAddr::CheckAndSetGarlicCat( void )
{
    if( IsNativeI2P() && !IsI2P() ) {               // Fix the ip address field
        memset(ip, 0, sizeof(ip));                  // Make sure the ip is completely zeroed out
        memcpy(ip, pchGarlicCat, sizeof(pchGarlicCat));
        return true;
    }
    return false;
}

/** \brief Sets the i2pDest field to the callers given string
 *  The ip field is not touched, if the parameter given is zero length, otherwise it is set to the GarlicCat
 *
 * \param sBase64Dest const std::string
 * \return bool true if the address is now set to a valid i2p destination, otherwise false
 *
 */
bool CNetAddr::SetI2pDestination( const std::string& sBase64Dest )
{
    size_t iSize = sBase64Dest.size();
    if( iSize ) {
        Init();
        memcpy(ip, pchGarlicCat, sizeof(pchGarlicCat));
    } else          // First & always if we're given some non-zero value, Make sure the whole field is zeroed out
        memset(i2pDest, 0, I2P_DESTINATION_STORE);

    // Copy what the caller wants put there, up to the max size
    // Its not going to be valid, if the size is wrong, but do it anyway
    if( iSize ) memcpy( i2pDest, sBase64Dest.c_str(), iSize < I2P_DESTINATION_STORE ? iSize : I2P_DESTINATION_STORE );
    return (iSize == I2P_DESTINATION_STORE) && IsNativeI2P();
}

// Convert this netaddress objects native i2p address into a b32.i2p address
std::string CNetAddr::ToB32String() const
{
    return B32AddressFromDestination( GetI2pDestination() );
}

bool CNetAddr::IsLocal() const
{
    // This address is local if it is the same as the public key of the session we have open,
    // ToDo: Compare the destination address with the session mydestination, this works for
    // now (maybe), but should be done better with getting the info from the i2psam module
    // NOTE:
    // The problem with doing this here, is that AddLocal never makes it on i2p, because
    // this address matches for the IsRouteable() test, causing it to fail.
    // There are two IsLocal(), the other one takes a CService object and works with the
    // mapLocal addresses, this one only stops the local ip addresses from getting into
    // the address stream, and should NOT include a valid i2p destination.
    // if( IsI2P() ) {
    //    string sMyDest = GetArg("-i2p.mydestination.publickey", "");
    //    return sMyDest == GetI2pDestination();
    // }

    // IPv4 loopback
   if (IsIPv4() && (GetByte(3) == 127 || GetByte(3) == 0))
       return true;

   // IPv6 loopback (::1/128)
   static const unsigned char pchLocal[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
   if (memcmp(ip, pchLocal, 16) == 0)
       return true;

   return false;
}

bool CNetAddr::IsMulticast() const
{
    return    (IsIPv4() && (GetByte(3) & 0xF0) == 0xE0)
           || (GetByte(15) == 0xFF);
}

bool CNetAddr::IsValid() const
{
    if( IsI2P() )
        return IsNativeI2P();
    // Cleanup 3-byte shifted addresses caused by garbage in size field
    // of addr messages from versions before 0.2.9 checksum.
    // Two consecutive addr messages look like this:
    // header20 vectorlen3 addr26 addr26 addr26 header20 vectorlen3 addr26 addr26 addr26...
    // so if the first length field is garbled, it reads the second batch
    // of addr misaligned by 3 bytes.
    if (memcmp(ip, pchIPv4+3, sizeof(pchIPv4)-3) == 0)
        return false;

    // unspecified IPv6 address (::/128)
    unsigned char ipNone[16] = {};
    if (memcmp(ip, ipNone, 16) == 0)
        return false;

    // documentation IPv6 address
    if (IsRFC3849())
        return false;

    if (IsIPv4())
    {
        // INADDR_NONE
        uint32_t ipNone = INADDR_NONE;
        if (memcmp(ip+12, &ipNone, 4) == 0)
            return false;

        // 0
        ipNone = 0;
        if (memcmp(ip+12, &ipNone, 4) == 0)
            return false;
    }

    return true;
}

bool CNetAddr::IsRoutable() const
{
    bool fDetermined = IsValid() && !(IsRFC1918() || IsRFC2544() || IsRFC3927() || IsRFC4862() || IsRFC6598() || IsRFC5737() || (IsRFC4193() && !(IsTor() || IsI2P())) || IsRFC4843() || IsLocal());
    /* LogPrintf( "Is this address %s routable? %s  It appears Valid=%s, Local=%s, I2P=%s\n", ToString(),
                  fDetermined ? "YES" : "NO",
                  IsValid() ? "1" : "0",
                  IsLocal() ? "1" : "0",
                  IsI2P() ? "1" : "0");
    LogPrintf( "RFC1918=%s RFC2544=%s RFC3927=%s RFC4862=%s RFC6598=%s RFC5737=%s RFC4193=%s RFC4843=%s\n",
                  IsRFC1918() ? "1" : "0",
                  IsRFC2544() ? "1" : "0",
                  IsRFC3927() ? "1" : "0",
                  IsRFC4862() ? "1" : "0",
                  IsRFC6598() ? "1" : "0",
                  IsRFC5737() ? "1" : "0",
                  IsRFC4193() ? "1" : "0",
                  IsRFC4843() ? "1" : "0" ); */
    return fDetermined;
}

enum Network CNetAddr::GetNetwork() const
{
    if (!IsRoutable())
        return NET_UNROUTABLE;

    if (IsIPv4())
        return NET_IPV4;

    if (IsTor())
        return NET_TOR;

    if (IsI2P()) return NET_I2P;

    return NET_IPV6;
}

std::string CNetAddr::ToStringIP() const
{
    if( IsI2P() )
        return IsNativeI2P() ? ToB32String() : "???.b32.i2p";
    if (IsTor())
        return EncodeBase32(&ip[6], 10) + ".onion";
    CService serv(*this, 0);
    struct sockaddr_storage sockaddr;
    socklen_t socklen = sizeof(sockaddr);
    if (serv.GetSockAddr((struct sockaddr*)&sockaddr, &socklen)) {
        char name[1025] = "";
        if (!getnameinfo((const struct sockaddr*)&sockaddr, socklen, name, sizeof(name), NULL, 0, NI_NUMERICHOST))
            return std::string(name);
    }
    if (IsIPv4())
        return strprintf("%u.%u.%u.%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0));
    else
        return strprintf("%x:%x:%x:%x:%x:%x:%x:%x",
                         GetByte(15) << 8 | GetByte(14), GetByte(13) << 8 | GetByte(12),
                         GetByte(11) << 8 | GetByte(10), GetByte(9) << 8 | GetByte(8),
                         GetByte(7) << 8 | GetByte(6), GetByte(5) << 8 | GetByte(4),
                         GetByte(3) << 8 | GetByte(2), GetByte(1) << 8 | GetByte(0));
}

std::string CNetAddr::ToString() const
{
    return ToStringIP();
}

bool operator==(const CNetAddr& a, const CNetAddr& b)
{
    return (memcmp(a.ip, b.ip, 16) == 0 && memcmp(a.i2pDest, b.i2pDest, I2P_DESTINATION_STORE) == 0);
}

bool operator!=(const CNetAddr& a, const CNetAddr& b)
{
    return (memcmp(a.ip, b.ip, 16) != 0 || memcmp(a.i2pDest, b.i2pDest, I2P_DESTINATION_STORE) != 0);
}

bool operator<(const CNetAddr& a, const CNetAddr& b)
{
    return (memcmp(a.ip, b.ip, 16) < 0 || (memcmp(a.ip, b.ip, 16) == 0 && memcmp(a.i2pDest, b.i2pDest, I2P_DESTINATION_STORE) < 0));
}

bool CNetAddr::GetInAddr(struct in_addr* pipv4Addr) const
{
    if (!IsIPv4())
        return false;
    memcpy(pipv4Addr, ip+12, 4);
    return true;
}

bool CNetAddr::GetIn6Addr(struct in6_addr* pipv6Addr) const
{
    if (IsNativeI2P()) return false;
    memcpy(pipv6Addr, ip, 16);
    return true;
}

// get canonical identifier of an address' group
// no two connections will be attempted to addresses with the same group
std::vector<unsigned char> CNetAddr::GetGroup() const
{
    std::vector<unsigned char> vchRet;
    int nClass = NET_IPV6;
    int nStartByte = 0;
    int nBits = 16;

    if( IsI2P() ) {
        vchRet.resize(I2P_DESTINATION_STORE + 1);
        vchRet[0] = NET_I2P;
        memcpy(&vchRet[1], i2pDest, I2P_DESTINATION_STORE);
        return vchRet;
    }

    // all local addresses belong to the same group
    if (IsLocal())
    {
        nClass = 255;
        nBits = 0;
    }

    // all unroutable addresses belong to the same group
    if (!IsRoutable())
    {
        nClass = NET_UNROUTABLE;
        nBits = 0;
    }
    // for IPv4 addresses, '1' + the 16 higher-order bits of the IP
    // includes mapped IPv4, SIIT translated IPv4, and the well-known prefix
    else if (IsIPv4() || IsRFC6145() || IsRFC6052())
    {
        nClass = NET_IPV4;
        nStartByte = 12;
    }
    // for 6to4 tunnelled addresses, use the encapsulated IPv4 address
    else if (IsRFC3964())
    {
        nClass = NET_IPV4;
        nStartByte = 2;
    }
    // for Teredo-tunnelled IPv6 addresses, use the encapsulated IPv4 address
    else if (IsRFC4380())
    {
        vchRet.push_back(NET_IPV4);
        vchRet.push_back(GetByte(3) ^ 0xFF);
        vchRet.push_back(GetByte(2) ^ 0xFF);
        return vchRet;
    }
    else if (IsTor())
    {
        nClass = NET_TOR;
        nStartByte = 6;
        nBits = 4;
    }
    // for he.net, use /36 groups
    else if (GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0x04 && GetByte(12) == 0x70)
        nBits = 36;
    // for the rest of the IPv6 network, use /32 groups
    else
        nBits = 32;

    vchRet.push_back(nClass);
    while (nBits >= 8)
    {
        vchRet.push_back(GetByte(15 - nStartByte));
        nStartByte++;
        nBits -= 8;
    }
    if (nBits > 0)
        vchRet.push_back(GetByte(15 - nStartByte) | ((1 << nBits) - 1));

    return vchRet;
}

uint64_t CNetAddr::GetHash() const
{
    uint256 hash = IsI2P() ? Hash(i2pDest, i2pDest + I2P_DESTINATION_STORE) : Hash(&ip[0], &ip[16]);
    uint64_t nRet;
    memcpy(&nRet, &hash, sizeof(nRet));
    return nRet;
}

void CNetAddr::print() const
{
    LogPrintf("CNetAddr(%s)\n", ToString());
}

// private extensions to enum Network, only returned by GetExtNetwork,
// and only used in GetReachabilityFrom
static const int NET_UNKNOWN = NET_MAX + 0;
static const int NET_TEREDO  = NET_MAX + 1;
int static GetExtNetwork(const CNetAddr *addr)
{
    if (addr == NULL)
        return NET_UNKNOWN;
    if (addr->IsRFC4380())
        return NET_TEREDO;
    return addr->GetNetwork();
}

/** Calculates a metric for how reachable (*this) is from a given partner */
int CNetAddr::GetReachabilityFrom(const CNetAddr *paddrPartner) const
{
    enum Reachability {
        REACH_UNREACHABLE,
        REACH_DEFAULT,
        REACH_TEREDO,
        REACH_IPV6_WEAK,
        REACH_IPV4,
        REACH_IPV6_STRONG,
        REACH_PRIVATE
    };

    if (!IsRoutable())
        return REACH_UNREACHABLE;

    int ourNet = GetExtNetwork(this);
    int theirNet = GetExtNetwork(paddrPartner);
    bool fTunnel = IsRFC3964() || IsRFC6052() || IsRFC6145();

    switch(theirNet) {
    case NET_IPV4:
        switch(ourNet) {
        default:       return REACH_DEFAULT;
        case NET_IPV4: return REACH_IPV4;
        }
    case NET_IPV6:
        switch(ourNet) {
        default:         return REACH_DEFAULT;
        case NET_TEREDO: return REACH_TEREDO;
        case NET_IPV4:   return REACH_IPV4;
        case NET_IPV6:   return fTunnel ? REACH_IPV6_WEAK : REACH_IPV6_STRONG; // only prefer giving our IPv6 address if it's not tunnelled
        }
    case NET_I2P:
        switch(ourNet) {
        default:             return REACH_UNREACHABLE;
        case NET_I2P: return REACH_PRIVATE;
        }
    case NET_TOR:
        switch(ourNet) {
        default:         return REACH_DEFAULT;
        case NET_IPV4:   return REACH_IPV4; // Tor users can connect to IPv4 as well
        case NET_TOR:    return REACH_PRIVATE;
        }
    case NET_TEREDO:
        switch(ourNet) {
        default:          return REACH_DEFAULT;
        case NET_TEREDO:  return REACH_TEREDO;
        case NET_IPV6:    return REACH_IPV6_WEAK;
        case NET_IPV4:    return REACH_IPV4;
        }
    case NET_UNKNOWN:
    case NET_UNROUTABLE:
    default:
        switch(ourNet) {
        default:          return REACH_DEFAULT;
        case NET_TEREDO:  return REACH_TEREDO;
        case NET_IPV6:    return REACH_IPV6_WEAK;
        case NET_IPV4:    return REACH_IPV4;
        case NET_TOR:     return REACH_PRIVATE; // either from Tor, or don't care about our address
        case NET_I2P: return REACH_PRIVATE;  // Same for i2p
        }
    }
}

void CService::Init()
{
    //! This initialization fact becomes very important to the programmer
    //! for service object comparisons and other details.  We do not use
    //! the port for i2p and so its important that it remain zero,
    port = 0;
}

CService::CService()
{
    Init();
}

CService::CService(const CNetAddr& cip, unsigned short portIn) : CNetAddr(cip), port(portIn)
{
}

CService::CService(const struct in_addr& ipv4Addr, unsigned short portIn) : CNetAddr(ipv4Addr), port(portIn)
{
}

CService::CService(const struct in6_addr& ipv6Addr, unsigned short portIn) : CNetAddr(ipv6Addr), port(portIn)
{
}

CService::CService(const struct sockaddr_in& addr) : CNetAddr(addr.sin_addr), port(ntohs(addr.sin_port))
{
    assert(addr.sin_family == AF_INET);
}

CService::CService(const struct sockaddr_in6 &addr) : CNetAddr(addr.sin6_addr), port(ntohs(addr.sin6_port))
{
   assert(addr.sin6_family == AF_INET6);
}

bool CService::SetSockAddr(const struct sockaddr *paddr)
{
    switch (paddr->sa_family) {
    case AF_INET:
        *this = CService(*(const struct sockaddr_in*)paddr);
        return true;
    case AF_INET6:
        *this = CService(*(const struct sockaddr_in6*)paddr);
        return true;
    default:
        return false;
    }
}

CService::CService(const char *pszIpPort, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(pszIpPort, ip, 0, fAllowLookup))
        *this = ip;
}

CService::CService(const char *pszIpPort, int portDefault, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(pszIpPort, ip, portDefault, fAllowLookup))
        *this = ip;
}

CService::CService(const std::string &strIpPort, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(strIpPort.c_str(), ip, 0, fAllowLookup))
        *this = ip;
}

CService::CService(const std::string &strIpPort, int portDefault, bool fAllowLookup)
{
    Init();
    CService ip;
    if (Lookup(strIpPort.c_str(), ip, portDefault, fAllowLookup))
        *this = ip;
}

unsigned short CService::GetPort() const
{
    return port;
}

bool operator==(const CService& a, const CService& b)
{
    return (CNetAddr)a == (CNetAddr)b && a.port == b.port;
}

bool operator!=(const CService& a, const CService& b)
{
    return (CNetAddr)a != (CNetAddr)b || a.port != b.port;
}

bool operator<(const CService& a, const CService& b)
{
    return (CNetAddr)a < (CNetAddr)b || ((CNetAddr)a == (CNetAddr)b && a.port < b.port);
}

bool CService::GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const
{
    if (IsIPv4()) {
        if (*addrlen < (socklen_t)sizeof(struct sockaddr_in))
            return false;
        *addrlen = sizeof(struct sockaddr_in);
        struct sockaddr_in *paddrin = (struct sockaddr_in*)paddr;
        memset(paddrin, 0, *addrlen);
        if (!GetInAddr(&paddrin->sin_addr))
            return false;
        paddrin->sin_family = AF_INET;
        paddrin->sin_port = htons(port);
        return true;
    }
    if (IsIPv6()) {
        if (*addrlen < (socklen_t)sizeof(struct sockaddr_in6))
            return false;
        *addrlen = sizeof(struct sockaddr_in6);
        struct sockaddr_in6 *paddrin6 = (struct sockaddr_in6*)paddr;
        memset(paddrin6, 0, *addrlen);
        if (!GetIn6Addr(&paddrin6->sin6_addr))
            return false;
        paddrin6->sin6_family = AF_INET6;
        paddrin6->sin6_port = htons(port);
        return true;
    }
    return false;
}

std::vector<unsigned char> CService::GetKey() const
{
     std::vector<unsigned char> vKey;

    if (IsNativeI2P())
    {
        assert( IsI2P() );
        vKey.resize(I2P_DESTINATION_STORE);
        memcpy(&vKey[0], i2pDest, I2P_DESTINATION_STORE);
        return vKey;
    }
     vKey.resize(18);
     memcpy(&vKey[0], ip, 16);
     vKey[16] = port / 0x100;
     vKey[17] = port & 0x0FF;
     return vKey;
}

std::string CService::ToStringPort() const
{
    return strprintf("%u", port);
}

std::string CService::ToStringIPPort() const
{
    if( IsI2P() ) return ToStringIP();                // Drop the port for i2p addresses
    std::string PortStr = ToStringPort();
    return ( IsIPv4() || IsTor() ) ? ToStringIP() + ":" + PortStr : "[" + ToStringIP() + "]:" + PortStr;
}

std::string CService::ToString() const
{
    return ToStringIPPort();
}

void CService::print() const
{
    LogPrint("net","CService(%s)\n", ToString());
}

void CService::SetPort(unsigned short portIn)
{
    port = portIn;
}

CSubNet::CSubNet():
    valid(false)
{
    memset(netmask, 0, sizeof(netmask));
}

CSubNet::CSubNet(const std::string &strSubnet, bool fAllowLookup)
{
    size_t slash = strSubnet.find_last_of('/');
    std::vector<CNetAddr> vIP;

    valid = true;
    // Default to /32 (IPv4) or /128 (IPv6), i.e. match single address
    memset(netmask, 255, sizeof(netmask));

    std::string strAddress = strSubnet.substr(0, slash);
    if (LookupHost(strAddress.c_str(), vIP, 1, fAllowLookup))
    {
        network = vIP[0];
        if (slash != strSubnet.npos)
        {
            std::string strNetmask = strSubnet.substr(slash + 1);
            int32_t n;
            // IPv4 addresses start at offset 12, and first 12 bytes must match, so just offset n
            int noffset = network.IsIPv4() ? (12 * 8) : 0;
            if (ParseInt32(strNetmask, &n)) // If valid number, assume /24 symtex
            {
                if(n >= 0 && n <= (128 - noffset)) // Only valid if in range of bits of address
                {
                    n += noffset;
                    // Clear bits [n..127]
                    for (; n < 128; ++n)
                        netmask[n>>3] &= ~(1<<(n&7));
                }
                else
                {
                    valid = false;
                }
            }
            else // If not a valid number, try full netmask syntax
            {
                if (LookupHost(strNetmask.c_str(), vIP, 1, false)) // Never allow lookup for netmask
                {
                    // Remember: GetByte returns bytes in reversed order
                    // Copy only the *last* four bytes in case of IPv4, the rest of the mask should stay 1's as
                    // we don't want pchIPv4 to be part of the mask.
                    int asize = network.IsIPv4() ? 4 : 16;
                    for(int x=0; x<asize; ++x)
                        netmask[15-x] = vIP[0].GetByte(x);
                }
                else
                {
                    valid = false;
                }
            }
        }
    }
    else
    {
        valid = false;
    }
}

bool CSubNet::Match(const CNetAddr &addr) const
{
    bool fResult = true;        // Assume success

    if (!valid || !addr.IsValid())
        fResult = false;
    else {
        for(int x=0; x<16; ++x)
            if ((addr.GetByte(x) & netmask[15-x]) != network.GetByte(x)) {
                fResult = false;
                break;
            }
    }
    LogPrint("rpcio", "CSubNet::Match() %s with CNetAddr: %s has %s\n", ToString(), addr.ToString(), fResult ? "passed" : "failed" );
    return fResult;
}

std::string CSubNet::ToString() const
{
    std::string strNetmask;
    if (network.IsIPv4())
        strNetmask = strprintf("%u.%u.%u.%u", netmask[12], netmask[13], netmask[14], netmask[15]);
    else
        strNetmask = strprintf("%x:%x:%x:%x:%x:%x:%x:%x",
                         netmask[0] << 8 | netmask[1], netmask[2] << 8 | netmask[3],
                         netmask[4] << 8 | netmask[5], netmask[6] << 8 | netmask[7],
                         netmask[8] << 8 | netmask[9], netmask[10] << 8 | netmask[11],
                         netmask[12] << 8 | netmask[13], netmask[14] << 8 | netmask[15]);
    return network.ToString() + "/" + strNetmask;
}

bool CSubNet::IsValid() const
{
    return valid;
}

bool operator==(const CSubNet& a, const CSubNet& b)
{
    return a.valid == b.valid && a.network == b.network && !memcmp(a.netmask, b.netmask, 16);
}

bool operator!=(const CSubNet& a, const CSubNet& b)
{
    return !(a==b);
}

#ifdef WIN32
std::string NetworkErrorString(int err)
{
    char buf[256];
    buf[0] = 0;
    if(FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buf, sizeof(buf), NULL))
    {
        return strprintf("%s (%d)", buf, err);
    }
    else
    {
        return strprintf("Unknown error (%d)", err);
    }
}
#else
std::string NetworkErrorString(int err)
{
    char buf[256];
    const char *s = buf;
    buf[0] = 0;
    /* Too bad there are two incompatible implementations of the
     * thread-safe strerror. */
#ifdef STRERROR_R_CHAR_P /* GNU variant can return a pointer outside the passed buffer */
    s = strerror_r(err, buf, sizeof(buf));
#else /* POSIX variant always returns message in buffer */
    if (strerror_r(err, buf, sizeof(buf)))
        buf[0] = 0;
#endif
    return strprintf("%s (%d)", s, err);
}
#endif

bool CloseSocket(SOCKET& hSocket)
{
    if (hSocket == INVALID_SOCKET)
        return false;
#ifdef WIN32
    int ret = closesocket(hSocket);
#else
    int ret = close(hSocket);
#endif
    hSocket = INVALID_SOCKET;
    return ret != SOCKET_ERROR;
}

bool SetSocketNonBlocking(SOCKET& hSocket, bool fNonBlocking)
{
    if (fNonBlocking) {
#ifdef WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR) {
#else
        int fFlags = fcntl(hSocket, F_GETFL, 0);
        if (fcntl(hSocket, F_SETFL, fFlags | O_NONBLOCK) == SOCKET_ERROR) {
#endif
            CloseSocket(hSocket);
            return false;
        }
    } else {
#ifdef WIN32
        u_long nZero = 0;
        if (ioctlsocket(hSocket, FIONBIO, &nZero) == SOCKET_ERROR) {
#else
        int fFlags = fcntl(hSocket, F_GETFL, 0);
        if (fcntl(hSocket, F_SETFL, fFlags & ~O_NONBLOCK) == SOCKET_ERROR) {
#endif
            CloseSocket(hSocket);
            return false;
        }
    }

    return true;
}

/**
 * Specific functions we need to implement I2P functionality
 */

//! GR note...Figuring out where these best fit into the scheme of I2P implementation has been a difficult problem.
//!
//! the ones below work with or without building i2psam enabled and are used here to implement i2p address space
//! regardless of an i2p router interface being defined or included in your build.
//! NATIVE_I2P_NET_STRING and I2P_DESTINATION_STORE are both defined as the same size 516 bytes.  However, the first
//! is defined and used in the hardware specific code.  The later is defined for this module and determines the exact
//! storage allocated in the CNetAddr object so we can support I2P destination address space.
//!
//! We plan on changing the details inside that object for addresses, In the future we'll store them in binary, not
//! base64, and have variable length certificates at the end of the keys, conforming to the new I2P definition, yet
//! not changing the size of of the object here in each CNetAddr.
bool IsTorOnly()
{
    bool torOnly = false;
    const std::vector<std::string>& onlyNets = mapMultiArgs["-onlynet"];
    torOnly = (onlyNets.size() == 1) && ( (onlyNets[0] == "tor") || (onlyNets[0] == "onion") );
    return torOnly;
}

bool IsI2POnly()
{
    bool i2pOnly = false;
    if (mapArgs.count("-onlynet")) {
        const std::vector<std::string>& onlyNets = mapMultiArgs["-onlynet"];
        i2pOnly = (onlyNets.size() == 1 && onlyNets[0] == "i2p") || (onlyNets.size() == 2 && onlyNets[0] == "i2p" && onlyNets[1] == "i2p");
    }
    return i2pOnly;
}

// If either/or dark net or if we're running a proxy or onion and in either of those cases if i2p is also enabled
bool IsDarknetOnly()
{
    return IsI2POnly() || IsTorOnly() || ( IsI2PEnabled() && ((mapArgs.count("-proxy") && mapArgs["-proxy"] != "0") || (mapArgs.count("-onion") && mapArgs["-onion"] != "0")) );
}

// We now just check the -i2p.options.enabled parameter here, and return its value true or false
bool IsI2PEnabled()
{
    return GetBoolArg("-i2p.options.enabled", false);
}

bool IsBehindDarknet()
{
    return IsDarknetOnly() || (mapArgs.count("-onion") && mapArgs["-onion"] != "0");
}

bool IsMyDestinationShared()
{
    return GetBoolArg("-i2p.mydestination.shareaddr", true); //CSlave: sharing enabled by default unless specified shareaddr=0  
}

// This test should pass for both public and private keys, as the first part of the private key is the public key.
bool isValidI2pAddress( const std::string& I2pAddr )
{
    if( I2pAddr.size() < I2P_DESTINATION_STORE ) return false;
    return (I2pAddr.substr( I2P_DESTINATION_STORE - 4, 4 ) == "AAAA");
}

bool isValidI2pB32( const std::string& B32Address )
{
    return (B32Address.size() == NATIVE_I2P_B32ADDR_SIZE) && (B32Address.substr(B32Address.size() - 8, 8) == ".b32.i2p");
}

bool isStringI2pDestination( const std::string & strName )
{
    return isValidI2pB32( strName ) || isValidI2pAddress( strName );
}

uint256 GetI2pDestinationHash( const std::string& destination )
{
    std::string canonicalDest = destination;                    // Copy the string locally, so we can modify it & its not a const

    for (size_t pos = canonicalDest.find_first_of('-'); pos != std::string::npos; pos = canonicalDest.find_first_of('-', pos))
        canonicalDest[pos] = '+';
    for (size_t pos = canonicalDest.find_first_of('~'); pos != std::string::npos; pos = canonicalDest.find_first_of('~', pos))
        canonicalDest[pos] = '/';
    std::string rawDest = DecodeBase64(canonicalDest);
    uint256 b32hash;
    SHA256((const unsigned char*)rawDest.c_str(), rawDest.size(), (unsigned char*)&b32hash);
    return b32hash;
}

std::string B32AddressFromDestination(const std::string& destination)
{
    uint256 b32hash = GetI2pDestinationHash( destination );
    std::string result = EncodeBase32(b32hash.begin(), b32hash.end() - b32hash.begin()) + ".b32.i2p";
    for (size_t pos = result.find_first_of('='); pos != std::string::npos; pos = result.find_first_of('=', pos-1))
        result.erase(pos, 1);
    return result;
}
