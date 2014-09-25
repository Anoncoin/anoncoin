// Exceptions and constants for Zerocoin.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.

#ifndef ZEROCOIN_H_
#define ZEROCOIN_H_

#include <stdexcept>
#include <boost/shared_ptr.hpp>

#define ZEROCOIN_DEFAULT_SECURITYLEVEL      80
#define ZEROCOIN_MIN_SECURITY_LEVEL         80
#define ZEROCOIN_MAX_SECURITY_LEVEL         80
#define ACCPROOF_KPRIME                     160
#define ACCPROOF_KDPRIME                    128
#define MAX_COINMINT_ATTEMPTS               10000
#define ZEROCOIN_MINT_PRIME_PARAM			20
#define ZEROCOIN_VERSION_STRING             "0.13"
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
//#define	ZEROCOIN_FAST_MINT 1

// Errors thrown by the Zerocoin library


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
