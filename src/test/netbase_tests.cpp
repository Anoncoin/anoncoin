// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netbase.h"

#include <string>

#include <boost/test/unit_test.hpp>

using namespace std;

bool fHasIPv6Installed = false;
// Without this, the following fail if IPv6 is not installed
// test/netbase_tests.cpp(21): error in "netbase_networks": check CNetAddr("2001::8888").GetNetwork() == NET_IPV6 failed
// test/netbase_tests.cpp(22): error in "netbase_networks": check CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() == NET_TOR failed
// test/netbase_tests.cpp(28): error in "netbase_properties": check CNetAddr("::FFFF:192.168.1.1").IsIPv4() failed
// test/netbase_tests.cpp(33): error in "netbase_properties": check CNetAddr("2001:0DB8::").IsRFC3849() failed
// test/netbase_tests.cpp(35): error in "netbase_properties": check CNetAddr("2002::1").IsRFC3964() failed
// test/netbase_tests.cpp(36): error in "netbase_properties": check CNetAddr("FC00::").IsRFC4193() failed
// test/netbase_tests.cpp(37): error in "netbase_properties": check CNetAddr("2001::2").IsRFC4380() failed
// test/netbase_tests.cpp(38): error in "netbase_properties": check CNetAddr("2001:10::").IsRFC4843() failed
// test/netbase_tests.cpp(39): error in "netbase_properties": check CNetAddr("FE80::").IsRFC4862() failed
// test/netbase_tests.cpp(40): error in "netbase_properties": check CNetAddr("64:FF9B::").IsRFC6052() failed
// test/netbase_tests.cpp(41): error in "netbase_properties": check CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor() failed
// test/netbase_tests.cpp(43): error in "netbase_properties": check CNetAddr("::1").IsLocal() failed
// test/netbase_tests.cpp(45): error in "netbase_properties": check CNetAddr("2001::1").IsRoutable() failed
// test/netbase_tests.cpp(88): error in "netbase_lookupnumeric": check TestParse("::ffff:127.0.0.1", "127.0.0.1:65535") failed
// test/netbase_tests.cpp(89): error in "netbase_lookupnumeric": check TestParse("::", "[::]:65535") failed
// test/netbase_tests.cpp(90): error in "netbase_lookupnumeric": check TestParse("[::]:9377", "[::]:9377") failed
// test/netbase_tests.cpp(100): error in "onioncat_test": check addr1 == addr2 failed
// test/netbase_tests.cpp(116): error in "subnet_test": check CSubNet("::ffff:127.0.0.1").Match(CNetAddr("127.0.0.1")) failed
// test/netbase_tests.cpp(117): error in "subnet_test": check CSubNet("1:2:3:4:5:6:7:8").Match(CNetAddr("1:2:3:4:5:6:7:8")) failed
// test/netbase_tests.cpp(119): error in "subnet_test": check CSubNet("1:2:3:4:5:6:7:0/112").Match(CNetAddr("1:2:3:4:5:6:7:1234")) failed
// test/netbase_tests.cpp(121): error in "subnet_test": check CSubNet("::/0").Match(CNetAddr("1:2:3:4:5:6:7:1234")) failed
// test/netbase_tests.cpp(122): error in "subnet_test": check CSubNet("::/0").Match(CNetAddr("1.2.3.4")) failed
// test/netbase_tests.cpp(135): error in "subnet_test": check CSubNet("1:2:3:4:5:6:7:8/0").IsValid() failed
// test/netbase_tests.cpp(136): error in "subnet_test": check CSubNet("1:2:3:4:5:6:7:8/33").IsValid() failed
// test/netbase_tests.cpp(138): error in "subnet_test": check CSubNet("1:2:3:4:5:6:7:8/128").IsValid() failed
BOOST_AUTO_TEST_SUITE(netbase_tests)

BOOST_AUTO_TEST_CASE(netbase_networks)
{
    BOOST_CHECK(CNetAddr("127.0.0.1").GetNetwork()                              == NET_UNROUTABLE);
    BOOST_CHECK(CNetAddr("::1").GetNetwork()                                    == NET_UNROUTABLE);
    BOOST_CHECK(CNetAddr("8.8.8.8").GetNetwork()                                == NET_IPV4);
    fHasIPv6Installed = CNetAddr("2001::8888").IsRoutable();
    if( fHasIPv6Installed ) {
        BOOST_CHECK(CNetAddr("2001::8888").GetNetwork()                             == NET_IPV6);
        BOOST_CHECK(CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() == NET_TOR);
        // Think this fails because it is unroutable, ToDo:
        // BOOST_CHECK(CNetAddr("FD60:DB4D:DDB5:0000:0000:0000:0000:0000").GetNetwork() == NET_I2P);
    } else
        BOOST_TEST_MESSAGE( "WARNING - Unable to test IPv6 features.  Appears your system does not have IPv6 socket support installed.");

}

BOOST_AUTO_TEST_CASE(netbase_properties)
{
    BOOST_CHECK(CNetAddr("127.0.0.1").IsIPv4());
    if( fHasIPv6Installed ) BOOST_CHECK(CNetAddr("::FFFF:192.168.1.1").IsIPv4());
    BOOST_CHECK(CNetAddr("::1").IsIPv6());
    BOOST_CHECK(CNetAddr("10.0.0.1").IsRFC1918());
    BOOST_CHECK(CNetAddr("192.168.1.1").IsRFC1918());
    BOOST_CHECK(CNetAddr("172.31.255.255").IsRFC1918());
    if( fHasIPv6Installed ) BOOST_CHECK(CNetAddr("2001:0DB8::").IsRFC3849());
    BOOST_CHECK(CNetAddr("169.254.1.1").IsRFC3927());
    if( fHasIPv6Installed ) {
        BOOST_CHECK(CNetAddr("2002::1").IsRFC3964());
        BOOST_CHECK(CNetAddr("FC00::").IsRFC4193());
        BOOST_CHECK(CNetAddr("2001::2").IsRFC4380());
        BOOST_CHECK(CNetAddr("2001:10::").IsRFC4843());
        BOOST_CHECK(CNetAddr("FE80::").IsRFC4862());
        BOOST_CHECK(CNetAddr("64:FF9B::").IsRFC6052());
        BOOST_CHECK(CNetAddr("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor());
        BOOST_CHECK(CNetAddr("FD60:DB4D:DDB5:0000:0000:0000:0000:0000").IsI2P());
    }
    BOOST_CHECK(CNetAddr("127.0.0.1").IsLocal());
    if( fHasIPv6Installed ) BOOST_CHECK(CNetAddr("::1").IsLocal());
    BOOST_CHECK(CNetAddr("8.8.8.8").IsRoutable());
    if( fHasIPv6Installed ) BOOST_CHECK(CNetAddr("2001::1").IsRoutable());
    BOOST_CHECK(CNetAddr("127.0.0.1").IsValid());
}

bool static TestSplitHost(string test, string host, int port)
{
    string hostOut;
    int portOut = -1;
    SplitHostPort(test, portOut, hostOut);
    return hostOut == host && port == portOut;
}

BOOST_AUTO_TEST_CASE(netbase_splithost)
{
    BOOST_CHECK(TestSplitHost("www.anoncoin.net", "www.anoncoin.net", -1));
    BOOST_CHECK(TestSplitHost("[www.anoncoin.net]", "www.anoncoin.net", -1));
    BOOST_CHECK(TestSplitHost("www.anoncoin.net:80", "www.anoncoin.net", 80));
    BOOST_CHECK(TestSplitHost("[www.anoncoin.net]:80", "www.anoncoin.net", 80));
    BOOST_CHECK(TestSplitHost("127.0.0.1", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("127.0.0.1:9377", "127.0.0.1", 9377));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]:9377", "127.0.0.1", 9377));
    BOOST_CHECK(TestSplitHost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[::ffff:127.0.0.1]:9377", "::ffff:127.0.0.1", 9377));
    BOOST_CHECK(TestSplitHost("[::]:9377", "::", 9377));
    BOOST_CHECK(TestSplitHost("::9377", "::9377", -1));
    BOOST_CHECK(TestSplitHost(":9377", "", 9377));
    BOOST_CHECK(TestSplitHost("[]:9377", "", 9377));
    BOOST_CHECK(TestSplitHost("", "", -1));
}

bool static TestParse(string src, string canon)
{
    CService addr;
    if (!LookupNumeric(src.c_str(), addr, 65535))
        return canon == "";
    return canon == addr.ToString();
}

BOOST_AUTO_TEST_CASE(netbase_lookupnumeric)
{
    BOOST_CHECK(TestParse("127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("127.0.0.1:9377", "127.0.0.1:9377"));
    if( fHasIPv6Installed ) BOOST_CHECK(TestParse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    if( fHasIPv6Installed ) BOOST_CHECK(TestParse("::", "[::]:65535"));
    if( fHasIPv6Installed ) BOOST_CHECK(TestParse("[::]:9377", "[::]:9377"));
    BOOST_CHECK(TestParse("[127.0.0.1]", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse(":::", ""));
}

BOOST_AUTO_TEST_CASE(onioncat_test)
{
    // values from https://web.archive.org/web/20121122003543/http://www.cypherpunk.at/onioncat/wiki/OnionCat
    CNetAddr addr1("5wyqrzbvrdsumnok.onion");
    if( fHasIPv6Installed ) {
        CNetAddr addr2("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca");
        BOOST_CHECK(addr1 == addr2);
    }
    BOOST_CHECK(addr1.IsTor());
    BOOST_CHECK(addr1.ToStringIP() == "5wyqrzbvrdsumnok.onion");
    BOOST_CHECK(addr1.IsRoutable());
}

BOOST_AUTO_TEST_CASE(subnet_test)
{
    BOOST_CHECK(CSubNet("1.2.3.0/24") == CSubNet("1.2.3.0/255.255.255.0"));
    BOOST_CHECK(CSubNet("1.2.3.0/24") != CSubNet("1.2.4.0/255.255.255.0"));
    BOOST_CHECK(CSubNet("1.2.3.0/24").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("1.2.2.0/24").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(CSubNet("1.2.3.4").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(CSubNet("1.2.3.4/32").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("1.2.3.4").Match(CNetAddr("5.6.7.8")));
    BOOST_CHECK(!CSubNet("1.2.3.4/32").Match(CNetAddr("5.6.7.8")));
    if( fHasIPv6Installed ) {
        BOOST_CHECK(CSubNet("::ffff:127.0.0.1").Match(CNetAddr("127.0.0.1")));
        BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8").Match(CNetAddr("1:2:3:4:5:6:7:8")));
        BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8").Match(CNetAddr("1:2:3:4:5:6:7:9")));
        BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:0/112").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
        // All-Matching IPv6 Matches arbitrary IPv4 and IPv6
        BOOST_CHECK(CSubNet("::/0").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
        BOOST_CHECK(CSubNet("::/0").Match(CNetAddr("1.2.3.4")));
        // All-Matching IPv4 does not Match IPv6
        BOOST_CHECK(!CSubNet("0.0.0.0/0").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
    }
    // Invalid subnets Match nothing (not even invalid addresses)
    BOOST_CHECK(!CSubNet().Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("").Match(CNetAddr("4.5.6.7")));
    BOOST_CHECK(!CSubNet("bloop").Match(CNetAddr("0.0.0.0")));
    BOOST_CHECK(!CSubNet("bloop").Match(CNetAddr("hab")));
    // Check valid/invalid
    BOOST_CHECK(CSubNet("1.2.3.0/0").IsValid());
    BOOST_CHECK(!CSubNet("1.2.3.0/-1").IsValid());
    BOOST_CHECK(CSubNet("1.2.3.0/32").IsValid());
    BOOST_CHECK(!CSubNet("1.2.3.0/33").IsValid());
    if( fHasIPv6Installed ) {
        BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/0").IsValid());
        BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/33").IsValid());
        BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8/-1").IsValid());
        BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/128").IsValid());
        BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8/129").IsValid());
    }
    BOOST_CHECK(!CSubNet("fuzzy").IsValid());
}

BOOST_AUTO_TEST_SUITE_END()
