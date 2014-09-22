// Accumulator and AccumulatorWitness classes for Zerocoin.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.

#include <sstream>
#include <sys/time.h>
#include "../Zerocoin.h"

namespace libzerocoin {

//Accumulator class
Accumulator::Accumulator(const AccumulatorAndProofParams* p, const CoinDenomination d): params(p), denomination(d) {
	if (!(params->initialized)) {
		throw ZerocoinException("Invalid parameters for accumulator");
	}

	// copy in the accumulator bases
	//TODO: make this less fucking verbose... BOOST_FOREACH?
	for (std::vector<Bignum>::const_iterator it = this->params->accumulatorBases.begin(); it < this->params->accumulatorBases.end(); it++) {
		this->value.push_back(*it);
	}

	if (this->value.size() != UFO_COUNT) {
		throw ZerocoinException("FATAL: number of elements in accumulator must match UFO count");
	}
}

void Accumulator::accumulate(const PublicCoin& coin) {
	// Make sure we're initialized
	if(this->value.size() == 0) {
		throw ZerocoinException("attempted to accumulate a coin in an Accumulator that's not initialized");
	}

	if(this->denomination != coin.getDenomination()) {
		//std::stringstream msg;
		std::string msg;
		msg = "Wrong denomination for coin. Expected coins of denomination: ";
		msg += this->denomination.getValue();
		msg += ". Instead, got a coin of denomination: ";
		msg += coin.getDenomination().getValue();
		throw ZerocoinException(msg);
	}

	if(!coin.validate()) {
		throw ZerocoinException("Coin is not valid");
	}

	struct timeval tv0, tv1;
	double elapsed;
	// Compute new accumulator = for each UFO N_i: "old accumulator_i"^{minted_coin} mod N_i
	gettimeofday(&tv0, NULL);
	for (uint32_t i = 0; i < this->value.size(); i++) {
		this->value[i] = this->value[i].pow_mod(coin.getValue(), this->params->accumulatorModuli.at(i));
	}
	gettimeofday(&tv1, NULL);
	elapsed = (tv1.tv_sec  - tv0.tv_sec) +
			  (tv1.tv_usec - tv0.tv_usec) / 1e6;
}

CoinDenomination Accumulator::getDenomination() const {
	if(this->value.size() == 0) {
		throw ZerocoinException("attempted to get denomination of a coin in an Accumulator that's not initialized");
	}
	return this->denomination;
}

std::vector<Bignum> Accumulator::getValue() const {
	if(this->value.size() == 0) {
		throw ZerocoinException("attempted to get value of a coin in an Accumulator that's not initialized");
	}
	return this->value;
}

Bignum Accumulator::getValue(unsigned int modulusIdx) const {
	if(this->value.size() == 0) {
		throw ZerocoinException("attempted to get value of a coin in an Accumulator that's not initialized");
	}
	return this->value.at(modulusIdx);
}

Accumulator& Accumulator::operator += (const PublicCoin& c) {
	this->accumulate(c);
	return *this;
}

bool Accumulator::operator == (const Accumulator rhs) const {
	if(this->value.size() == 0 || rhs.value.size() == 0) {
		throw ZerocoinException("attempted to compare Accumulators when one or both are not initialized");
	}
	return this->value == rhs.value;
}

//AccumulatorWitness class
AccumulatorWitness::AccumulatorWitness(const Params* p,
                                       const Accumulator& checkpoint, const PublicCoin coin): params(p), witness(checkpoint), element(coin) {
}

void AccumulatorWitness::addElement(const PublicCoin& c) {
	if(element != c) {
		witness += c;
	}
}

std::vector<Bignum> AccumulatorWitness::getValue() const {
	return this->witness.getValue();
}

Bignum AccumulatorWitness::getValue(unsigned int modulusIdx) const {
	return this->witness.getValue(modulusIdx);
}

bool AccumulatorWitness::verifyWitness(const Accumulator& a, const PublicCoin &publicCoin) const {
	Accumulator temp(witness);
	temp += element;
	return (temp == a && this->element == publicCoin);
}

AccumulatorWitness& AccumulatorWitness::operator +=(
    const PublicCoin& rhs) {
	this->addElement(rhs);
	return *this;
}

} /* namespace libzerocoin */
