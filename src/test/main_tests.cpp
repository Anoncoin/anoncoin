// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "main.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(main_tests)
/** \brief
    Running test...  With Subsidy printing enabled...
    Block 77778 Subsidy=1000000000 Sum=42685400000000
    Block 306601 Subsidy=250000000 Sum=157096150000000
    Block 306602 Subsidy=250000000 Sum=157346150000000
    Block 613602 Subsidy=125000000 Sum=233971150000000
    Block 920602 Subsidy=62500000 Sum=272283650000000
    Block 1226602 Subsidy=31250000 Sum=291377400000000
    Block 1533602 Subsidy=15625000 Sum=300955525000000
    Block 1839602 Subsidy=7812500 Sum=305728962500000
    Block 2146602 Subsidy=3906250 Sum=308123493750000
    Block 2453602 Subsidy=1953125 Sum=309320759375000
    Block 2759602 Subsidy=976562 Sum=309917439062000
    Block 3066602 Subsidy=488281 Sum=310216755315000
    Block 3372602 Subsidy=244140 Sum=310365925160000
    Block 3679602 Subsidy=122070 Sum=310440754070000
    Block 3986602 Subsidy=61035 Sum=310478168525000
    Block 4292602 Subsidy=30517 Sum=310496814717000
    Block 4599602 Subsidy=15258 Sum=310506168177000
    Block 4905602 Subsidy=7629 Sum=310510829496000
    Block 5212602 Subsidy=3814 Sum=310513167784000
    Block 5519602 Subsidy=1907 Sum=310514336775000
    Block 5825602 Subsidy=953 Sum=310514919363000
    Block 6132602 Subsidy=476 Sum=310515211457000
    Block 6438602 Subsidy=238 Sum=310515356875000
    Block 6745602 Subsidy=119 Sum=310515429822000
    Block 7052602 Subsidy=59 Sum=310515466295000
    Block 7358602 Subsidy=29 Sum=310515484319000
    Block 7665602 Subsidy=14 Sum=310515493207000
    Block 7971602 Subsidy=7 Sum=310515497484000
    Block 8278602 Subsidy=3 Sum=310515499629000
    Block 8585602 Subsidy=1 Sum=310515500548000
    Block 8891602 Subsidy=0 Sum=310515500853000
    Total Subsidy Sum=310515500853000
 *
 */

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    uint64_t nSum = 0;
    uint64_t nSubsidy;
    uint64_t nSubsidy_last;
    int nHeight;
    for (nHeight = 0; nHeight < 77779; nHeight++) {
        nSubsidy = GetBlockValue(nHeight, 0);
        nSum += nSubsidy;
    }
    // printf( "Block %llu Subsidy=%llu Sum=%llu\n", nHeight-1, nSubsidy, nSum);
    nSubsidy_last = nSubsidy = GetBlockValue(nHeight, 0);
    while( nSubsidy_last == nSubsidy ) {
        nSubsidy = GetBlockValue(nHeight++, 0);
        nSum += nSubsidy;
    }
    // printf( "Block %llu Subsidy=%llu Sum=%llu\n", nHeight++, nSubsidy, nSum);
    // before 60yrs worth of blocks the subsidy should go to zero
    for( ; nHeight < 3066000*3 + 1; nHeight += 1000) {
        uint64_t nSubsidy = GetBlockValue(nHeight, 0);
        nSum += nSubsidy * 1000;
        BOOST_CHECK(MoneyRange(nSum));
        if( nSubsidy_last != nSubsidy ) {
            // printf( "Block %llu Subsidy=%llu Sum=%llu\n", nHeight, nSubsidy, nSum);
            nSubsidy_last = nSubsidy;
            if( !nSubsidy )
                break;
        }
    }
    BOOST_CHECK(nSum == 310515500853000LL);
    // printf( "Total Subsidy Sum=%llu\n", nSum);
}

BOOST_AUTO_TEST_SUITE_END()
