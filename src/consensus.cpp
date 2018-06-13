#include "consensus.h"

#include "block.h"
#include "chain.h"
#include "uint256.h"

namespace CashIsKing
{

bool ANCConsensus::CheckProofOfWork(const uint256& hash, unsigned int nBits)
{}

uint256 ANCConsensus::GetPoWHashForNextBlock()
{
  CBlockIndex *tip = chainActive.Tip();
  if (tip)
  {
    int32_t nHeight = tip->nHeight;
  } else {
    //TODO: FIX
    return uint256(0);
  }
}

/**
 * 
 * This function always return scrypt pow, until nDifficultySwitchHeight6
 * where GOST3411 take over.
 * 
 * */
uint256 ANCConsensus::GetPoWHashForThisBlock(const CBlockHeader& block)
{
  CBlockIndex *tip = chainActive.Tip();
  if (tip)
  {
    int32_t nHeight = tip->nHeight;
    if (nHeight >= nDifficultySwitchHeight6)
    {
      return block.GetGost3411Hash();
    } else {
      return block.GetHash();
    }
  } else {
    return block.GetHash();
  }
}

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
