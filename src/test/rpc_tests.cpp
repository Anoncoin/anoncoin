// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace json_spirit;

Array
createArgs(int nRequired, const char* address1=NULL, const char* address2=NULL)
{
    Array result;
    result.push_back(nRequired);
    Array addresses;
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    result.push_back(addresses);
    return result;
}

Value CallRPC(string args)
{
    vector<string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    Array params = RPCConvertValues(strMethod, vArgs);

    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        Value result = (*method)(params, false);
        return result;
    }
    catch (Object& objError)
    {
        throw runtime_error(find_value(objError, "message").get_str());
    }
}


BOOST_AUTO_TEST_SUITE(rpc_tests)

BOOST_AUTO_TEST_CASE(rpc_rawparams)
{
    // Test raw transaction API argument handling
    Value r;

    BOOST_CHECK_THROW(CallRPC("getrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction not_hex"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed not_int"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("createrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction null null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction not_array"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] []"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction {} {}"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] {}"));
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {} extra"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("decoderawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction DEADBEEF"), runtime_error);
    string rawtx = "0100000001a15d57094aa7a21a28cb20b59aab8fc7d1149a3bdbcddba9c622e4f5f6a99ece010000006c493046022100f93bb0e7d8db7bd46e40132d1f8242026e045f03a0efe71bbb8e3f475e970d790221009337cd7f1f929f00cc6ff01f03729b069a7c21b59b1736ddfee5db5946c5da8c0121033b9b137ee87d5a812d6f506efdd37f0affa7ffc310711c06c7f3e097c9447c52ffffffff0100e1f505000000001976a9140389035a9225b3839e2bbf32d826a1e222031fd888ac00000000";
    BOOST_CHECK_NO_THROW(r = CallRPC(string("decoderawtransaction ")+rawtx));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "version").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "locktime").get_int(), 0);
    BOOST_CHECK_THROW(r = CallRPC(string("decoderawtransaction ")+rawtx+" extra"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("signrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction ff00"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx));
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx+" null null NONE|ANYONECANPAY"));
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx+" [] [] NONE|ANYONECANPAY"));
    BOOST_CHECK_THROW(CallRPC(string("signrawtransaction ")+rawtx+" null null badenum"), runtime_error);

    // Only check failure cases for sendrawtransaction, there's no network to send to...
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction DEADBEEF"), runtime_error);
    BOOST_CHECK_THROW(CallRPC(string("sendrawtransaction ")+rawtx+" extra"), runtime_error);
}

static void ShowWhy( Value& result ) {
    string strPrint;

    if (result.type() == null_type)
        strPrint = "Null Response";
    else if (result.type() == str_type)
        strPrint = result.get_str();
    else
        strPrint = write_string(result, true);
    printf( "\nJSON Response=%s\n", strPrint.c_str() );
}

BOOST_AUTO_TEST_CASE(rpc_rawsign)
{
    Value r;
    // input is a 1-of-2 multisig (so is output):
    string prevout =
    "[{\"txid\":\"ee9322302672ce461f8a488767463f4956623ff3f715363e7119f98d7a8d082d\","
    "\"vout\":1,\"scriptPubKey\":\"a914253a62409e0c588e3e70dbcf516b8d79d1b8dd5487\","
    "\"redeemScript\":\"514104cd89f47bbefb7f764aa5582671d68ba66737046c180049da1fa33230d7c1151809fa98c826e01874f042ed975ae18fb0f75e50e56905a4c7eb50a5778a20e1324104c5d9e90fe378de23bace0d74032fb53ef8dce16522e2e5cde84f4892832ff44f9e97ea82e6571c9c9d8256b752b39228b8045230abfa8becfa9fabc975aca41e52ae\"}]";
    r = CallRPC(string("createrawtransaction ")+prevout+" "+
    "{\"378h5299NFCUAB5NaBprCPMGf1gJS4fU6S\":0.1}");
    string notsigned = r.get_str();
    string privkey1 = "\"64yQeJSK2bEgSJXFgvYPZZy4EYbwvPywiR4zNv5ZSSgqHkKNEgr\"";
    string privkey2 = "\"6653tjwzUf4itcZePKUTdcqrcvGnwyZEAwpE5vNZrZqZdWfcA8i\"";
    r = CallRPC(string("signrawtransaction ")+notsigned+" "+prevout+" "+"[]");
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == false);
    r = CallRPC(string("signrawtransaction ")+notsigned+" "+prevout+" "+"["+privkey1+","+privkey2+"]");
    // ShowWhy( r );
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == true);
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values)
{
    BOOST_CHECK(write_string(ValueFromAmount(0LL), false) == "0.00000000");
    BOOST_CHECK(write_string(ValueFromAmount(1LL), false) == "0.00000001");
    BOOST_CHECK(write_string(ValueFromAmount(17622195LL), false) == "0.17622195");
    BOOST_CHECK(write_string(ValueFromAmount(50000000LL), false) == "0.50000000");
    BOOST_CHECK(write_string(ValueFromAmount(89898989LL), false) == "0.89898989");
    BOOST_CHECK(write_string(ValueFromAmount(100000000LL), false) == "1.00000000");
    BOOST_CHECK(write_string(ValueFromAmount(2099999999999990LL), false) == "20999999.99999990");
    BOOST_CHECK(write_string(ValueFromAmount(2099999999999999LL), false) == "20999999.99999999");
}

static Value ValueFromString(const std::string &str)
{
    Value value;
    BOOST_CHECK(read_string(str, value));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values)
{
    BOOST_CHECK(AmountFromValue(ValueFromString("0.00000001")) == 1LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.17622195")) == 17622195LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.5")) == 50000000LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.50000000")) == 50000000LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.89898989")) == 89898989LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("1.00000000")) == 100000000LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("2099999.9999999")) == 209999999999990LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("2099999.99999999")) == 209999999999999LL);
}

BOOST_AUTO_TEST_SUITE_END()
