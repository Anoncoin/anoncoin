// Parameter manipulation routines for the Zerocoin cryptographic
// components.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Copyright 2013-2014 The Anoncoin developers.
// Distributed under the MIT license.


#include <iostream>				// GNOSIS DEBUG
#include <string>
#include "../Zerocoin.h"

namespace libzerocoin {

#define PRINT_BIGNUM(name, val)                                            \
{                                                                          \
  CBigNum v = (val);                                                       \
  std::cout << "GNOSIS DEBUG: " name << "(" << v.bitSize() << " bits) is " \
            << v.ToString(16) << std::endl;                                \
}

#define PRINT_GROUP_PARAMS(p)                                              \
{                                                                          \
  IntegerGroupParams _p = (p);                                             \
  PRINT_BIGNUM(""#p ".g", _p.g());                                           \
  PRINT_BIGNUM(""#p ".h", _p.h());                                           \
  PRINT_BIGNUM(""#p ".groupOrder", _p.groupOrder);                         \
  PRINT_BIGNUM(""#p ".modulus",    _p.modulus);                            \
  std::cout << std::endl;                                                  \
}

/// \brief Fill in a set of Zerocoin parameters deterministically.
/// \param aux              An optional auxiliary string used in derivation
/// \param securityLevel    A security level
///
/// \throws         ZerocoinException if the process fails
///
/// Fills in a ZC_Params data structure deterministically from
/// the results of the RSA UFO project (2014-06-31 - 2014-09-15).

void
CalculateParams(Params &params, string aux, uint32_t securityLevel)
{
	std::cout << "GNOSIS DEBUG: CalculateParams in ParamGeneration.cpp" << std::endl;
	params.initialized = false;
	params.accumulatorParams.initialized = false;
	std::cout << "GNOSIS DEBUG: aux is " << aux << std::endl;

	// Verify that "securityLevel" is  at least 80 bits (minimum).
	if (securityLevel < 80) {
		throw ZerocoinException("Security level must be at least 80 bits.");
	}
	std::cout << "GNOSIS DEBUG: securityLevel is " << securityLevel << std::endl;

	// Calculate UFOs
	calculateUFOs(params.accumulatorParams);

	Bignum ufo_sum(0);
	// Add the UFOs together for deterministically seeding all other parameters.
	// This is kind of arbitrary, but I had to pick something...
	vector<Bignum>& r_ufos = params.accumulatorParams.accumulatorModuli;
	for (uint32_t i = 0; i < r_ufos.size(); i++) {
		uint32_t N_i_len = r_ufos[i].bitSize();
		if (N_i_len < UFO_MIN_BIT_LENGTH) {
			throw ZerocoinException("RSA UFO modulus is too small");
		}
		std::cout << "GNOSIS DEBUG: accumulator modulus " << i << " is " << N_i_len
				  << " bits" <<  std::endl;
		ufo_sum += r_ufos[i];
	}

	// Calculate the required size of the field "F_p" into which
	// we're embedding the coin commitment group. This may throw an
	// exception if the securityLevel is too large to be supported
	// by the current modulus.
	uint32_t pLen = 0;
	uint32_t qLen = 0;
	calculateGroupParamLengths(2048, securityLevel, &pLen, &qLen);	// GNOSIS: replaced "NLen - 2" with 2048

	// Calculate candidate parameters ("p", "q") for the coin commitment group
	// using a deterministic process based on the RSA UFOs, the "aux" string, and
	// the dedicated string "COMMITMENTGROUP".
	params.coinCommitmentGroup = deriveIntegerGroupParams(calculateSeed(ufo_sum, aux, securityLevel, STRING_COMMIT_GROUP),
	                             pLen, qLen);
	// g and h are invalid, since they are now different for each coin; see
	// "Rational Zero" by Garman et al., section 4.4.
	params.coinCommitmentGroup.invalidateGenerators();
	PRINT_BIGNUM("params.coinCommitmentGroup.groupOrder", params.coinCommitmentGroup.groupOrder);
	PRINT_BIGNUM("params.coinCommitmentGroup.modulus", params.coinCommitmentGroup.modulus);

	// Next, we derive parameters for a second Accumulated Value commitment group.
	// This is a Schnorr group with the specific property that the order of the group
	// must be exactly equal to "q" from the commitment group. We set
	// the modulus of the new group equal to "2q+1" and test to see if this is prime.
	params.serialNumberSoKCommitmentGroup = deriveIntegerGroupFromOrder(params.coinCommitmentGroup.modulus);
	PRINT_GROUP_PARAMS(params.serialNumberSoKCommitmentGroup);

	// Calculate the parameters for the internal commitment
	// using the same process.
	params.accumulatorParams.accumulatorPoKCommitmentGroup = deriveIntegerGroupParams(calculateSeed(ufo_sum, aux, securityLevel, STRING_AIC_GROUP),
	        qLen + 300, qLen + 1);
	PRINT_GROUP_PARAMS(params.accumulatorParams.accumulatorPoKCommitmentGroup);

	//TODO: ONE FOR EACH UFO
	for (uint32_t i = 0; i < r_ufos.size(); i++) {
		// Calculate the parameters for the accumulator QRN commitment generators. This isn't really
		// a whole group, just a pair of random generators in QR_N.
		uint32_t resultCtr;
		IntegerGroupParams accQRNGrp;
		uint32_t N_i_len = r_ufos[i].bitSize();
		accQRNGrp.g(generateIntegerFromSeed(N_i_len - 1,
				calculateSeed(ufo_sum, aux, securityLevel, STRING_QRNCOMMIT_GROUPG),
				&resultCtr).pow_mod(Bignum(2), r_ufos[i]));
		accQRNGrp.h(generateIntegerFromSeed(N_i_len - 1,
				calculateSeed(ufo_sum, aux, securityLevel, STRING_QRNCOMMIT_GROUPH),
				&resultCtr).pow_mod(Bignum(2), r_ufos[i]));
		std::cout << "GNOSIS DEBUG: r_ufos[" << i << "] accumulator QR_N group setup:" << std::endl;
		PRINT_BIGNUM("SHOULD BE DIFFERENT FROM OTHER G: accQRNGrp.g", accQRNGrp.g());    // probably fine
		PRINT_BIGNUM("SHOULD BE DIFFERENT FROM OTHER H: accQRNGrp.h", accQRNGrp.h());

		params.accumulatorParams.accumulatorQRNCommitmentGroups.push_back(accQRNGrp);

		// Calculate the accumulator base, which we calculate as "u = C**2 mod N"
		// where C is an arbitrary value. In the unlikely case that "u = 1" we increment
		// "C" and repeat.
		Bignum constant(ACCUMULATOR_BASE_CONSTANT);
		Bignum accBase(1);
		for (uint32_t count = 0; count < MAX_ACCUMGEN_ATTEMPTS && accBase.isOne(); count++) {
			accBase = constant.pow_mod(Bignum(2), r_ufos[i]);
			constant++;
		}
		if (accBase.isOne()) {
			throw ZerocoinException("failed to calculate accumulator base (max attempts)!");
		}
		params.accumulatorParams.accumulatorBases.push_back(accBase);
	}

	// Compute the accumulator range. The upper range is the largest possible coin commitment value.
	// The lower range is sqrt(upper range) + 1. Since OpenSSL doesn't have
	// a square root function we use a slightly higher approximation.
	params.accumulatorParams.maxCoinValue = params.coinCommitmentGroup.modulus;
	params.accumulatorParams.minCoinValue = Bignum(2).pow((params.coinCommitmentGroup.modulus.bitSize() / 2) + 3);

	// If all went well, mark params as successfully initialized.
	params.accumulatorParams.initialized = true;

	// If all went well, mark params as successfully initialized.
	params.initialized = true;
}

/// \brief Format a seed string by hashing several values.
/// TODO documentation
/// \throws         bignum_error and whatever CHashWriter throws?
///
/// Returns the hash of the value.

uint256
calculateGeneratorSeed(Bignum serialNumber, string label, uint32_t index, uint32_t count)
{
	CHashWriter hasher(0,0);

	// Compute the hash of:
	// <serialNumber>||<label>||<index>||<count>
	hasher << serialNumber;
	hasher << string("||");
	hasher << label;
	hasher << string("||");
	hasher << index;
	hasher << string("||");
	hasher << count;

	return hasher.GetHash();
}

/// \brief Format a seed string by hashing several values.
/// TODO documentation
/// \throws         bignum_error and whatever CHashWriter throws?
///
/// Returns the hash of the value.

uint256
calculateGeneratorSeed(uint256 seed, uint256 pSeed, uint256 qSeed, string label, uint32_t index, uint32_t count)
{
	CHashWriter hasher(0,0);

	// Compute the hash of:
	// <seed>||<pSeed>||<qSeed>||<label>||<index>||<count>
	hasher << seed;
	hasher << string("||");
	hasher << pSeed;
	hasher << string("||");
	hasher << qSeed;
	hasher << string("||");
	hasher << label;
	hasher << string("||");
	hasher << index;
	hasher << string("||");
	hasher << count;

	return hasher.GetHash();
}

/// \brief Format a seed string by hashing several values.
/// \param N                A Bignum
/// \param aux              An auxiliary string
/// \param securityLevel    The security level in bits
/// \param groupName        A group description string
/// \throws         bignum_error and whatever CHashWriter throws? TODO
///
/// Returns the hash of the value.

uint256
calculateSeed(Bignum modulus, string auxString, uint32_t securityLevel, string groupName)
{
	CHashWriter hasher(0,0);

	// Compute the hash of:
	// <modulus>||<securitylevel>||<auxString>||groupName
	hasher << modulus;
	hasher << string("||");
	hasher << securityLevel;
	hasher << string("||");
	hasher << auxString;
	hasher << string("||");
	hasher << groupName;

	return hasher.GetHash();
}

uint256
calculateHash(uint256 input)
{
	CHashWriter hasher(0,0);

	// Compute the hash of "input"
	hasher << input;

	return hasher.GetHash();
}

/// \brief Calculate field/group parameter sizes based on a security level.
/// \param maxPLen          Maximum size of the field (modulus "p") in bits.
/// \param securityLevel    Required security level in bits (at least 80)
/// \param pLen             Result: length of "p" in bits
/// \param qLen             Result: length of "q" in bits
/// \throws                 ZerocoinException if the process fails
///
/// Calculates the appropriate sizes of "p" and "q" for a prime-order
/// subgroup of order "q" embedded within a field "F_p". The sizes
/// are based on a 'securityLevel' provided in symmetric-equivalent
/// bits. Our choices slightly exceed the specs in FIPS 186-3:
///
/// securityLevel = 80:     pLen = 1024, qLen = 256
/// securityLevel = 112:    pLen = 2048, qLen = 256
/// securityLevel = 128:    qLen = 3072, qLen = 320
///
/// If the length of "p" exceeds the length provided in "maxPLen", or
/// if "securityLevel < 80" this routine throws an exception.

// GNOSIS: now that we have RSA UFOs, this function is not important.

void
calculateGroupParamLengths(uint32_t maxPLen, uint32_t securityLevel,
                           uint32_t *pLen, uint32_t *qLen)
{
	*pLen = *qLen = 0;

	if (securityLevel < 80) {
		throw ZerocoinException("Security level must be at least 80 bits.");
	} else if (securityLevel == 80) {
		*qLen = 256;
		*pLen = 1024;
	} else if (securityLevel <= 112) {
		*qLen = 256;
		*pLen = 2048;
	} else if (securityLevel <= 128) {
		*qLen = 320;
		*pLen = 3072;
	} else {
		throw ZerocoinException("Security level not supported.");
	}

	if (*pLen > maxPLen) {
		throw ZerocoinException("Modulus size is too small for this security level.");
	}
}

/// \brief Deterministically compute a set of group parameters using NIST procedures.
/// \param seedStr  A byte string seeding the process.
/// \param pLen     The desired length of the modulus "p" in bits
/// \param qLen     The desired length of the order "q" in bits
/// \return         An IntegerGroupParams object
///
/// Calculates the description of a group G of prime order "q" embedded within
/// a field "F_p". The input to this routine is an arbitrary seed. It uses the
/// algorithms described in FIPS 186-3 Appendix A.1.2 to calculate
/// primes "p" and "q". It uses the procedure in Appendix A.2.3 to
/// derive two generators "g", "h".

IntegerGroupParams
deriveIntegerGroupParams(uint256 seed, uint32_t pLen, uint32_t qLen)
{
	IntegerGroupParams result;
	Bignum p;
	Bignum q;
	uint256 pSeed, qSeed;

	// Calculate "p" and "q" and "domain_parameter_seed" from the
	// "seed" buffer above, using the procedure described in NIST
	// FIPS 186-3, Appendix A.1.2.
	calculateGroupModulusAndOrder(seed, pLen, qLen, &(result.modulus),
	                              &(result.groupOrder), &pSeed, &qSeed);

	// Calculate the generators "g", "h" using the process described in
	// NIST FIPS 186-3, Appendix A.2.3. This algorithm takes ("p", "q",
	// "domain_parameter_seed", "index"). We use "index" value 1
	// to generate "g" and "index" value 2 to generate "h".
	result.g(calculateGroupGenerator(Bignum(0), seed, pSeed, qSeed, result.modulus, result.groupOrder, 1));
	result.h(calculateGroupGenerator(Bignum(0), seed, pSeed, qSeed, result.modulus, result.groupOrder, 2));

	// Perform some basic tests to make sure we have good parameters
	if ((uint32_t)(result.modulus.bitSize()) < pLen ||          // modulus is pLen bits long
	        (uint32_t)(result.groupOrder.bitSize()) < qLen ||       // order is qLen bits long
	        !(result.modulus.isPrime()) ||                          // modulus is prime
	        !(result.groupOrder.isPrime()) ||                       // order is prime
	        !((result.g().pow_mod(result.groupOrder, result.modulus)).isOne()) || // g^order mod modulus = 1
	        !((result.h().pow_mod(result.groupOrder, result.modulus)).isOne()) || // h^order mod modulus = 1
	        ((result.g().pow_mod(Bignum(100), result.modulus)).isOne()) ||        // g^100 mod modulus != 1
	        ((result.h().pow_mod(Bignum(100), result.modulus)).isOne()) ||        // h^100 mod modulus != 1
	        result.g() == result.h() ||                                 // g != h
	        result.g().isOne()) {                                      // g != 1
		// If any of the above tests fail, throw an exception
		throw ZerocoinException("Group parameters are not valid");
	}

	return result;
}

/// \brief Deterministically compute a  set of group parameters with a specified order.
/// \param groupOrder   The order of the group
/// \return         An IntegerGroupParams object
///
/// Given "q" calculates the description of a group G of prime order "q" embedded within
/// a field "F_p".

IntegerGroupParams
deriveIntegerGroupFromOrder(Bignum &groupOrder)
{
	IntegerGroupParams result;

	// Set the order to "groupOrder"
	result.groupOrder = groupOrder;

	// Try possible values for "modulus" of the form "groupOrder * 2 * i" where
	// "p" is prime and i is a counter starting at 1.
	for (uint32_t i = 1; i < NUM_SCHNORRGEN_ATTEMPTS; i++) {
		// Set modulus equal to "groupOrder * 2 * i"
		result.modulus = (result.groupOrder * Bignum(i*2)) + Bignum(1);

		// Test the result for primality
		// TODO: This is a probabilistic routine and thus not the right choice
		if (result.modulus.isPrime(256)) {

			// Success.
			//
			// Calculate the generators "g", "h" using the process described in
			// NIST FIPS 186-3, Appendix A.2.3. This algorithm takes ("p", "q",
			// "domain_parameter_seed", "index"). We use "index" value 1
			// to generate "g" and "index" value 2 to generate "h".
			uint256 seed = calculateSeed(groupOrder, "", 128, "");
			uint256 pSeed = calculateHash(seed);
			uint256 qSeed = calculateHash(pSeed);
			result.g(calculateGroupGenerator(Bignum(0), seed, pSeed, qSeed, result.modulus, result.groupOrder, 1));
			result.h(calculateGroupGenerator(Bignum(0), seed, pSeed, qSeed, result.modulus, result.groupOrder, 2));

			// Perform some basic tests to make sure we have good parameters
			if (!(result.modulus.isPrime()) ||                          // modulus is prime
			        !(result.groupOrder.isPrime()) ||                       // order is prime
			        !((result.g().pow_mod(result.groupOrder, result.modulus)).isOne()) || // g^order mod modulus = 1
			        !((result.h().pow_mod(result.groupOrder, result.modulus)).isOne()) || // h^order mod modulus = 1
			        ((result.g().pow_mod(Bignum(100), result.modulus)).isOne()) ||        // g^100 mod modulus != 1
			        ((result.h().pow_mod(Bignum(100), result.modulus)).isOne()) ||        // h^100 mod modulus != 1
			        result.g() == result.h() ||                                 // g != h
			        result.g().isOne()) {                                       // g != 1
				// If any of the above tests fail, throw an exception
				throw ZerocoinException("Group parameters are not valid");
			}

			return result;
		}
	}

	// If we reached this point group generation has failed. Throw an exception.
	throw ZerocoinException("Too many attempts to generate Schnorr group.");
}

/// \brief Deterministically compute a group description using NIST procedures.
/// \param seed                         A byte string seeding the process.
/// \param pLen                         The desired length of the modulus "p" in bits
/// \param qLen                         The desired length of the order "q" in bits
/// \param resultModulus                A value "p" describing a finite field "F_p"
/// \param resultGroupOrder             A value "q" describing the order of a subgroup
/// \param resultDomainParameterSeed    A resulting seed for use in later calculations.
///
/// Calculates the description of a group G of prime order "q" embedded within
/// a field "F_p". The input to this routine is in arbitrary seed. It uses the
/// algorithms described in FIPS 186-3 Appendix A.1.2 to calculate
/// primes "p" and "q".

void
calculateGroupModulusAndOrder(uint256 seed, uint32_t pLen, uint32_t qLen,
                              Bignum *resultModulus, Bignum *resultGroupOrder,
                              uint256 *resultPseed, uint256 *resultQseed)
{
	// Verify that the seed length is >= qLen
	if (qLen > (sizeof(seed)) * 8) {
		// TODO: The use of 256-bit seeds limits us to 256-bit group orders. We should probably change this.
		// throw ZerocoinException("Seed is too short to support the required security level.");
	}

#ifdef ZEROCOIN_DEBUG
	std::cout << "calculateGroupModulusAndOrder: pLen = " << pLen << std::endl;
#endif

	// Generate a random prime for the group order.
	// This may throw an exception, which we'll pass upwards.
	// Result is the value "resultGroupOrder", "qseed" and "qgen_counter".
	uint256     qseed;
	uint32_t    qgen_counter;
	*resultGroupOrder = generateRandomPrime(qLen, seed, &qseed, &qgen_counter);

	// Using ⎡pLen / 2 + 1⎤ as the length and qseed as the input_seed, use the random prime
	// routine to obtain p0 , pseed, and pgen_counter. We pass exceptions upward.
	uint32_t    p0len = ceil((pLen / 2.0) + 1);
	uint256     pseed;
	uint32_t    pgen_counter;
	Bignum p0 = generateRandomPrime(p0len, qseed, &pseed, &pgen_counter);

	// Set x = 0, old_counter = pgen_counter
	uint32_t    old_counter = pgen_counter;

	// Generate a random integer "x" of pLen bits
	uint32_t iterations;
	Bignum x = generateIntegerFromSeed(pLen, pseed, &iterations);
	pseed += (iterations + 1);

	// Set x = 2^{pLen−1} + (x mod 2^{pLen–1}).
	Bignum powerOfTwo = Bignum(2).pow(pLen-1);
	x = powerOfTwo + (x % powerOfTwo);

	// t = ⎡x / (2 * resultGroupOrder * p0)⎤.
	// TODO: we don't have a ceiling function
	Bignum t = x / (Bignum(2) * (*resultGroupOrder) * p0);

	// Now loop until we find a valid prime "p" or we fail due to
	// pgen_counter exceeding ((4*pLen) + old_counter).
	for ( ; pgen_counter <= ((4*pLen) + old_counter) ; pgen_counter++) {
		// If (2 * t * resultGroupOrder * p0 + 1) > 2^{pLen}, then
		// t = ⎡2^{pLen−1} / (2 * resultGroupOrder * p0)⎤.
		powerOfTwo = Bignum(2).pow(pLen);
		Bignum prod = (Bignum(2) * t * (*resultGroupOrder) * p0) + Bignum(1);
		if (prod > powerOfTwo) {
			// TODO: implement a ceil function
			t = Bignum(2).pow(pLen-1) / (Bignum(2) * (*resultGroupOrder) * p0);
		}

		// Compute a candidate prime resultModulus = 2tqp0 + 1.
		*resultModulus = (Bignum(2) * t * (*resultGroupOrder) * p0) + Bignum(1);

		// Verify that resultModulus is prime. First generate a pseudorandom integer "a".
		Bignum a = generateIntegerFromSeed(pLen, pseed, &iterations);
		pseed += iterations + 1;

		// Set a = 2 + (a mod (resultModulus–3)).
		a = Bignum(2) + (a % ((*resultModulus) - Bignum(3)));

		// Set z = a^{2 * t * resultGroupOrder} mod resultModulus
		Bignum z = a.pow_mod(Bignum(2) * t * (*resultGroupOrder), (*resultModulus));

		// If GCD(z–1, resultModulus) == 1 AND (z^{p0} mod resultModulus == 1)
		// then we have found our result. Return.
		if ((resultModulus->gcd(z - Bignum(1))).isOne() &&
		        (z.pow_mod(p0, (*resultModulus))).isOne()) {
			// Success! Return the seeds and primes.
			*resultPseed = pseed;
			*resultQseed = qseed;
			return;
		}

		// This prime did not work out. Increment "t" and try again.
		t = t + Bignum(1);
	} // loop continues until pgen_counter exceeds a limit

	// We reach this point only if we exceeded our maximum iteration count.
	// Throw an exception.
	throw ZerocoinException("Unable to generate a prime modulus for the group");
}

/// \brief Deterministically derives coin commitment group generators g & h from a serial number (and group modulus and order).
/// \param serialNumber                 Serial number of the ZC spend.
/// \param modulus                      Prime modulus for the field.
/// \param groupOrder                   Order of the group.
/// \param g_out						Out param for g generator.
/// \param h_out						Out param for h generator.
/// \throws                             A ZerocoinException if error.
///
/// The purpose of having different generators for each ZC spend is to prevent
/// one solution of the discrete log problem from allowing infinite double spends.
/// See "Rational Zero" by Garman et al., section 4.4 for more.
///
/// Unlike the other functions in this file, this is called after initial setup
/// of Zerocoin parameters (i.e., it is called during minting, spending, and verifying).
void
deriveGeneratorsFromSerialNumber(Bignum serialNumber, Bignum modulus, Bignum groupOrder, Bignum& g_out, Bignum& h_out)
{
	Bignum g, h;
	g = calculateGroupGenerator(serialNumber, 0, 0, 0, modulus, groupOrder, 1);
	h = calculateGroupGenerator(serialNumber, 0, 0, 0, modulus, groupOrder, 2);
	if (g == h) {
		throw ZerocoinException("g == h for coin commitment group generators derived from serial number");
	}
	g_out = g;
	h_out = h;
}

/// \brief Deterministically compute a generator for a given group.
/// \param serialNumber                 For coin commitment group. *seed params used iff zero.
/// \param seed                         A first seed for the process.
/// \param pSeed                        A second seed for the process.
/// \param qSeed                        A third seed for the process.
/// \param modulus                      Proposed prime modulus for the field.
/// \param groupOrder                   Proposed order of the group.
/// \param index                        Index value, selects which generator you're building.
/// \return                             The resulting generator.
/// \throws                             A ZerocoinException if error.
///
/// Generates a random group generator deterministically as a function of either (serialNumber) or (seed,pSeed,qSeed)
/// Uses the algorithm described in FIPS 186-3 Appendix A.2.3.

Bignum
calculateGroupGenerator(Bignum serialNumber, uint256 seed, uint256 pSeed, uint256 qSeed, Bignum modulus, Bignum groupOrder, uint32_t index)
{
	Bignum result;

	// Verify that 0 <= index < 256
	if (index > 255) {
		throw ZerocoinException("Invalid index for group generation");
	}

	// Compute e = (modulus - 1) / groupOrder
	Bignum e = (modulus - Bignum(1)) / groupOrder;

	// Loop until we find a generator
	for (uint32_t count = 1; count < MAX_GENERATOR_ATTEMPTS; count++) {
		// hash = Hash(seed || pSeed || qSeed || “ggen” || index || count
		uint256 hash = (serialNumber > 0) ? calculateGeneratorSeed(serialNumber, "ggen", index, count)
		                                  : calculateGeneratorSeed(seed, pSeed, qSeed, "ggen", index, count);
		Bignum W(hash);

		// Compute result = W^e mod p
		result = W.pow_mod(e, modulus);

		// If result > 1, we have a generator
		if (result > 1) {
			return result;
		}
	}

	// We only get here if we failed to find a generator
	throw ZerocoinException("Unable to find a generator, too many attempts");
}

/// \brief Deterministically compute a random prime number.
/// \param primeBitLen                  Desired bit length of the prime.
/// \param in_seed                      Input seed for the process.
/// \param out_seed                     Result: output seed from the process.
/// \param prime_gen_counter            Result: number of iterations required.
/// \return                             The resulting prime number.
/// \throws                             A ZerocoinException if error.
///
/// Generates a random prime number of primeBitLen bits from a given input
/// seed. Uses the Shawe-Taylor algorithm as described in FIPS 186-3
/// Appendix C.6. This is a recursive function.

Bignum
generateRandomPrime(uint32_t primeBitLen, uint256 in_seed, uint256 *out_seed,
                    uint32_t *prime_gen_counter)
{
	// Verify that primeBitLen is not too small
	if (primeBitLen < 2) {
		throw ZerocoinException("Prime length is too short");
	}

	// If primeBitLen < 33 bits, perform the base case.
	if (primeBitLen < 33) {
		Bignum result(0);

		// Set prime_seed = in_seed, prime_gen_counter = 0.
		uint256     prime_seed = in_seed;
		(*prime_gen_counter) = 0;

		// Loop up to "4 * primeBitLen" iterations.
		while ((*prime_gen_counter) < (4 * primeBitLen)) {

			// Generate a pseudorandom integer "c" of length primeBitLength bits
			uint32_t iteration_count;
			Bignum c = generateIntegerFromSeed(primeBitLen, prime_seed, &iteration_count);
#ifdef ZEROCOIN_DEBUG
			std::cout << "generateRandomPrime: primeBitLen = " << primeBitLen << std::endl;
			std::cout << "Generated c = " << c << std::endl;
#endif

			prime_seed += (iteration_count + 1);
			(*prime_gen_counter)++;

			// Set "intc" to be the least odd integer >= "c" we just generated
			uint32_t intc = c.getulong();
			intc = (2 * floor(intc / 2.0)) + 1;
#ifdef ZEROCOIN_DEBUG
			std::cout << "Should be odd. c = " << intc << std::endl;
			std::cout << "The big num is: c = " << c << std::endl;
#endif

			// Perform trial division on this (relatively small) integer to determine if "intc"
			// is prime. If so, return success.
			if (primalityTestByTrialDivision(intc)) {
				// Return "intc" converted back into a Bignum and "prime_seed". We also updated
				// the variable "prime_gen_counter" in previous statements.
				result = intc;
				*out_seed = prime_seed;

				// Success
				return result;
			}
		} // while()

		// If we reached this point there was an error finding a candidate prime
		// so throw an exception.
		throw ZerocoinException("Unable to find prime in Shawe-Taylor algorithm");

		// END OF BASE CASE
	}
	// If primeBitLen >= 33 bits, perform the recursive case.
	else {
		// Recurse to find a new random prime of roughly half the size
		uint32_t newLength = ceil((double)primeBitLen / 2.0) + 1;
		Bignum c0 = generateRandomPrime(newLength, in_seed, out_seed, prime_gen_counter);

		// Generate a random integer "x" of primeBitLen bits using the output
		// of the previous call.
		uint32_t numIterations;
		Bignum x = generateIntegerFromSeed(primeBitLen, *out_seed, &numIterations);
		(*out_seed) += numIterations + 1;

		// Compute "t" = ⎡x / (2 * c0⎤
		// TODO no Ceiling call
		Bignum t = x / (Bignum(2) * c0);

		// Repeat the following procedure until we find a prime (or time out)
		for (uint32_t testNum = 0; testNum < MAX_PRIMEGEN_ATTEMPTS; testNum++) {

			// If ((2 * t * c0) + 1 > 2^{primeBitLen}),
			// then t = ⎡2^{primeBitLen} – 1 / (2 * c0)⎤.
			if ((Bignum(2) * t * c0) > (Bignum(2).pow(Bignum(primeBitLen)))) {
				t = ((Bignum(2).pow(Bignum(primeBitLen))) - Bignum(1)) / (Bignum(2) * c0);
			}

			// Set c = (2 * t * c0) + 1
			Bignum c = (Bignum(2) * t * c0) + Bignum(1);

			// Increment prime_gen_counter
			(*prime_gen_counter)++;

			// Test "c" for primality as follows:
			// 1. First pick an integer "a" in between 2 and (c - 2)
			Bignum a = generateIntegerFromSeed(c.bitSize(), (*out_seed), &numIterations);
			a = Bignum(2) + (a % (c - Bignum(3)));
			(*out_seed) += (numIterations + 1);

			// 2. Compute "z" = a^{2*t} mod c
			Bignum z = a.pow_mod(Bignum(2) * t, c);

			// 3. Check if "c" is prime.
			//    Specifically, verify that gcd((z-1), c) == 1 AND (z^c0 mod c) == 1
			// If so we return "c" as our result.
			if (c.gcd(z - Bignum(1)).isOne() && z.pow_mod(c0, c).isOne()) {
				// Return "c", out_seed and prime_gen_counter
				// (the latter two of which were already updated)
				return c;
			}

			// 4. If the test did not succeed, increment "t" and loop
			t = t + Bignum(1);
		} // end of test loop
	}

	// We only reach this point if the test loop has iterated MAX_PRIMEGEN_ATTEMPTS
	// and failed to identify a valid prime. Throw an exception.
	throw ZerocoinException("Unable to generate random prime (too many tests)");
}

Bignum
generateIntegerFromSeed(uint32_t numBits, uint256 seed, uint32_t *numIterations)
{
	Bignum      result(0);
	uint32_t    iterations = ceil((double)numBits / (double)HASH_OUTPUT_BITS);

#ifdef ZEROCOIN_DEBUG
	std::cout << "numBits = " << numBits << std::endl;
	std::cout << "iterations = " << iterations << std::endl;
#endif

	// Loop "iterations" times filling up the value "result" with random bits
	for (uint32_t count = 0; count < iterations; count++) {
		// result += ( H(pseed + count) * 2^{count * p0len} )
		result += Bignum(calculateHash(seed + count)) * Bignum(2).pow(count * HASH_OUTPUT_BITS);
	}

	result = Bignum(2).pow(numBits - 1) + (result % (Bignum(2).pow(numBits - 1)));

	// Return the number of iterations and the result
	*numIterations = iterations;
	return result;
}

/// \brief Determines whether a uint32_t is a prime through trial division.
/// \param candidate       Candidate to test.
/// \return                true if the value is prime, false otherwise
///
/// Performs trial division to determine whether a uint32_t is prime.

bool
primalityTestByTrialDivision(uint32_t candidate)
{
	// TODO: HACK HACK WRONG WRONG
	Bignum canBignum(candidate);

	return canBignum.isPrime();
}

/// \brief Deterministically calculates a "raw" UFO by concatenating the bits of SHA-256 hashes.
/// \param ufoIndex        The index of this UFO. Start at 0.
/// \param numBits         Number of bits of SHA-256 data to use.
/// \return                The "raw" UFO, meaning small factors have not been removed.
///
/// Using only one of these UFOs is insecure, since there is a non-negligible
/// probability that it can be factored. To use securely, about 13 ~3800-bit
/// UFOs are required, after filtering out those that can be completely
/// factorized, as well as those that can be significantly reduced by removing
/// small factors (a threshold number of bits should be chosen at the
/// beginning; if the product of all small factors has a log_2 greater than
/// this threshold, the candidate should be rejected).
///
/// This relies on HASH_OUTPUT_BITS matching the bit length from CHashWriter.

Bignum
calculateRawUFO(uint32_t ufoIndex, uint32_t numBits) {
	Bignum result(0);
	uint32_t hashes = numBits / HASH_OUTPUT_BITS;

	if (numBits != HASH_OUTPUT_BITS * hashes) {
		throw ZerocoinException("numBits must be divisible by HASH_OUTPUT_BITS");		// not implemented
	}

	for (uint32_t i = 0; i < hashes; i++) {
		CHashWriter hasher(0,0);
		hasher << ufoIndex;
		hasher << string("||");
		hasher << numBits;
		hasher << string("||");
		hasher << i;
		uint256 hash = hasher.GetHash();
		result <<= HASH_OUTPUT_BITS;
		result += Bignum(hash);
	}

	return result;
}

/// \throws         ZerocoinException if the process fails
void
calculateUFOs(AccumulatorAndProofParams& out_accParams)
{
	//TODO: refactor this and chain of callers to allow supplying of new factors from outside libzerocoin
	vector<Bignum> f_ufos;

	// These are the products of the known RSA UFO factors. The factors
	// themselves can be recovered in minutes using the msieve program.
	
	Bignum tmp;

	tmp.SetHex("138b1bb66beb5");
	f_ufos.push_back(tmp); // ufoIndex 0
	tmp.SetHex("705b7363063e8ec4304d7c93c60685");
	f_ufos.push_back(tmp); // ufoIndex 1
	tmp.SetHex("1161f7fee1c13ef659dec7078ad");
	f_ufos.push_back(tmp); // ufoIndex 2
	tmp.SetHex("21dccb848ed9a2d191fd48a2766509f852e6f54fd");
	f_ufos.push_back(tmp); // ufoIndex 3
	tmp.SetHex("b1acbba887");
	f_ufos.push_back(tmp); // ufoIndex 4
	tmp.SetHex("65bd9c8b14ab1b5032");
	f_ufos.push_back(tmp); // ufoIndex 5
	tmp.SetHex("2e29e0c390");
	f_ufos.push_back(tmp); // ufoIndex 6
	tmp.SetHex("12a672f13277467c915630167075b56a420430cae6a");
	f_ufos.push_back(tmp); // ufoIndex 7
	tmp.SetHex("f0cfe54b22265234827f0397e9d88f2a2b8ca2be5cec7c51d4");
	f_ufos.push_back(tmp); // ufoIndex 8
	tmp.SetHex("9bc2b1d6970d5726e9e930ad7b62fb9bfce5149fde6");
	f_ufos.push_back(tmp); // ufoIndex 9
	tmp.SetHex("91b4b08ddf306ff2fce4");
	f_ufos.push_back(tmp); // ufoIndex 10
	tmp.SetHex("4cf7b332c2a4edbbc4617e9c20ce47859674ab4727e386059ee03ac");
	f_ufos.push_back(tmp); // ufoIndex 11
	tmp.SetHex("b37af95a3b722da08af71fc3c6725d064");
	f_ufos.push_back(tmp); // ufoIndex 12
	tmp.SetHex("28aac40f4cbd78ff9372718d4e12eecb4b543284744be3afa31d63fc55a47bf9d2aba2362582963e7c3ce8c0e06fc9f2c7b82e992b37be83e52be6ab6afe71");
	f_ufos.push_back(tmp); // ufoIndex 13
	tmp.SetHex("131411f01b");
	f_ufos.push_back(tmp); // ufoIndex 14
	tmp.SetHex("c29236dfc030d03e4d50a4116994213dab7b5e1cf27d1cfb43e35893ca3e6379444f99bee0720d928ac");
	f_ufos.push_back(tmp); // ufoIndex 15

	//out_accParams.accumulatorModuli
	for (unsigned int ufoIndex = 0; out_accParams.accumulatorModuli.size() < UFO_COUNT; ufoIndex++) {
		// divide out the factors
		// throw ZerocoinException if f_ufos too small
		// throw ZerocoinException if not evenly divisible
		if (f_ufos.size() - 1 < (unsigned long)ufoIndex) {
			throw ZerocoinException("factor product not found");
		}

		Bignum u = calculateRawUFO(ufoIndex, UFO_INITIAL_BIT_LENGTH);
		if (u % f_ufos[ufoIndex] != 0) {
			throw ZerocoinException("FATAL: factor product is not divisible into raw UFO!!!");
		}

		u /= f_ufos[ufoIndex];

		// push into accModuli
		if (!u.isPrime() && u.bitSize() >= UFO_MIN_BIT_LENGTH) {
			out_accParams.accumulatorModuli.push_back(u);
		}
	}
}

} // namespace libzerocoin
