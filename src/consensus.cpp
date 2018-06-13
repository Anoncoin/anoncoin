#include "consensus.h"

#include "uint256.h"
#include "chain.h"

namespace CashIsKing
{

bool ANCConsensus::CheckProofOfWork(const uint256& hash, unsigned int nBits)
{}

uint256 ANCConsensus::GetPoWHashForBlock(const ::CBlockIndex* pIndex)
{}


uint256 ANCConsensus::GetWorkProof(const uint256& uintTarget)
{
  // We need to compute 2**256 / (Target+1), but we can't represent 2**256
  // as it's too large for a uint256. However, as 2**256 is at least as large
  // as Target+1, it is equal to ((2**256 - Target - 1) / (Target+1)) + 1,
  // or ~Target / (Target+1) + 1.
  return !uintTarget.IsNull() ? (~uintTarget / (uintTarget + 1)) + 1 : ::uint256(0);
}

uint256 ANCConsensus::GetBlockProof(const ::CBlockIndex& block)
{
  ::uint256 bnTarget;
  bool fNegative;
  bool fOverflow;

  bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
  if (block.nHeight >= HARDFORK_BLOCK3) {
      
  }
  return (!fNegative && !fOverflow) ? ANCConsensus::GetWorkProof( bnTarget ) : ::uint256(0);
}

const int32_t ANCConsensus::nDifficultySwitchHeight1 = 15420;   // Protocol 1 happened here
const int32_t ANCConsensus::nDifficultySwitchHeight2 = 77777;  // Protocol 2 starts at this block
const int32_t ANCConsensus::nDifficultySwitchHeight3 = 87777;  // Protocol 3 began the KGW era
const int32_t ANCConsensus::nDifficultySwitchHeight4 = 555555;
const int32_t ANCConsensus::nDifficultySwitchHeight5 = 585555;
const int32_t ANCConsensus::nDifficultySwitchHeight6 = 900000;


}
