// Parameter class for Zerocoin.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.

#include "../Zerocoin.h"

namespace libzerocoin {

Params::Params(uint32_t securityLevel) {
	this->zkp_hash_len = securityLevel;
	this->zkp_iterations = securityLevel;

	this->accumulatorParams.k_prime = ACCPROOF_KPRIME;
	this->accumulatorParams.k_dprime = ACCPROOF_KDPRIME;

	// Generate the parameters
	CalculateParams(*this, ZEROCOIN_PROTOCOL_VERSION, securityLevel);

	this->accumulatorParams.initialized = true;
	this->initialized = true;
}

AccumulatorAndProofParams::AccumulatorAndProofParams() {
	this->initialized = false;
}

IntegerGroupParams::IntegerGroupParams(): generatorsAreValid(true) {
	this->initialized = false;
}

Bignum IntegerGroupParams::randomElement() const {
	// The generator of the group raised
	// to a random number less than the order of the group
	// provides us with a uniformly distributed random number.
	return this->g().pow_mod(Bignum::randBignum(this->groupOrder),this->modulus);
}

} /* namespace libzerocoin */
