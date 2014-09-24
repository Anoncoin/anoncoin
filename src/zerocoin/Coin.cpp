// PublicCoin and PrivateCoin classes for Zerocoin.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.

#include <stdexcept>
#include "../Zerocoin.h"

namespace libzerocoin {

//PublicCoin class
PublicCoin::PublicCoin(const Params* p):
	params(p),
    initialized(false) {
	if (this->params->initialized == false) {
		throw ZerocoinException("Params are not initialized");
	}
};

PublicCoin::PublicCoin(const Params* p, const Bignum& coin):
	params(p),
	value(coin),
	has_denomination(false),
	initialized(true)
{
	if (this->params->initialized == false) {
		throw ZerocoinException("Params are not initialized");
	}
};

PublicCoin::PublicCoin(const Params* p, const Bignum& coin, const CoinDenomination d):
	params(p),
	value(coin),
	denomination(d),
	has_denomination(true),
	initialized(true)
{
	if (this->params->initialized == false) {
		throw ZerocoinException("Params are not initialized");
	}
};

bool PublicCoin::operator==(const PublicCoin& rhs) const {
	// FIXME check param equality
	if (!this->initialized || !rhs.initialized) {
		throw ZerocoinException("attempted to compare a PublicCoin that was not initialized");
	}
	return this->value == rhs.value && this->denomination == rhs.denomination;
}

bool PublicCoin::operator!=(const PublicCoin& rhs) const {
	return !(*this == rhs);
}

Bignum PublicCoin::getValue() const {
	if (!this->initialized) {
		throw ZerocoinException("attempted to get value of PublicCoin that was not initialized!");
	}
	return this->value;
}

CoinDenomination PublicCoin::getDenomination() const {
	if (!this->initialized) {
		throw ZerocoinException("attempted to get denomination of PublicCoin that was not initialized!");
	}
	return this->denomination;
}

// set the denomination if coin does not have one; otherwise, throw ZerocoinException
void PublicCoin::setDenomination(const CoinDenomination d) {
	if (this->has_denomination) {
		throw ZerocoinException("attempted to set denomination of PublicCoin that already has one");
	}

	this->denomination = d;
	this->has_denomination = true;
}

bool PublicCoin::validate() const{
	if (!this->initialized) {
		throw ZerocoinException("attempted to validate PublicCoin that was not initialized!");
	}
	return (this->params->accumulatorParams.minCoinValue < value) &&
		   (value < this->params->accumulatorParams.maxCoinValue) &&
		   value.isPrime(params->zkp_iterations);
}

//PrivateCoin class
PrivateCoin::PrivateCoin(const Params* p): params(p), publicCoin(p), initialized(false) {
	this->_init();
}

PrivateCoin::PrivateCoin(const Params* p, const CoinDenomination denomination):
	params(p),
	publicCoin(p),
	initialized(false)
{
	this->_init();
	this->publicCoin.setDenomination(denomination);
}

void PrivateCoin::_init() {
	// Verify that the parameters are valid
	if(this->params->initialized == false) {
		throw ZerocoinException("Params are not initialized");
	}

#ifdef ZEROCOIN_FAST_MINT
	// Mint a new coin with a random serial number using the fast process.
	// This is more vulnerable to timing attacks so don't mint coins when
	// somebody could be timing you.
	this->mintCoinFast();
#else
	// Mint a new coin with a random serial number using the standard process.
	this->mintCoin();
#endif
	
	this->initialized = true;
}

/**
 *
 * @return the coin's serial number
 */
Bignum PrivateCoin::getSerialNumber() const {
	if (!this->initialized) {
		throw ZerocoinException("tried to get serial number of PrivateCoin that was not initialized");
	}
	return this->serialNumber;
}

Bignum PrivateCoin::getRandomness() const {
	if (!this->initialized) {
		throw ZerocoinException("tried to get randomness of PrivateCoin that was not initialized");
	}
	return this->randomness;
}

CoinDenomination PrivateCoin::getDenomination() const {
	return this->publicCoin.getDenomination();
}

void PrivateCoin::setDenomination(const CoinDenomination d) {
	this->publicCoin.setDenomination(d);
}

void PrivateCoin::mintCoin() {
	// Repeat this process up to MAX_COINMINT_ATTEMPTS times until
	// we obtain a prime number
	for(uint32_t attempt = 0; attempt < MAX_COINMINT_ATTEMPTS; attempt++) {

		// Generate a random serial number in the range 0...{q-1} where
		// "q" is the order of the commitment group.
		Bignum s = Bignum::randBignum(this->params->coinCommitmentGroup.groupOrder);

		// derive generators from serial number
		Bignum g, h;
		deriveGeneratorsFromSerialNumber(s, params->coinCommitmentGroup.modulus,
										 params->coinCommitmentGroup.groupOrder, g, h);

		// Generate a Pedersen commitment to the serial number "s"
		Commitment coin(&params->coinCommitmentGroup, s, g, h);

		// Now verify that the commitment is a prime number
		// in the appropriate range. If not, we'll throw this coin
		// away and generate a new one.
		if (coin.getCommitmentValue().isPrime(ZEROCOIN_MINT_PRIME_PARAM) &&
		        coin.getCommitmentValue() >= params->accumulatorParams.minCoinValue &&
		        coin.getCommitmentValue() <= params->accumulatorParams.maxCoinValue) {
			// Found a valid coin. Store it.
			this->serialNumber = s;
			this->randomness = coin.getRandomness();
			this->publicCoin = PublicCoin(params,coin.getCommitmentValue());

			// Success! We're done.
			return;
		}
	}

	// We only get here if we did not find a coin within
	// MAX_COINMINT_ATTEMPTS. Throw an exception.
	throw ZerocoinException("Unable to mint a new Zerocoin (too many attempts)");
}

void PrivateCoin::mintCoinFast() {
	
	// Generate a random serial number in the range 0...{q-1} where
	// "q" is the order of the commitment group.
	Bignum s = Bignum::randBignum(this->params->coinCommitmentGroup.groupOrder);
	
	// Generate a random number "r" in the range 0...{q-1}
	Bignum r = Bignum::randBignum(this->params->coinCommitmentGroup.groupOrder);
	
	Bignum cc_g, cc_h; // generators g & h for coin commitment group
	deriveGeneratorsFromSerialNumber(s, this->params->coinCommitmentGroup.modulus, this->params->coinCommitmentGroup.groupOrder, cc_g, cc_h);

	// Manually compute a Pedersen commitment to the serial number "s" under randomness "r"
	// C = g^s * h^r mod p
	Bignum commitmentValue = cc_g.pow_mod(s, this->params->coinCommitmentGroup.modulus).mul_mod(cc_h.pow_mod(r, this->params->coinCommitmentGroup.modulus), this->params->coinCommitmentGroup.modulus);
	
	// Repeat this process up to MAX_COINMINT_ATTEMPTS times until
	// we obtain a prime number
	for (uint32_t attempt = 0; attempt < MAX_COINMINT_ATTEMPTS; attempt++) {
		// First verify that the commitment is a prime number
		// in the appropriate range. If not, we'll throw this coin
		// away and generate a new one.
		if (commitmentValue.isPrime(ZEROCOIN_MINT_PRIME_PARAM) &&
			commitmentValue >= params->accumulatorParams.minCoinValue &&
			commitmentValue <= params->accumulatorParams.maxCoinValue) {
			// Found a valid coin. Store it.
			this->serialNumber = s;
			this->randomness = r;
			this->publicCoin = PublicCoin(params, commitmentValue);
				
			// Success! We're done.
			return;
		}
		
		// Generate a new random "r_delta" in 0...{q-1}
		Bignum r_delta = Bignum::randBignum(this->params->coinCommitmentGroup.groupOrder);

		// The commitment was not prime. Increment "r" and recalculate "C":
		// r = r + r_delta mod q
		// C = C * h mod p
		r = (r + r_delta) % this->params->coinCommitmentGroup.groupOrder;
		commitmentValue = commitmentValue.mul_mod(cc_h.pow_mod(r_delta, this->params->coinCommitmentGroup.modulus), this->params->coinCommitmentGroup.modulus);
	}
		
	// We only get here if we did not find a coin within
	// MAX_COINMINT_ATTEMPTS. Throw an exception.
	throw ZerocoinException("Unable to mint a new Zerocoin (too many attempts)");
}
	
PublicCoin PrivateCoin::getPublicCoin() const {
	return this->publicCoin;
}

} /* namespace libzerocoin */
