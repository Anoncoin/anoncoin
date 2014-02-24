// Copyright (c) 2014 The Anoncoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zc_ufo.h"
#include "zc_ufo_pre.gen.h"  // auto-generated

using namespace std;

// Generate a vector of RSA UFOs to use for Zerocoin accumulator moduli.
// Throws runtime_error if dividing out the small factors fails (meaning the
// UFO precursor numbers are incorrect for some reason).
void GenerateUFOs(vector<Bignum>& vN_out) {
    Bignum smallFactors;
    /********* UFO 1 *********/
    Bignum N1;
    N1.SetHex(string(UFO_PRECURSOR_1_TO_40));

    // divide out small factors: 9*353*2975341*12681178183
    smallFactors.SetHex(string("67f8aca5bf3c7e033"));
    if (N1 % smallFactors != 0) {
        throw runtime_error("Failed to divide out small factors from UFO 1");
    }
    N1 /= smallFactors;
    vN_out.push_back(N1);

    /********* UFO 2 *********/
    Bignum N2;
    N2.SetHex(string(UFO_PRECURSOR_41_TO_80));

    // divide out small factors: 2*2*3*3*3*11*7920443*140265994384656263345672113
    // the largest of these small factors was found by ECM factorization
    smallFactors.SetHex(string("fe30b09c5a20e0f945e0f07794ce0c"));
    if (N2 % smallFactors != 0) {
        throw runtime_error("Failed to divide out small factors from UFO 2");
    }
    N2 /= smallFactors;
    vN_out.push_back(N2);
}
