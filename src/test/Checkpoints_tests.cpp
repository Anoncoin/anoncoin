// Copyright (c) 2011-2013 The Bitcoin Core developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for block-chain checkpoints
//

#include "checkpoints.h"

#include "block.h"              // For the uintFakeHash class
#include "uint256.h"

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 p16000 = uint256("0x683517a8cae8530f39e636f010ecd1750665c3d91f57ba71d6556535972ab328");
    uint256 p210000 = uint256("0xb9c2c5030199a87cb99255551b7f67bff6adf3ef2caf97258b93b417d14f9051");
    uint256 p314123 = uint256("0x45236d336950cb1ce679b88d03ed6ceb26cbcdd85f0e3ac4c98e7e005962f65c");

    BOOST_CHECK(Checkpoints::CheckBlock(16000, p16000));
    BOOST_CHECK(Checkpoints::CheckBlock(210000, p210000));
    BOOST_CHECK(Checkpoints::CheckBlock(314123, p314123));


    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckBlock(16000, p210000));
    BOOST_CHECK(!Checkpoints::CheckBlock(210000, p16000));
    BOOST_CHECK(!Checkpoints::CheckBlock(314123, p210000));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckBlock(16000+1, p16000));
    BOOST_CHECK(Checkpoints::CheckBlock(210000+1, p210000));
    BOOST_CHECK(Checkpoints::CheckBlock(314123+1, p314123));

    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 314123);
}

BOOST_AUTO_TEST_SUITE_END()
