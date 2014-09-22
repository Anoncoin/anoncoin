// CoinDenomination, PublicCoin, and PrivateCoin classes for Zerocoin.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.

#ifndef COIN_H_
#define COIN_H_

#include "../Zerocoin.h"

#include "../bignum.h"
#include "Params.h"
namespace libzerocoin {

class CoinDenomination {
public:
	// Anoncoin denominations are (in satoshis) either 10^e or 5*10^e where e is in [0, 11]
	// throws ZerocoinException if value not one of the Anoncoin denominations
	explicit CoinDenomination(int64 value) {
		if (value <= 0) {
			throw ZerocoinException("coin denomination must be positive");
		}
		int64 e = 0, m = value;
		while (m % 10 == 0) {
			m /= 10;
			e++;
		}
		if (e > 11 || (m != 1 && m != 5)) {
			throw ZerocoinException("coin denomination is invalid");
		}
		this->value = value;
		this->initialized = true;
	}

	CoinDenomination() : initialized(false) { }

	// value in satoshis
	// throws ZerocoinException if constructed w/ default constructor
	int64 getValue() const {
		if (!this->initialized) {
			throw ZerocoinException("coin denomination not initialized");
		}
		return this->value;
	}

	// throws ZerocoinException if either are not initialized
	bool operator==(const CoinDenomination& d) const {
		if (!this->initialized || !d.initialized) {
			throw ZerocoinException("cannot compare denominations when one or both are not initialized");
		}
		return this->value == d.value;
	}

	// throws ZerocoinException if either are not initialized
	bool operator!=(const CoinDenomination& d) const {
		return !CoinDenomination::operator==(d);
	}

private:
	int64 value;
	bool initialized;

public:
	IMPLEMENT_SERIALIZE_AND_SET_INIT
	(
		READWRITE(value);
	)
};

/** A Public coin is the part of a coin that
 * is published to the network and what is handled
 * by other clients. It contains only the value
 * of commitment to a serial number and the
 * denomination of the coin.
 */
class PublicCoin {
public:
	template<typename Stream>
	PublicCoin(const Params* p, Stream& strm): params(p) {
		strm >> *this;
	}

	PublicCoin( const Params* p);

	/**Generates a public coin
	 *
	 * @param p cryptographic paramters
	 * @param coin the value of the commitment.
	 * @param denomination The denomination of the coin.
	 */
	PublicCoin( const Params* p, const Bignum& coin, const CoinDenomination d);
	Bignum getValue() const;
	CoinDenomination getDenomination() const;
	bool operator==(const PublicCoin& rhs) const;
	bool operator!=(const PublicCoin& rhs) const;
	/** Checks that a coin prime
	 *  and in the appropriate range
	 *  given the parameters
	 * @return true if valid
	 */
    bool validate() const;
	IMPLEMENT_SERIALIZE_AND_SET_INIT
	(
	    READWRITE(value);
	    READWRITE(denomination);
	)
private:
	const Params* params;
	Bignum value;
	CoinDenomination denomination;
	bool initialized;
};

/**
 * A private coin. As the name implies, the content
 * of this should stay private except PublicCoin.
 *
 * Contains a coin's serial number, a commitment to it,
 * and opening randomness for the commitment.
 *
 * @warning Failure to keep this secret(or safe),
 * @warning will result in the theft of your coins
 * @warning and a TOTAL loss of anonymity.
 */
class PrivateCoin {
public:
	template<typename Stream>
	PrivateCoin(const Params* p, Stream& strm): params(p), publicCoin(p) {
		strm >> *this;
	}
	PrivateCoin(const Params* p,const CoinDenomination denomination);
	PublicCoin getPublicCoin() const;
	Bignum getSerialNumber() const;
	Bignum getRandomness() const;

	IMPLEMENT_SERIALIZE_AND_SET_INIT
	(
	    READWRITE(publicCoin);
	    READWRITE(randomness);
	    READWRITE(serialNumber);
	)
private:
	const Params* params;
	PublicCoin publicCoin;
	Bignum randomness;
	Bignum serialNumber;
	bool initialized;

	/**
	 * @brief Mint a new coin.
	 * @param denomination the denomination of the coin to mint
	 * @throws ZerocoinException if the process takes too long
	 *
	 * Generates a new Zerocoin by (a) selecting a random serial
	 * number, (b) committing to this serial number and repeating until
	 * the resulting commitment is prime. Stores the
	 * resulting commitment (coin) and randomness (trapdoor).
	 **/
	void mintCoin(const CoinDenomination denomination);
	
	/**
	 * @brief Mint a new coin using a faster process.
	 * @param denomination the denomination of the coin to mint
	 * @throws ZerocoinException if the process takes too long
	 *
	 * Generates a new Zerocoin by (a) selecting a random serial
	 * number, (b) committing to this serial number and repeating until
	 * the resulting commitment is prime. Stores the
	 * resulting commitment (coin) and randomness (trapdoor).
	 * This routine is substantially faster than the
	 * mintCoin() routine, but could be more vulnerable
	 * to timing attacks. Don't use it if you think someone
	 * could be timing your coin minting.
	 **/
	void mintCoinFast(const CoinDenomination denomination);

};

} /* namespace libzerocoin */
#endif /* COIN_H_ */
