// Accumulator and AccumulatorWitness classes for Zerocoin.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.

#ifndef ACCUMULATOR_H_
#define ACCUMULATOR_H_

#include "../Zerocoin.h"

namespace libzerocoin {
/**
 * \brief Implementation of the RSA-based accumulator.
 **/

class Accumulator {
public:

	/**
	 * @brief      Construct an Accumulator from a stream.
	 * @param p    An AccumulatorAndProofParams object containing global parameters
	 * @param d    the denomination of coins we are accumulating
	 * @throw      Zerocoin exception in case of invalid parameters
	 **/
	template<typename Stream>
	Accumulator(const AccumulatorAndProofParams* p, Stream& strm): params(p) {
		strm >> *this;
	}

	template<typename Stream>
	Accumulator(const Params* p, Stream& strm) {
		strm >> *this;
		this->params = &(p->accumulatorParams);
	}

	// GNOSIS TODO: need the following for any reason?
	//Accumulator() { }

	/**
	 * @brief      Construct an Accumulator from a Params object.
	 * @param p    A Params object containing global parameters
	 * @param d the denomination of coins we are accumulating
	 * @throw     Zerocoin exception in case of invalid parameters
	 **/
	Accumulator(const AccumulatorAndProofParams* p, const CoinDenomination d);

	/**
	 * Accumulate a coin into the accumulator. Validates
	 * the coin prior to accumulation.
	 *
	 * @param coin	A PublicCoin to accumulate.
	 *
	 * @throw		Zerocoin exception if the coin is not valid.
	 *
	 **/
	void accumulate(const PublicCoin &coin);

	CoinDenomination getDenomination() const;

	/** Get the accumulator result
	 *
	 * @return a Bignum containing the result.
	 */
	std::vector<Bignum> getValue() const;

	/**
	 *
	 * @return the value of the accumulator for a single modulus
	 * @throws std::out_of_range if the index is out of bounds
	 */
	Bignum getValue(unsigned int modulusIdx) const;



	// /**
	//  * Used to set the accumulator value
	//  *
	//  * Use this to handle accumulator checkpoints
	//  * @param b the value to set the accumulator to.
	//  * @throw  A ZerocoinException if the accumulator value is invalid.
	//  */
	// void setValue(Bignum &b); // shouldn't this be a constructor?

	/** Used to accumulate a coin
	 *
	 * @param c the coin to accumulate
	 * @return a refrence to the updated accumulator.
	 */
	Accumulator& operator +=(const PublicCoin& c);
	bool operator==(const Accumulator rhs) const;

	IMPLEMENT_SERIALIZE
	(
	    READWRITE(value);
	    READWRITE(denomination);
	)
private:
	const AccumulatorAndProofParams* params;
	std::vector<Bignum> value;
	CoinDenomination denomination;
};

/**A witness that a PublicCoin is in the accumulation of a set of coins
 *
 */
class AccumulatorWitness {
public:
	template<typename Stream>
	AccumulatorWitness(const Params* p, Stream& strm): params(p) {
		strm >> *this;
	}

	/**  Constructs a witness.  You must add all elements after the witness
	 * @param p pointer to params
	 * @param checkpoint the last known accumulator value before the element was added
	 * @param coin the coin we want a witness to
	 */
	AccumulatorWitness(const Params* p, const Accumulator& checkpoint, const PublicCoin coin);

	/** Adds element to the set whose accumulation we are proving coin is a member of
	 *
	 * @param c the coin to add
	 */
	void addElement(const PublicCoin& c);

	/**
	 *
	 * @return the value of the witnesses for all accumulator moduli
	 */
	std::vector<Bignum> getValue() const;

	/**
	 *
	 * @return the value of the witness for a single accumulator modulus
	 * @throws std::out_of_range if the index is out of bounds
	 */
	Bignum getValue(unsigned int modulusIdx) const;

	/** Checks that this is a witness to the accumulation of coin
	 * @param a             the accumulator we are checking against.
	 * @param publicCoin    the coin we're providing a witness for
	 * @return True if the witness computation validates
	 */
	bool verifyWitness(const Accumulator& a, const PublicCoin &publicCoin) const;

	/**
	 * Adds rhs to the set whose accumulation we're proving coin is a member of
	 * @param rhs the PublicCoin to add
	 * @return
	 */
	AccumulatorWitness& operator +=(const PublicCoin& rhs);
private:
	const Params* params;
	Accumulator witness;
	const PublicCoin element;
};

} /* namespace libzerocoin */
#endif /* ACCUMULATOR_H_ */
