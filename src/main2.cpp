// ToDo: Remove this file as an include in main.cpp.
//       Rename it to something better & write a header file for it.
//       Then develop the associated build script updates so it compiles & links in with everything else...
//

// #define LOG_DIFFICULTY_RETARGETING

// This value define the Anoncoin block rate production, difficulty calculations and Proof Of Work functions should use this as the goal...
static const int64_t nTargetSpacing = 180;      // in Seconds - Anoncoin target block time since KGW era, is 3 minutes * 60 secs/min
static const int64_t nTargetTimespan = 86184;   // in Seconds - Anoncoin legacy value is ~23.94hrs, it came from a
                                                // 420 blocks * 205.2 seconds/block calculation, now used in orignal
                                                // NextWorkRequired & ComputeMinWork functions.

// Difficulty Protocols have changed over time, at specific points in Anoncoin's history
// the following SwitchHeight values are those blocks where an event occured which
// required changing in the way things were calculated. aka HARDFORK
static const int nDifficultySwitchHeight = 15420;   // Protocol 1 happened here
static const int nDifficultySwitchHeight2 = 77777;  // Protocol 2 starts at this block
static const int nDifficultySwitchHeight3 = 87777;  // Protocol 3 began the KGW era
#if defined( HARDFORK_BLOCK )
static const int nDifficultySwitchHeight4 = HARDFORK_BLOCK;
#endif

#define SECONDSPERDAY (60 * 60 * 24)  // This value is used in various difficulty calculation routines, was stored, should have just been DEFINED for the compiler.

/*
 *  Difficulty formula, Anoncoin - From the early months, when blocks were very new...
 */
unsigned int static OldGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const CBigNum &bnProofOfWorkLimit)
{
    // These legacy values define the Anoncoin block rate production and are used in this difficulty calculation only...
    static const int64_t nTargetSpacingLegacy = 205;        // Originally 3.42 minutes * 60 secs was Anoncoin spacing target in seconds

    if (pindexLast == NULL)                                 // If the last block was the Genesis block, we're done
        return bnProofOfWorkLimit.GetCompact();

    // Anoncoin difficulty adjustment protocol switch (Thanks to FeatherCoin for this idea)
    static const int newTargetTimespan = 2050;              // For when another adjustment in the timespan was made
    int nHeight = pindexLast->nHeight + 1;
    bool fNewDifficultyProtocol = nHeight >= nDifficultySwitchHeight;
    bool fNewDifficultyProtocol2 = false;
    int64_t nTargetTimespanCurrent = nTargetTimespan;
    int64_t nLegacyInterval;

    if( nHeight >= nDifficultySwitchHeight2 ) {         // Jump back to sqrt(2) as the factor of adjustment.
        fNewDifficultyProtocol2 = true;
        fNewDifficultyProtocol = false;
    }

    if( fNewDifficultyProtocol ) nTargetTimespanCurrent *= 4;
    if (fNewDifficultyProtocol2) nTargetTimespanCurrent = newTargetTimespan;
    nLegacyInterval = nTargetTimespanCurrent / nTargetSpacingLegacy;

    // Only change once per interval, or at protocol switch height
    if( nHeight % nInterval != 0 && !fNewDifficultyProtocol2 && nHeight != nDifficultySwitchHeight )
        return pindexLast->nBits;

    // Anoncoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = nLegacyInterval-1;
    if ((pindexLast->nHeight+1) != nLegacyInterval)
        blockstogoback = nLegacyInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    blockstogoback = fNewDifficultyProtocol2 ? (newTargetTimespan/205) : blockstogoback;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    int64_t nActualTimespanMax = fNewDifficultyProtocol ? (nTargetTimespanCurrent*4) : ((nTargetTimespanCurrent*99)/70);
    int64_t nActualTimespanMin = fNewDifficultyProtocol ? (nTargetTimespanCurrent/4) : ((nTargetTimespanCurrent*70)/99);
    if (pindexLast->nHeight+1 >= nDifficultySwitchHeight2) {
        if (nActualTimespan < nActualTimespanMin)
            nActualTimespan = nActualTimespanMin;
        if (nActualTimespan > nActualTimespanMax)
            nActualTimespan = nActualTimespanMax;
    } else if (pindexLast->nHeight+1 > nDifficultySwitchHeight) {
        if (nActualTimespan < nActualTimespanMin/4)
            nActualTimespan = nActualTimespanMin/4;
        if (nActualTimespan > nActualTimespanMax)
            nActualTimespan = nActualTimespanMax;
    } else {
        if (nActualTimespan < nActualTimespanMin)
            nActualTimespan = nActualTimespanMin;
        if (nActualTimespan > nActualTimespanMax)
            nActualTimespan = nActualTimespanMax;
    }

    // Retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= fNewDifficultyProtocol2 ? nTargetTimespanCurrent : nTargetTimespan;
    if (bnNew > bnProofOfWorkLimit) bnNew = bnProofOfWorkLimit;

    // debug print
#if defined( LOG_DIFFICULTY_RETARGETING )
    LogPrintf("Difficulty Retarget - pre-Kimoto Gravity Well era\n");
    LogPrintf("nTargetTimespan = %lld    nActualTimespan = %lld\n", nTargetTimespan, nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().ToString());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString());
#endif
    return bnNew.GetCompact();
}

/*
 *  Difficulty formula, Anoncoin - kimoto gravity well, use of this ended early in 2015
 */
unsigned int static GetNextWorkRequiredForKgw(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const CBigNum &bnProofOfWorkLimit)
{
    uint64_t PastBlocksMin = (SECONDSPERDAY * 0.25) / nTargetSpacing;
    uint64_t PastBlocksMax = (SECONDSPERDAY * 7) / nTargetSpacing;

    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    const CBlockHeader *BlockCreating = pblock;

    uint64_t    PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double  PastRateAdjustmentRatio = double(1);
    CBigNum PastDifficultyAverage;
    CBigNum PastDifficultyAveragePrev;
    double  EventHorizonDeviation;
    double  EventHorizonDeviationFast;
    double  EventHorizonDeviationSlow;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin)
        return bnProofOfWorkLimit.GetCompact();

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if( PastBlocksMax > 0 && i > PastBlocksMax )
            break;
        PastBlocksMass++;

        if (i == 1)
            PastDifficultyAverage.SetCompact(BlockReading->nBits);
        else
            PastDifficultyAverage = ((CBigNum().SetCompact(BlockReading->nBits) - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev;
        PastDifficultyAveragePrev = PastDifficultyAverage;
        PastRateActualSeconds = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();
        PastRateTargetSeconds = nTargetSpacing * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);
        if (PastRateActualSeconds < 0)
            PastRateActualSeconds = 0;
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);

        EventHorizonDeviation = 1.0 + (0.7084 * pow((double(PastBlocksMass)/double(144.0)), -1.228));
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1.0 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin) {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)) {
                assert(BlockReading);
                break;
            }
        }
        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    CBigNum bnNew(PastDifficultyAverage);
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
        bnNew *= PastRateActualSeconds;
        bnNew /= PastRateTargetSeconds;
    }
    if( bnNew > bnProofOfWorkLimit ) bnNew = bnProofOfWorkLimit;

    // debug print
#if defined( LOG_DIFFICULTY_RETARGETING )
    LogPrintf("Difficulty Retarget - Kimoto Gravity Well era\n");
    LogPrintf("PastRateAdjustmentRatio = %g\n", PastRateAdjustmentRatio);
    LogPrintf("Before: %08x %s\n", BlockLastSolved->nBits, CBigNum().SetCompact(BlockLastSolved->nBits).getuint256().ToString().c_str());
    LogPrintf("After: %08x %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());
#endif
    return bnNew.GetCompact();
}

#if defined( HARDFORK_BLOCK )

//MultiAlgo Target updates, most all these values could just as well be defined as constants too...do
#define MULTIALGO_AVERAGING_INTERVAL 10                                 // in blocks, ToDo: why not use nMedianTimeSpan?
#define MULTIALGO_TARGET_SPACING CChainParams::MAX_ALGO_TYPES * (nTargetSpacing / 2)  // in seconds, Anoncoin with 3 algos = target spacing of 4.5 minute spacing

static const int64_t nInterval = nTargetTimespan / nTargetSpacing;      // New global constant for Multi-Algo calculations. Interval of 478.8 seconds or 7.98 minutes
static const int64_t nAveragingTargetTimespan = MULTIALGO_AVERAGING_INTERVAL * MULTIALGO_TARGET_SPACING;
static const int64_t nLocalDifficultyAdjustment = 4;                    // In percentage: 4% down, 16% up
static const int64_t nMaxAdjustDownV3 = 16;                             // 16% adjustment down
static const int64_t nMaxAdjustUpV3 = 8;                                // 8% adjustment up
static const int64_t nMinActualTimespanV3 = nAveragingTargetTimespan * (100 - nMaxAdjustUpV3) / 100;
static const int64_t nMaxActualTimespanV3 = nAveragingTargetTimespan * (100 + nMaxAdjustDownV3) / 100;

// New Block version type #3 definitions we'll be using
enum BlockMinedWith {
    BLOCK_VERSION_DEFAULT        = 3,           // Merged mined blocks, up the default block version by one, to version #3, and default to algo SCRYPT
    BLOCK_VERSION_SHA256D        = (1 << 8) | 3,// The next byte up, now represents the algo this block was mined with, if zero then it was SCRYPT and simply is a version 3 block
    BLOCK_VERSION_PRIME          = (2 << 8) | 3,// Code needs to now check though, as the upper bits could shed more light on the proof of work results.
    BLOCK_VERSION_ALGOS          = (7 << 8) | 3 // This mask can be used to expose the 3 bits in the byte above version #, allowing for up to 7 more algos
};

// GetAlgo takes the simple nVersion field (int), found in every block, and decides which Algo was used for it.
// Returns the enumeration type, as defined in the chainparam source files
CChainParams::MinedWithAlgo GetAlgo(int nVersion)
{
    CChainParams::MinedWithAlgo mwa;
    switch( (BlockMinedWith)nVersion )
    {
        case BLOCK_VERSION_SHA256D:
            mwa = CChainParams::ALGO_SHA256D;
        case BLOCK_VERSION_PRIME:
            mwa = CChainParams::ALGO_PRIME;
        default: mwa = CChainParams::ALGO_SCRYPT;     // Version 1, 2 or anything else assume SCRYPT
    }
    return mwa;
}

std::string GetAlgoName(CChainParams::MinedWithAlgo Algo)
{
    switch (Algo)
    {
        case CChainParams::ALGO_SCRYPT:
            return std::string("scrypt");
        case CChainParams::ALGO_SHA256D:
            return std::string("sha256d");
        case CChainParams::ALGO_PRIME:
            return std::string("prime");
    }
    return std::string("unknown");
}

CChainParams::MinedWithAlgo GetBlockAlgo( const CBlockIndex* pIndex ) { return GetAlgo( pIndex->nVersion ); }
/**
 * Here we hunt back in time from the given block index, to the newest block mined of the given algo,
 * if the block index search takes up to genesis or before the HARDFORK_BLOCK, then we know the algo was SCRYPT,
 * and can stop looking any further, if that was not the requested search.
 */
const CBlockIndex* GetLastBlockIndexForAlgo(const CBlockIndex* pindex, CChainParams::MinedWithAlgo mwa);

const CBlockIndex* GetLastBlockIndexForAlgo(const CBlockIndex* pindex, CChainParams::MinedWithAlgo mwa)
{
    while( pindex && (pindex->nHeight > HARDFORK_BLOCK || mwa == CChainParams::ALGO_SCRYPT) )
    {
        if( GetBlockAlgo( pindex ) == mwa )
            return pindex;
        pindex = pindex->pprev;
    }
    return NULL;
}

/*
 *  Difficulty formula, Anoncoin - Early in 2015, due to continous attack & weakness exploits known about the KGW difficulty algo, this was implemented as a solution
 */
unsigned int static GetNextWorkRequiredAfterKgw(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const CBigNum &bnProofOfWorkLimit )
{
    CChainParams::MinedWithAlgo algo = CChainParams::ALGO_SCRYPT;
    unsigned int nProofOfWorkLimit = bnProofOfWorkLimit.GetCompact();

    if (pindexLast == NULL)                                 // If the last block was the Genesis block, we're done
        return bnProofOfWorkLimit.GetCompact();

    // find first block in averaging interval
    // Go back by what we want to be MULTIALGO_AVERAGING_INTERVAL blocks per algo
    const CBlockIndex* pindexFirst = pindexLast;

    for (int i = 0; pindexFirst && i < CChainParams::MAX_ALGO_TYPES * MULTIALGO_AVERAGING_INTERVAL; i++)
        pindexFirst = pindexFirst->pprev;

    const CBlockIndex* pindexPrevAlgo = GetLastBlockIndexForAlgo(pindexLast, algo);
    if (pindexPrevAlgo == NULL || pindexFirst == NULL)
        return nProofOfWorkLimit; // not enough blocks available

    // Limit adjustment step
    // Use medians to prevent time-warp attacks
    int64_t nActualTimespan = pindexLast->GetMedianTimePast() - pindexFirst->GetMedianTimePast();

    nActualTimespan = nAveragingTargetTimespan + (nActualTimespan - nAveragingTargetTimespan)/6;
    LogPrintf("  nActualTimespan = %d before bounds\n", nActualTimespan);
    if (nActualTimespan < nMinActualTimespanV3)
        nActualTimespan = nMinActualTimespanV3;
    if (nActualTimespan > nMaxActualTimespanV3)
        nActualTimespan = nMaxActualTimespanV3;

    // Global retarget
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrevAlgo->nBits);
    bnNew *= nActualTimespan;
    bnNew /= nAveragingTargetTimespan;

    // Per-algo retarget
    int nAdjustments = pindexPrevAlgo->nHeight - pindexLast->nHeight + CChainParams::MAX_ALGO_TYPES - 1;
    if (nAdjustments > 0)
    {
        for (int i = 0; i < nAdjustments; i++)
        {
            bnNew /= 100 + nLocalDifficultyAdjustment;
            bnNew *= 100;
        }
    }
    if (nAdjustments < 0)
    {
        for (int i = 0; i < -nAdjustments; i++)
        {
            bnNew *= 100 + nLocalDifficultyAdjustment;
            bnNew /= 100;
        }
    }

    if( bnNew > bnProofOfWorkLimit ) bnNew = bnProofOfWorkLimit;

    // debug print
// #if defined( LOG_DIFFICULTY_RETARGETING )
    LogPrintf("Difficulty Retarget - post-Kimoto Gravity Well\n");
    LogPrintf("nTargetTimespan = %d    nActualTimespan = %d\n", nTargetTimespan, nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, CBigNum().SetCompact(pindexLast->nBits).getuint256().ToString().c_str());
    LogPrintf("After: %08x %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());
// #endif

	return bnNew.GetCompact();
}

#endif // defined( HARD_FORK )

// The following function are all that is seen from the outside world and used throughout
// the rest of the source code for Anoncoin, everything above should be static and only
// referenced from within this source code file.

/*
 * The workhorse routine, which oversees the blockchain Proof Of Work algorithms
 */
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    // Before we implement merged mining, there was no other ProofOfWorkLimit than the base Scrypt algo
    // We now setup that variable, and pass it as a parameter to the various implementations of difficulty
    // calculation, that have happened over the years...
    const CBigNum &bnProofOfWorkLimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );

    assert(pindexLast);
    if( TestNet() || pindexLast->nHeight > nDifficultySwitchHeight3 ) {
#if defined( HARDFORK_BLOCK )
        if( TestNet() || pindexLast->nHeight > nDifficultySwitchHeight4 ) {
            return GetNextWorkRequiredAfterKgw(pindexLast, pblock, bnProofOfWorkLimit );
        } else
#endif
        return GetNextWorkRequiredForKgw(pindexLast, pblock, bnProofOfWorkLimit );
    } else
        return OldGetNextWorkRequired(pindexLast, pblock, bnProofOfWorkLimit );
}


/*
 * The primary routine, which determines Anoncoins mined value over time...
 */
int64_t GetBlockValue(int nHeight, int64_t nFees)
{
    int64_t nSubsidy = 5 * COIN;
    // Some adjustments to the start of the lifetime to Anoncoin
    if (nHeight < 42000) {
        nSubsidy = 4.2 * COIN;
    } else if (nHeight < 77777) { // All luck is seven ;)
        nSubsidy = 7 * COIN;
    } else if (nHeight == 77778) {
        nSubsidy = 10 * COIN;
    } else {
        nSubsidy >>= (nHeight / 306600); // Anoncoin: 306600 blocks in ~2 years
    }
    return nSubsidy + nFees;
}


/*
 * The primary routine, which verifies a claim of Proof Of Work
 */
bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // printf("CheckProofOfWork Hash: %s, nBits: %08x, Target: %08x, bnLimt: %08x \n", hash.ToString().c_str(), nBits, bnTarget.GetCompact(), Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ).GetCompact());
    // ToDo: Which Algo?  After merge mining begins, which coins ProofOfWorkLimit
    // needs to be considered when evaluating someone else's claim about it.
    if (bnTarget <= 0 || bnTarget > Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT ))
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches or exceeds the claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

//
// minimum amount of work that could possibly be required nTime after
// minimum work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    // ToDo: Which Algo?  After merge mining begins, which coins ProofOfWorkLimit
    // needs to be considered when evaluating minimum work requirements..
    const CBigNum &bnProofOfWorkLimit = Params().ProofOfWorkLimit( CChainParams::ALGO_SCRYPT );

    // Testnet has min-difficulty blocks
    // after nTargetSpacing*2 time between blocks:
    if( TestNet() && nTime > nTargetSpacing*2 )
        return bnProofOfWorkLimit.GetCompact();

    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    while (nTime > 0 && bnResult < bnProofOfWorkLimit) {
		// ToDo: Ask CryptoSlave about a possible change to this value, after Kgw ends
		// on the Maximum adjustment amount. The following commented out equation maybe
		// a better fit for the model we want to use...
        //  bnResult = (bnResult * 100) / 50;

        // Maximum 141% adjustment...
        bnResult = (bnResult * 99) / 70;
        // ... in best-case exactly 4-times-normal target time
        nTime -= nTargetTimespan * 4;
    }
    if( bnResult > bnProofOfWorkLimit ) bnResult = bnProofOfWorkLimit;
    return bnResult.GetCompact();
}

