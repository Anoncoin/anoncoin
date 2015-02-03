// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "base58.h"
#include "script.h"
#include "uint256.h"
#include "util.h"

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

static const string strSecret1     ("65H6Yv7y8RKJjGQrEG9Nf5aAakSBcuoY5G6bYP86w4tmGrJMFYp");
static const string strSecret2     ("66PrZwCLgSCJCU2EwV28CQQYfADQ1o3SBzwWhYsFBSdp76yPrYV");
static const string strSecret1C    ("PPt3VsTkjkKoFZ2GHXqxb5BPHxi9sW4Q9odhTWeMBeBUQeKn4H9v");
static const string strSecret2C    ("PUosnND3s4CrucGUXUfxmLJvjRfiaWDkBYjc5yemb69Uuqfd2mz7");
static const CAnoncoinAddress addr1 ("AKALrwZGDhGAP2iCjqmp4tggFs9hZtPSNr");
static const CAnoncoinAddress addr2 ("ALmT6W4YG91Nam7huSERxgXMVWHeTruggz");
static const CAnoncoinAddress addr1C("AVinYSTA9Hc7p7bKBjrNSSoqkzohuNS2X2");
static const CAnoncoinAddress addr2C("AGR1AYjJRq21JUpWLCFmXxc6MCsCwAC61g");


static const string strAddressBad("1HV9Lc3sNHZxwj4Zk6fB38tEmBryq2cBiF");

// Define this to generate test vectors for the above compressed and uncompressed key values
// #define KEY_TESTS_DUMPINFO

#ifdef KEY_TESTS_DUMPINFO
void dumpKeyInfo(uint256 secret)
{
    CKey key;
    CAnoncoinSecret b58secret;
    printf("  * secret (hex): %s\n", secret.GetHex().c_str());
    for (int nCompressed=0; nCompressed<2; nCompressed++)
    {
        bool fCompressed = nCompressed == 1;
        printf("  * %s:\n", fCompressed ? "compressed" : "uncompressed");
        key.Set( secret.begin(), secret.end(), fCompressed );
        b58secret.SetKey( key );
        printf("    * secret (base58): %s\n", b58secret.ToString().c_str());
        CPubKey pubkey = key.GetPubKey();
        printf("    * address (base58): %s\n", CAnoncoinAddress(CTxDestination(pubkey.GetID())).ToString().c_str() );
    }
}
#endif

// Visitor to check address type
class TestAddrTypeVisitor : public boost::static_visitor<bool>
{
private:
    std::string exp_addrType;
public:
    TestAddrTypeVisitor(const std::string &exp_addrType) : exp_addrType(exp_addrType) { }
    bool operator()(const CKeyID &id) const
    {
        return (exp_addrType == "pubkey");
    }
    bool operator()(const CScriptID &id) const
    {
        return (exp_addrType == "script");
    }
    bool operator()(const CNoDestination &no) const
    {
        return (exp_addrType == "none");
    }
};

BOOST_AUTO_TEST_SUITE(key_tests)

BOOST_AUTO_TEST_CASE(key_test1)
{
    // These 32 byte ECDSA secp256k1 secrets will be used for Anoncoin KeyTest vectors
#ifdef KEY_TESTS_DUMPINFO
    dumpKeyInfo( uint256( "0x031da951fb26244f9230ae66b093d72b4c5c9ee148336bc9c465663a1a033363" ) );
    dumpKeyInfo( uint256( "0x75cf1d5be870a08d9769177f72545c47533060442ed35d48a52a0fb0f1083ef6" ) );
#endif
    CAnoncoinSecret bsecret1, bsecret2, bsecret1C, bsecret2C, baddress1;
    BOOST_CHECK( bsecret1.SetString (strSecret1));
    BOOST_CHECK( bsecret2.SetString (strSecret2));
    BOOST_CHECK( bsecret1C.SetString(strSecret1C));
    BOOST_CHECK( bsecret2C.SetString(strSecret2C));
    BOOST_CHECK(!baddress1.SetString(strAddressBad));

    CKey key1  = bsecret1.GetKey();
    BOOST_CHECK(key1.IsCompressed() == false);
    CKey key2  = bsecret2.GetKey();
    BOOST_CHECK(key2.IsCompressed() == false);
    CKey key1C = bsecret1C.GetKey();
    BOOST_CHECK(key1C.IsCompressed() == true);
    CKey key2C = bsecret2C.GetKey();
    BOOST_CHECK(key1C.IsCompressed() == true);

    CPubKey pubkey1  = key1. GetPubKey();
    CPubKey pubkey2  = key2. GetPubKey();
    CPubKey pubkey1C = key1C.GetPubKey();
    CPubKey pubkey2C = key2C.GetPubKey();

    // Experimental code for use in another test:
    // CScript scriptPubKey;
    // scriptPubKey.SetDestination( pubkey1C.GetID() );
    // printf( "PublicKey Script of 1C:%s\n", scriptPubKey.ToString().c_str() );

    BOOST_CHECK( addr1.IsValid() );
    BOOST_CHECK( addr2.IsValid() );
    BOOST_CHECK( addr1C.IsValid() );
    BOOST_CHECK( addr2C.IsValid() );

    std::string exp_addrType( "pubkey" );       // "pubkey" is expected for all the addr variables, if not, then incorrect for this coin
    CTxDestination dest;
    dest = addr1.Get();
    BOOST_CHECK_MESSAGE(boost::apply_visitor(TestAddrTypeVisitor(exp_addrType), dest), "addr1 is not a Public Key");
    dest = addr2.Get();
    BOOST_CHECK_MESSAGE(boost::apply_visitor(TestAddrTypeVisitor(exp_addrType), dest), "addr2 is not a Public Key");
    dest = addr1C.Get();
    BOOST_CHECK_MESSAGE(boost::apply_visitor(TestAddrTypeVisitor(exp_addrType), dest), "addr1C is not a Public Key");
    dest = addr2C.Get();
    BOOST_CHECK_MESSAGE(boost::apply_visitor(TestAddrTypeVisitor(exp_addrType), dest), "addr2C is not a Public Key");

#ifdef KEY_TESTS_DUMPINFO
    // Diagnostic output, that can be helpful if the test data will not pass
    printf(":%s:\n", CAnoncoinAddress(CTxDestination(pubkey1.GetID())).ToString().c_str() );
    printf(":%s:\n", CAnoncoinAddress(CTxDestination(pubkey2.GetID())).ToString().c_str() );
    printf(":%s:\n", CAnoncoinAddress(CTxDestination(pubkey1C.GetID())).ToString().c_str() );
    printf(":%s:\n", CAnoncoinAddress(CTxDestination(pubkey2C.GetID())).ToString().c_str() );
#endif

    BOOST_CHECK(addr1.Get()  == CTxDestination(pubkey1.GetID()));
    BOOST_CHECK(addr2.Get()  == CTxDestination(pubkey2.GetID()));
    BOOST_CHECK(addr1C.Get() == CTxDestination(pubkey1C.GetID()));
    BOOST_CHECK(addr2C.Get() == CTxDestination(pubkey2C.GetID()));

    for (int n=0; n<16; n++)
    {
        string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());

        // normal signatures

        vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.Sign (hashMsg, sign1));
        BOOST_CHECK(key2.Sign (hashMsg, sign2));
        BOOST_CHECK(key1C.Sign(hashMsg, sign1C));
        BOOST_CHECK(key2C.Sign(hashMsg, sign2C));

        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2.Verify(hashMsg, sign2C));

        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2));
        BOOST_CHECK( pubkey1C.Verify(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1C.Verify(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2));
        BOOST_CHECK(!pubkey2C.Verify(hashMsg, sign1C));
        BOOST_CHECK( pubkey2C.Verify(hashMsg, sign2C));

        // compact signatures (with key recovery)

        vector<unsigned char> csign1, csign2, csign1C, csign2C;

        BOOST_CHECK(key1.SignCompact (hashMsg, csign1));
        BOOST_CHECK(key2.SignCompact (hashMsg, csign2));
        BOOST_CHECK(key1C.SignCompact(hashMsg, csign1C));
        BOOST_CHECK(key2C.SignCompact(hashMsg, csign2C));

        CPubKey rkey1, rkey2, rkey1C, rkey2C;

        BOOST_CHECK(rkey1.RecoverCompact (hashMsg, csign1));
        BOOST_CHECK(rkey2.RecoverCompact (hashMsg, csign2));
        BOOST_CHECK(rkey1C.RecoverCompact(hashMsg, csign1C));
        BOOST_CHECK(rkey2C.RecoverCompact(hashMsg, csign2C));

        BOOST_CHECK(rkey1  == pubkey1);
        BOOST_CHECK(rkey2  == pubkey2);
        BOOST_CHECK(rkey1C == pubkey1C);
        BOOST_CHECK(rkey2C == pubkey2C);
    }
}

BOOST_AUTO_TEST_SUITE_END()
