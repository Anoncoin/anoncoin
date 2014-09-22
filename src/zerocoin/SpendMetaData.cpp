// SpendMetaData class for Zerocoin.
//
// Copyright 2013 Ian Miers, Christina Garman and Matthew Green
// Distributed under the MIT license.

#include "../Zerocoin.h"

namespace libzerocoin {

SpendMetaData::SpendMetaData(uint256 accumulatorId, uint256 txHash): accumulatorId(accumulatorId), txHash(txHash) {}

} /* namespace libzerocoin */
