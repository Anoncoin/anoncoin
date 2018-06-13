#ifndef __CONSENSUS_H__
#define __CONSENSUS_H__

#include <stdint.h>


// TODO: Remove definitions once we're sure we don't need them.

#define HARDFORK_BLOCK 555555 //! CSlave: if not hardcoded, the hardfork block can be defined with "configure --with-hardfork=block"
#define HARDFORK_BLOCK2 585555 // block to change the parameters of the PID

#ifndef HARDFORK_BLOCK3
#define HARDFORK_BLOCK3 900000
#endif

class uint256;
class CBlockIndex;
class CBlockHeader;

namespace CashIsKing
{

class ANCConsensus
{
private:

public:
  uint256 GetPoWHashForNextBlock();
  uint256 GetPoWHashForThisBlock(const CBlockHeader& block);
  uint256 GetBlockProof(const ::CBlockIndex& block);
  uint256 GetWorkProof(const uint256& uintTarget);
  bool CheckProofOfWork(const uint256& hash, unsigned int nBits);

  static const int32_t nDifficultySwitchHeight1;  // Protocol 1 happened here
  static const int32_t nDifficultySwitchHeight2;  // Protocol 2 starts at this block
  static const int32_t nDifficultySwitchHeight3;  // Protocol 3 began the KGW era
  static const int32_t nDifficultySwitchHeight4;
  static const int32_t nDifficultySwitchHeight5;
  static const int32_t nDifficultySwitchHeight6;
};

}

using namespace CashIsKing;
ANCConsensus ancConsensus;


#endif
