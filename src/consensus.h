#ifndef __CONSENSUS_H__
#define __CONSENSUS_H__

#include <stdint.h>


// TODO: Remove definitions once we're sure we don't need them.

#ifndef HARDFORK_BLOCK
#define HARDFORK_BLOCK 555555 //! CSlave: if not hardcoded, the hardfork block can be defined with "configure --with-hardfork=block"
#endif

#ifndef HARDFORK_BLOCK2
#define HARDFORK_BLOCK2 585555 // block to change the parameters of the PID
#endif

#ifndef HARDFORK_BLOCK3
#define HARDFORK_BLOCK3 900000
#endif


class uint256;
class CBlockIndex;
class CBlockHeader;
class CChain;
class CValidationState;

namespace CashIsKing
{

class ANCConsensus
{
private:
  void getMainnetStrategy(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader, uint256& uintResult);
  void getTestnetStrategy(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader, uint256& uintResult);

  bool SkipPoWCheck();

public:
  ANCConsensus();

  uint256 GetPoWRequiredForNextBlock();
  uint256 GetPoWHashForThisBlock(const CBlockHeader& block);
  //! Block proofs are based on the nBits field found within the block, not the actual hash
  uint256 GetBlockProof(const ::CBlockIndex& block);
  //! The primary method of providing difficulty to the user requires the use of a logarithm scale, that can not be done
  //! for 256bit numbers unless they are first converted to a work proof, then its value can be returned as a double
  //! floating point value which is meaningful.
  double GetLog2Work( const uint256& uintDifficulty );

  /** Context-dependent validity checks */
  bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const CBlockIndex* pindexPrev);
  
  bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, bool fCheckPOW = true);

  uint256 GetWorkProof(const uint256& uintTarget);
  //! Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
  bool CheckProofOfWork(const uint256& hash, unsigned int nBits);

  unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader);
  uint256 OriginalGetNextWorkRequired(const CBlockIndex* pindexLast);

  int64_t GetBlockValue(int nHeight, int64_t nFees);

  bool IsUsingGost3411Hash();

  static const int32_t nDifficultySwitchHeight1;  // Protocol 1 happened here
  static const int32_t nDifficultySwitchHeight2;  // Protocol 2 starts at this block
  static const int32_t nDifficultySwitchHeight3;  // Protocol 3 began the KGW era
  static const int32_t nDifficultySwitchHeight4;
  static const int32_t nDifficultySwitchHeight5;
  static const int32_t nDifficultySwitchHeight6;
  static const int32_t nDifficultySwitchHeight7;

#ifdef CPP11
  bool bShouldDebugLogPoW = false;
#else
  bool bShouldDebugLogPoW;
#endif
};

}

/** The currently-connected chain of blocks. */
extern CChain chainActive;


#endif
