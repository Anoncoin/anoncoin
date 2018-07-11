// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_NETBASE_H
#define ANONCOIN_NETBASE_H

#if defined(HAVE_CONFIG_H)
#include "config/anoncoin-config.h"
#endif

#include "compat.h"
#include "serialize.h"

#include <stdint.h>
#include <string>
#include <vector>

extern int nConnectTimeout;
extern bool fNameLookup;

/** -timeout default */
extern const int32_t DEFAULT_CONNECT_TIMEOUT;

#ifdef WIN32
// In MSVC, this is defined as a macro, undefine it to prevent a compile and link error
#undef SetPort
#endif

#ifdef ENABLE_I2PSAM
#include "i2pwrapper.h"
#endif

#define I2P_DESTINATION_STORE 516
#define NATIVE_I2P_B32ADDR_SIZE 60

enum Network
{
    NET_UNROUTABLE = 0,
    NET_IPV4,
    NET_IPV6,
    NET_TOR,
    NET_I2P,
    NET_MAX,
};

class uint256;
class CAddrMan;

extern int nConnectTimeout;
extern bool fNameLookup;
extern CAddrMan addrman;

/** IP address (IPv6, or IPv4 using mapped IPv6 range (::FFFF:0:0/96)) */
class CNetAddr
{
    protected:
        unsigned char ip[16]; // in network byte order
        unsigned char i2pDest[I2P_DESTINATION_STORE]; // I2P Destination
    public:
        CNetAddr();
        CNetAddr(const struct in_addr& ipv4Addr);
        explicit CNetAddr(const char *pszIp, bool fAllowLookup = false);
        explicit CNetAddr(const std::string &strIp, bool fAllowLookup = false);
        void Init();
        void SetIP(const CNetAddr& ip);
        /**
         * Set raw IPv4 or IPv6 address (in network byte order)
         * @note Only NET_IPV4 and NET_IPV6 are allowed for network.
         */
        void SetRaw(Network network, const uint8_t *data);

        bool SetSpecial(const std::string &strName); // for Tor & I2P addresses
        bool IsIPv4() const;    // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)
        bool IsIPv6() const;    // IPv6 address (not mapped IPv4, not Tor)
        bool IsRFC1918() const; // IPv4 private networks (10.0.0.0/8, 192.168.0.0/16, 172.16.0.0/12)
        bool IsRFC2544() const; // IPv4 inter-network communcations (192.18.0.0/15)
        bool IsRFC6598() const; // IPv4 ISP-level NAT (100.64.0.0/10)
        bool IsRFC5737() const; // IPv4 documentation addresses (192.0.2.0/24, 198.51.100.0/24, 203.0.113.0/24)
        bool IsRFC3849() const; // IPv6 documentation address (2001:0DB8::/32)
        bool IsRFC3927() const; // IPv4 autoconfig (169.254.0.0/16)
        bool IsRFC3964() const; // IPv6 6to4 tunnelling (2002::/16)
        bool IsRFC4193() const; // IPv6 unique local (FC00::/7)
        bool IsRFC4380() const; // IPv6 Teredo tunnelling (2001::/32)
        bool IsRFC4843() const; // IPv6 ORCHID (2001:10::/28)
        bool IsRFC4862() const; // IPv6 autoconfig (FE80::/64)
        bool IsRFC6052() const; // IPv6 well-known prefix (64:FF9B::/96)
        bool IsRFC6145() const; // IPv6 IPv4-translated address (::FFFF:0:0:0/96)
        bool IsTor() const;
        bool IsLocal() const;
        bool IsRoutable() const;
        bool IsValid() const;
        bool IsMulticast() const;
        enum Network GetNetwork() const;
        std::string ToString() const;
        std::string ToStringIP() const;
        unsigned int GetByte(int n) const;
        uint64_t GetHash() const;
        bool GetInAddr(struct in_addr* pipv4Addr) const;
        std::vector<unsigned char> GetGroup() const;
        int GetReachabilityFrom(const CNetAddr *paddrPartner = NULL) const;

        CNetAddr(const struct in6_addr& pipv6Addr);
        bool GetIn6Addr(struct in6_addr* pipv6Addr) const;
        void print() const;
        /** I2P Specific methods */
        bool IsI2P() const;
        bool IsNativeI2P() const;
        bool CheckAndSetGarlicCat( void );
        std::string GetI2pDestination() const;
        bool SetI2pDestination( const std::string& sBase64Dest );
        std::string ToB32String() const;

        friend bool operator==(const CNetAddr& a, const CNetAddr& b);
        friend bool operator!=(const CNetAddr& a, const CNetAddr& b);
        friend bool operator<(const CNetAddr& a, const CNetAddr& b);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
            READWRITE(FLATDATA(ip));
             if (!(nType & SER_IPADDRONLY)) {
                READWRITE(FLATDATA(i2pDest));
             }
        }
};

class CSubNet
{
    protected:
        /// Network (base) address
        CNetAddr network;
        /// Netmask, in network byte order
        uint8_t netmask[16];
        /// Is this value valid? (only used to signal parse errors)
        bool valid;

    public:
        CSubNet();
        explicit CSubNet(const std::string &strSubnet, bool fAllowLookup = false);

        bool Match(const CNetAddr &addr) const;

        std::string ToString() const;
        bool IsValid() const;

        friend bool operator==(const CSubNet& a, const CSubNet& b);
        friend bool operator!=(const CSubNet& a, const CSubNet& b);
};

/** A combination of a network address (CNetAddr) and a (TCP) port */
class CService : public CNetAddr
{
    protected:
        unsigned short port; // host order

    public:
        CService();
        CService(const CNetAddr& ip, unsigned short port);
        CService(const struct in_addr& ipv4Addr, unsigned short port);
        CService(const struct sockaddr_in& addr);
        explicit CService(const char *pszIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const char *pszIpPort, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, bool fAllowLookup = false);
        void Init();
        void SetPort(unsigned short portIn);
        unsigned short GetPort() const;
        bool GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const;
        bool SetSockAddr(const struct sockaddr* paddr);
        friend bool operator==(const CService& a, const CService& b);
        friend bool operator!=(const CService& a, const CService& b);
        friend bool operator<(const CService& a, const CService& b);
        std::vector<unsigned char> GetKey() const;
        std::string ToString() const;
        std::string ToStringPort() const;
        std::string ToStringIPPort() const;
        void print() const;

        CService(const struct in6_addr& ipv6Addr, unsigned short port);
        CService(const struct sockaddr_in6& addr);

        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
            READWRITE(FLATDATA(ip));
             if (!(nType & SER_IPADDRONLY)) {
                READWRITE(FLATDATA(i2pDest));
             }
            unsigned short portN = htons(port);
            READWRITE(portN);
            if (ser_action.ForRead())
                 port = ntohs(portN);
        }
};

typedef CService proxyType;

enum Network ParseNetwork(std::string net);
std::string GetNetworkName(enum Network net);
void SplitHostPort(std::string in, int &portOut, std::string &hostOut);
bool SetProxy(enum Network net, CService addrProxy);
bool GetProxy(enum Network net, proxyType &proxyInfoOut);
bool IsProxy(const CNetAddr &addr);
bool SetNameProxy(CService addrProxy);
bool HaveNameProxy();
bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions = 0, bool fAllowLookup = true);
bool Lookup(const char *pszName, CService& addr, int portDefault = 0, bool fAllowLookup = true);
bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault = 0, bool fAllowLookup = true, unsigned int nMaxSolutions = 0);
bool LookupNumeric(const char *pszName, CService& addr, int portDefault = 0);
bool ConnectSocket(const CService &addr, SOCKET& hSocketRet, int nTimeout, bool *outProxyConnectionFailed = 0);
bool ConnectSocketByName(CService &addr, SOCKET& hSocketRet, const char *pszDest, int portDefault, int nTimeout, bool *outProxyConnectionFailed = 0);

/** Return readable error string for a network error code */
std::string NetworkErrorString(int err);
/** Close socket and set hSocket to INVALID_SOCKET */
bool CloseSocket(SOCKET& hSocket);
/** Disable or enable blocking-mode for a socket */
bool SetSocketNonBlocking(SOCKET& hSocket, bool fNonBlocking);

/** I2P and darknet specific routines */
bool IsDarknetOnly();
bool IsTorOnly();
bool IsI2POnly();
bool IsI2PEnabled();
bool IsBehindDarknet();
bool IsMyDestinationShared();
bool isValidI2pAddress( const std::string& I2pAddr );
bool isValidI2pB32( const std::string& B32Address );
bool isStringI2pDestination( const std::string & strName );
std::string B32AddressFromDestination(const std::string& destination);
uint256 GetI2pDestinationHash( const std::string& destination );

#endif // ANONCOIN_NETBASE_H
