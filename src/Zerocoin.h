/**
* @file       Zerocoin.h
*
* @brief      Exceptions and constants for Zerocoin
*
* @author     Ian Miers, Christina Garman and Matthew Green
* @date       June 2013
*
* @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
* @license    This project is released under the MIT license.
**/

#ifndef ZEROCOIN_H_
#define ZEROCOIN_H_

#include <stdexcept>

#define ZEROCOIN_DEFAULT_SECURITYLEVEL      80
#define ZEROCOIN_MIN_SECURITY_LEVEL         80
#define ZEROCOIN_MAX_SECURITY_LEVEL         80
#define ACCPROOF_KPRIME                     160
#define ACCPROOF_KDPRIME                    128
#define MAX_COINMINT_ATTEMPTS               10000
#define ZEROCOIN_MINT_PRIME_PARAM			20
#define ZEROCOIN_VERSION_STRING             "0.12"
#define ZEROCOIN_VERSION_INT				11
#define ZEROCOIN_PROTOCOL_VERSION           "1"
#define HASH_OUTPUT_BITS                    256
#define ZEROCOIN_COMMITMENT_EQUALITY_PROOF  "COMMITMENT_EQUALITY_PROOF"
#define ZEROCOIN_ACCUMULATOR_PROOF          "ACCUMULATOR_PROOF"
#define ZEROCOIN_SERIALNUMBER_PROOF         "SERIALNUMBER_PROOF"

// Activate multithreaded mode for proof verification
// we won't be using OpenMP because of the way we will be
// integrating with Anoncoin
//#define ZEROCOIN_THREADING 1

// Uses a fast technique for coin generation. Could be more vulnerable
// to timing attacks. Turn off if an attacker can measure coin minting time.
#define	ZEROCOIN_FAST_MINT 1

// Errors thrown by the Zerocoin library


/////////////////////////

// Zerocoin modulus
//

// Testnet4 Modulus. Testnet will not use UFOs. At least not testnet4, probably in testnet5.
#define TESTNET_MODULUS "ceb0d4a98803da14042e2fed5d0b324fc15cc668e62c5dc016f3165a7220c789c5ac216c821f6ed02989ae69df53d546308497a0a19ab2df1c8fdbf8c14d3e301b4dac7ff087ee3e2e15aaf6616a431af6fb635a3dcf23d0ee49b99ca79017e87ef6484a2bd2691a0134f6be4b805f7c642dbbef829dec4094d4120d36703596f26cd3fa29e4d14a4d68343f0f6c72418e9d48f09d577b595e81badb0ba043d5994c1d766ba5c0397bf769479fa9223c093c38a4d7fedd7e1edd3bd1acbf189fc71e7d7e17ae376778818d160184e0ea4117b60d73fa1754e2dfbcdd8ed10ef4c8df68f23d3ec5c498c3cdae766506cbf4303f944cb4e0d4de38a9f86824ba08c7478aadac51e9d01ae53600c69d971b15f85894d0b984a019d2a5f5dd626e87014f80e69c35efbddd4a2b478c70cda55578a8c2331b48a21a82c465196bd8c1a86734666ffe1524bdc15ad711940dbfc80160dfa298aa8fc939c94c34d57b36c18a512a83ba771ccfb391955d61e3eabc98f7a9f0e58c51946c75da5ef94a8d"

/////////////////////////

class ZerocoinException : public std::runtime_error
{
public:
	explicit ZerocoinException(const std::string& str) : std::runtime_error(str) {}
};

// use the Anoncoin versions (previously: modified versions in bitcoin_bignum/)
#include "serialize.h"
#include "bignum.h"
#include "hash.h"

typedef CBigNum Bignum;

#include "zerocoin/Params.h"
#include "zerocoin/Coin.h"
#include "zerocoin/Commitment.h"
#include "zerocoin/Accumulator.h"
#include "zerocoin/AccumulatorProofOfKnowledge.h"
#include "zerocoin/CoinSpend.h"
#include "zerocoin/SerialNumberSignatureOfKnowledge.h"
#include "zerocoin/ParamGeneration.h"

#endif /* ZEROCOIN_H_ */
