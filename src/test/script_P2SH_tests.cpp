// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script.h"

#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script_error.h"
#include "sigcache.h"
#include "sign.h"
#include "transaction.h"

#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

// Test routines internal to script.cpp:
extern uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);

// Helpers:
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s);
    return sSerialized;
}

static bool
Verify(const CScript& scriptSig, const CScript& scriptPubKey, bool fStrict)
{
    ScriptError err;
    // Create dummy to/from transactions:
    CMutableTransaction mtxFrom;
    mtxFrom.vout.resize(1);
    mtxFrom.vout[0].scriptPubKey = scriptPubKey;
    CTransaction txFrom( mtxFrom );

    CMutableTransaction mtxTo;
    mtxTo.vin.resize(1);
    mtxTo.vout.resize(1);
    mtxTo.vin[0].prevout.n = 0;
    mtxTo.vin[0].prevout.hash = txFrom.GetHash();
    mtxTo.vin[0].scriptSig = scriptSig;
    mtxTo.vout[0].nValue = 1;
    //CTransaction txTo( mtxTo );

    return VerifyScript(scriptSig, scriptPubKey, fStrict ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE, MutableTransactionSignatureChecker(&mtxTo, 0), &err);
    //return VerifyScript(scriptSig, scriptPubKey, txTo, 0, fStrict ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE, 0);
}


BOOST_AUTO_TEST_SUITE(script_P2SH_tests)

BOOST_AUTO_TEST_CASE(sign)
{
    LOCK(cs_main);
    // Pay-to-script-hash looks like this:
    // scriptSig:    <sig> <sig...> <serialized_script>
    // scriptPubKey: HASH160 <hash> EQUAL

    // Test SignSignature() (and therefore the version of Solver() that signs transactions)
    CBasicKeyStore keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        keystore.AddKey(key[i]);
    }

    // 8 Scripts: checking all combinations of
    // different keys, straight/P2SH, pubkey/pubkeyhash
    CScript standardScripts[4];
    standardScripts[0] << key[0].GetPubKey() << OP_CHECKSIG;
    standardScripts[1].SetDestination(key[1].GetPubKey().GetID());
    standardScripts[2] << key[1].GetPubKey() << OP_CHECKSIG;
    standardScripts[3].SetDestination(key[2].GetPubKey().GetID());
    CScript evalScripts[4];
    for (int i = 0; i < 4; i++)
    {
        keystore.AddCScript(standardScripts[i]);
        evalScripts[i].SetDestination(standardScripts[i].GetID());
    }

    CMutableTransaction mtxFrom;  // Funding transaction:
    string reason;
    mtxFrom.vout.resize(8);
    for (int i = 0; i < 4; i++)
    {
        mtxFrom.vout[i].scriptPubKey = evalScripts[i];
        mtxFrom.vout[i].nValue = COIN;
        mtxFrom.vout[i+4].scriptPubKey = standardScripts[i];
        mtxFrom.vout[i+4].nValue = COIN;
    }
    CTransaction txFrom( mtxFrom );
    BOOST_CHECK(IsStandardTx(txFrom, reason));

    CMutableTransaction mtxTo[8];   // Spending transactions
    for (int i = 0; i < 8; i++)
    {
        mtxTo[i].vin.resize(1);
        mtxTo[i].vout.resize(1);
        mtxTo[i].vin[0].prevout.n = i;
        mtxTo[i].vin[0].prevout.hash = txFrom.GetHash();
        mtxTo[i].vout[0].nValue = 1;
        BOOST_CHECK_MESSAGE(IsMine(keystore, txFrom.vout[i].scriptPubKey), strprintf("IsMine %d", i));
    }
    for (int i = 0; i < 8; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, txFrom, mtxTo[i], 0), strprintf("SignSignature %d", i));
    }
    // All of the above should be OK, and the txTos have valid signatures
    // Check to make sure signature verification fails if we use the wrong ScriptSig:
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
        {
            CScript sigSave = mtxTo[i].vin[0].scriptSig;
            mtxTo[i].vin[0].scriptSig = mtxTo[j].vin[0].scriptSig;
            //CTransaction txTmp(mtxTo[i]);
            bool sigOK = CScriptCheck(CCoins(txFrom, 0), mtxTo[i], 0, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC, false)();
            // bool sigOK = VerifySignature(CCoins(txFrom, 0), txTmp, 0, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC, 0);
            if (i == j)
                BOOST_CHECK_MESSAGE(sigOK, strprintf("VerifySignature %d %d", i, j));
            else
                BOOST_CHECK_MESSAGE(!sigOK, strprintf("VerifySignature %d %d", i, j));
            mtxTo[i].vin[0].scriptSig = sigSave;
        }
}

BOOST_AUTO_TEST_CASE(norecurse)
{
    // Make sure only the outer pay-to-script-hash does the
    // extra-validation thing:
    CScript invalidAsScript;
    invalidAsScript << OP_INVALIDOPCODE << OP_INVALIDOPCODE;

    CScript p2sh;
    p2sh.SetDestination(invalidAsScript.GetID());

    CScript scriptSig;
    scriptSig << Serialize(invalidAsScript);

    // Should not verify, because it will try to execute OP_INVALIDOPCODE
    BOOST_CHECK(!Verify(scriptSig, p2sh, true));

    // Try to recur, and verification should succeed because
    // the inner HASH160 <> EQUAL should only check the hash:
    CScript p2sh2;
    p2sh2.SetDestination(p2sh.GetID());
    CScript scriptSig2;
    scriptSig2 << Serialize(invalidAsScript) << Serialize(p2sh);

    BOOST_CHECK(Verify(scriptSig2, p2sh2, true));
}

BOOST_AUTO_TEST_CASE(set)
{
    LOCK(cs_main);
    // Test the CScript::Set* methods
    CBasicKeyStore keystore;
    CKey key[4];
    std::vector<CPubKey> keys;
    for (int i = 0; i < 4; i++)
    {
        key[i].MakeNewKey(true);
        keystore.AddKey(key[i]);
        keys.push_back(key[i].GetPubKey());
    }

    CScript inner[4];
    inner[0].SetDestination(key[0].GetPubKey().GetID());
    inner[1].SetMultisig(2, std::vector<CPubKey>(keys.begin(), keys.begin()+2));
    inner[2].SetMultisig(1, std::vector<CPubKey>(keys.begin(), keys.begin()+2));
    inner[3].SetMultisig(2, std::vector<CPubKey>(keys.begin(), keys.begin()+3));

    CScript outer[4];
    for (int i = 0; i < 4; i++)
    {
        outer[i].SetDestination(inner[i].GetID());
        keystore.AddCScript(inner[i]);
    }

    CMutableTransaction mtxFrom;  // Funding transaction:
    string reason;
    mtxFrom.vout.resize(4);
    for (int i = 0; i < 4; i++)
    {
        mtxFrom.vout[i].scriptPubKey = outer[i];
        mtxFrom.vout[i].nValue = CENT;
    }
    CTransaction txFrom( mtxFrom );
    BOOST_CHECK(IsStandardTx(txFrom, reason));

    CMutableTransaction mtxTo[4]; // Spending transactions
    CTransaction txTo[4];
    for (int i = 0; i < 4; i++)
    {
        mtxTo[i].vin.resize(1);
        mtxTo[i].vout.resize(1);
        mtxTo[i].vin[0].prevout.n = i;
        mtxTo[i].vin[0].prevout.hash = txFrom.GetHash();
        mtxTo[i].vout[0].nValue = 1*CENT;
        mtxTo[i].vout[0].scriptPubKey = inner[i];
        txTo[i] = mtxTo[i];
        BOOST_CHECK_MESSAGE(IsMine(keystore, txFrom.vout[i].scriptPubKey), strprintf("IsMine %d", i));
    }
    for (int i = 0; i < 4; i++)
    {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, txFrom, mtxTo[i], 0), strprintf("SignSignature %d", i));
        BOOST_CHECK_MESSAGE(IsStandardTx(txTo[i], reason), strprintf("txTo[%d].IsStandard", i));
    }
}

BOOST_AUTO_TEST_CASE(is)
{
    // Test CScript::IsPayToScriptHash()
    uint160 dummy;
    CScript p2sh;
    p2sh << OP_HASH160 << dummy << OP_EQUAL;
    BOOST_CHECK(p2sh.IsPayToScriptHash());

    // Not considered pay-to-script-hash if using one of the OP_PUSHDATA opcodes:
    static const unsigned char direct[] =    { OP_HASH160, 20, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(CScript(direct, direct+sizeof(direct)).IsPayToScriptHash());
    static const unsigned char pushdata1[] = { OP_HASH160, OP_PUSHDATA1, 20, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(!CScript(pushdata1, pushdata1+sizeof(pushdata1)).IsPayToScriptHash());
    static const unsigned char pushdata2[] = { OP_HASH160, OP_PUSHDATA2, 20,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(!CScript(pushdata2, pushdata2+sizeof(pushdata2)).IsPayToScriptHash());
    static const unsigned char pushdata4[] = { OP_HASH160, OP_PUSHDATA4, 20,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, OP_EQUAL };
    BOOST_CHECK(!CScript(pushdata4, pushdata4+sizeof(pushdata4)).IsPayToScriptHash());

    CScript not_p2sh;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_HASH160 << dummy << dummy << OP_EQUAL;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_NOP << dummy << OP_EQUAL;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());

    not_p2sh.clear(); not_p2sh << OP_HASH160 << dummy << OP_CHECKSIG;
    BOOST_CHECK(!not_p2sh.IsPayToScriptHash());
}

BOOST_AUTO_TEST_CASE(switchover)
{
    // Test switch over code
    CScript notValid;
    notValid << OP_11 << OP_12 << OP_EQUALVERIFY;
    CScript scriptSig;
    scriptSig << Serialize(notValid);

    CScript fund;
    fund.SetDestination(notValid.GetID());


    // Validation should succeed under old rules (hash is correct):
    BOOST_CHECK(Verify(scriptSig, fund, false));
    // Fail under new:
    BOOST_CHECK(!Verify(scriptSig, fund, true));
}

BOOST_AUTO_TEST_CASE(AreInputsStandard)
{
    LOCK(cs_main);
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    CBasicKeyStore keystore;
    CKey key[3];
    vector<CPubKey> keys;
    for (int i = 0; i < 3; i++)
    {
        key[i].MakeNewKey(true);
        keystore.AddKey(key[i]);
        keys.push_back(key[i].GetPubKey());
    }

    CMutableTransaction mtxFrom;
    mtxFrom.vout.resize(6);

    // First three are standard:
    CScript pay1; pay1.SetDestination(key[0].GetPubKey().GetID());
    keystore.AddCScript(pay1);
    CScript payScriptHash1; payScriptHash1.SetDestination(pay1.GetID());
    CScript pay1of3; pay1of3.SetMultisig(1, keys);

    mtxFrom.vout[0].scriptPubKey = payScriptHash1;
    mtxFrom.vout[0].nValue = 1000;
    mtxFrom.vout[1].scriptPubKey = pay1;
    mtxFrom.vout[1].nValue = 2000;
    mtxFrom.vout[2].scriptPubKey = pay1of3;
    mtxFrom.vout[2].nValue = 3000;

    // Last three non-standard:
    CScript empty;
    keystore.AddCScript(empty);
    mtxFrom.vout[3].scriptPubKey = empty;
    mtxFrom.vout[3].nValue = 4000;
    // Can't use SetPayToScriptHash, it checks for the empty Script. So:
    mtxFrom.vout[4].scriptPubKey << OP_HASH160 << Hash160(empty) << OP_EQUAL;
    mtxFrom.vout[4].nValue = 5000;
    CScript oneOfEleven;
    oneOfEleven << OP_1;
    for (int i = 0; i < 11; i++)
        oneOfEleven << key[0].GetPubKey();
    oneOfEleven << OP_11 << OP_CHECKMULTISIG;
    mtxFrom.vout[5].scriptPubKey.SetDestination(oneOfEleven.GetID());
    mtxFrom.vout[5].nValue = 6000;

    CTransaction txFrom( mtxFrom );
    coins.ModifyCoins(txFrom.GetHash())->FromTx(txFrom, 0);

    CMutableTransaction mtxTo;
    mtxTo.vout.resize(1);
    mtxTo.vout[0].scriptPubKey.SetDestination(key[1].GetPubKey().GetID());

    mtxTo.vin.resize(3);
    mtxTo.vin[0].prevout.n = 0;
    mtxTo.vin[0].prevout.hash = txFrom.GetHash();
    BOOST_CHECK(SignSignature(keystore, txFrom, mtxTo, 0));
    mtxTo.vin[1].prevout.n = 1;
    mtxTo.vin[1].prevout.hash = txFrom.GetHash();
    BOOST_CHECK(SignSignature(keystore, txFrom, mtxTo, 1));
    mtxTo.vin[2].prevout.n = 2;
    mtxTo.vin[2].prevout.hash = txFrom.GetHash();
    BOOST_CHECK(SignSignature(keystore, txFrom, mtxTo, 2));

    CTransaction txTo( mtxTo );
    BOOST_CHECK(::AreInputsStandard(txTo, coins));
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(txTo, coins), 1U);

    // Make sure adding crap to the scriptSigs makes them non-standard:
    for (int i = 0; i < 3; i++)
    {
        CScript t = txTo.vin[i].scriptSig;
        mtxTo.vin[i].scriptSig = (CScript() << 11) + t;
        txTo = mtxTo;
        BOOST_CHECK(!::AreInputsStandard(txTo, coins));
        mtxTo.vin[i].scriptSig = t;
    }

    CMutableTransaction mtxToNonStd;
    mtxToNonStd.vout.resize(1);
    mtxToNonStd.vout[0].scriptPubKey.SetDestination(key[1].GetPubKey().GetID());
    mtxToNonStd.vout[0].nValue = 1000;
    mtxToNonStd.vin.resize(2);
    mtxToNonStd.vin[0].prevout.n = 4;
    mtxToNonStd.vin[0].prevout.hash = txFrom.GetHash();
    mtxToNonStd.vin[0].scriptSig << Serialize(empty);
    mtxToNonStd.vin[1].prevout.n = 5;
    mtxToNonStd.vin[1].prevout.hash = txFrom.GetHash();
    mtxToNonStd.vin[1].scriptSig << OP_0 << Serialize(oneOfEleven);

    CTransaction txToNonStd( mtxToNonStd );
    BOOST_CHECK(!::AreInputsStandard(txToNonStd, coins));
    BOOST_CHECK_EQUAL(GetP2SHSigOpCount(txToNonStd, coins), 11U);

    mtxToNonStd.vin[0].scriptSig.clear();
    txToNonStd = mtxToNonStd;
    BOOST_CHECK(!::AreInputsStandard(txToNonStd, coins));
}

BOOST_AUTO_TEST_SUITE_END()
