// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <consensus/params.h>
#include <timedata.h>
#include <arith_uint256.h>
#include <uint256.h>

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

#define HARDFORK_BLOCK 555555
#define HARDFORK_BLOCK2 585555

inline int roundint(double d)
{
    return (int)(d > 0 ? d + 0.5 : d - 0.5);
}

inline int64_t roundint64(double d)
{
    return (int64_t)(d > 0 ? d + 0.5 : d - 0.5);
}

struct FilterPoint
{
    int64_t nBlockTime;
    uint32_t nDiffBits;
    int32_t nSpacing;
    int32_t nSpacingError;
    int32_t nRateOfChange;
    bool operator < (const FilterPoint& rhs) const { return nBlockTime < rhs.nBlockTime; }
};

struct RetargetStats
{
    double dProportionalGain;       //! The Proportional gain of the control loop
    int64_t nIntegrationTime;       //! The Integration period in seconds.
    double dIntegratorGain;         //! The Integration gain of the control loop
    double dDerivativeGain;         //! The Derivative gain of the control loop

    bool fUsesHeader;
    bool fPidOutputLimited;
    bool fDifficultyLimited;

    int32_t nTipFilterSize;

    uint32_t nMaxDiffIncrease;
    uint32_t nMaxDiffDecrease;
    uint32_t nPrevDiffWeight;
    uint32_t nSpacingErrorWeight;
    uint32_t nRateChangeWeight;
    uint32_t nIntegratorHeight;     //! The last blockchain height we calculated the Integrator Charge for.
    uint32_t nBlocksSampled;        //! The number of blocks sampled during the Integrator charge.
    uint32_t nIntervalForceDiffDecrease; //The interval without any block found before forcing the difficulty to decrease

    int64_t nMinTimeAllowed;
    int64_t nLastCalculationTime;         //! The latest block time, possibly for an as yet un-mined block
    int64_t nIntegratorChargeTime;
    int64_t nBlockSpacing;

    double dSpacingError;         //! Defines the short term error in block time.  Positive or negative
    double dRateOfChange;     //! The derivative calculation is the short term rate of change measurement
    double dProportionalTerm;
    double dIntegratorTerm;
    double dDerivativeTerm;
    double dPidOutputTime;          //! The PID output time calculation, after all constant and calculated terms have been applied
    double dAverageTipSpacing;

    arith_uint256 uintPrevDiff;
    arith_uint256 uintTargetDiff;          //! Final result, the new retarget output value, before the min proof-of-work has been checked.
    arith_uint256 uintPrevShad;

    std::vector<FilterPoint> vTipFilter;
};

// namespace retargetpid {

class CRetargetPidController
{
private:
    //! The response to short term flux is controlled by adjusting the coefficient values on the P and D terms.
    //! Common knowledge to those in the PID controller designer field, yet a new way to look at retarget control
    //! for cryptocoin users.  Figuring out the best values to use, so we have a fast response to real world
    //! changes in the mining hash rate, not over or undershooting what should be the correct difficulty hash
    //! rate and offer smooth sailing without oscillation when mining is going along just right, is not an
    //! easy task.
    //! The Proportional term needs to know about very recent block timing errors, a difficult value to compute.
    //! Stored here as dBLockTimeError and calculated in SetBlockTimeError()
    //! The Derivative term wants to know that error relative to the target block time span.  As it is a rate
    //! of Change detector.  For both those terms offer us good performance when hash rate is changing fast and
    //! and while working with the small set of block times, currently at the tip of the chain.
    //! The Integrator is now the driving factor in Anoncoin Retarget control, its value is set it seconds and
    //! plans to initially test it with 3 days out to 2 weeks are being planned.  It looks at how close the block
    //! times have been over a much longer time period, as defined by the nIntegrationTime constant.  While
    //! executing ChargeIntegrator() it also summarizes what the difficulty has been in the past, which is then
    //! used as the output value if there is no short term error in the block times.

    //! The PID Control loop values as defined by the user
    double dProportionalGain; //! The Proportional gain of the control loop
    int64_t nIntegrationTime; //! The Integration period in seconds.
    double dIntegratorGain;   //! The Integration gain of the control loop
    double dDerivativeGain;   //! The Derivative gain of the control loop

    //! Operational constants, intermediate results and logging options
    bool fUsesHeader;
    bool fTipFilterInitialized;      //! A way to know if the tiptimes filter has been initialized with blockindex entries at the tip
    bool fPidOutputLimited;
    bool fDifficultyLimited;
    bool fRetargetNewLog;
    bool fDiffCurvesNewLog;
    bool fLogDiffLimits;

    //! The nTipFilterBlocks defines the size of the TipFilter for the PID controller, which defines how the Instantaneous Block
    //! Timing errors are calculated.  For everything except the Integrator term this is of major importance to how our PID retarget
    //! system works, and it's value is measured in number of blocks.  Block times, spacing, spacing error, rate of error change and
    //! previous difficulty values are all calculated as weighted values based on this filter.  If a future block header time is to
    //! be used, the size of the filter grows by 1, in order to accommodate that capability.
    int32_t nTipFilterBlocks;
    int32_t nWeightedAvgTipBlocksUp;   //CSlave: n last block of the tip for calculus of the weighted difficulty
    int32_t nWeightedAvgTipBlocksDown;   
    int32_t nIntegratorHeight;      //! Saves recalculating if we already have the Integrator charge at this height
    int32_t nIndexFilterHeight;     //! Same goes for the IndexTipFilter, where the previous difficulty calculation is made
    const CBlockIndex* pChargedToIndex;   //! Its quicker and easier to just keep a copy of the pointer to the BlockIndex, than search for it.

    uint32_t nMaxDiffIncrease;
    uint32_t nMaxDiffDecrease;
    uint32_t nBlocksSampled;        //! The number of blocks sampled during the Integrator charge.
    uint32_t nPrevDiffWeight;
    uint32_t nSpacingErrorWeight;
    uint32_t nRateChangeWeight;

    int64_t nLastCalculationTime;   //! Keeps the latest block time, possibly for an as yet unmined block at tip height + 1
    int64_t nIntegratorChargeTime;
    int64_t nPidOutputTime;         //! The PID output time after being limit checked (1sec), can now only be positive and used to adjust difficulty

    double dIntegratorBlockTime;    //! The actual average block time (in secs) as measured over the integration period
    double dSpacingError;           //! Defines the short term block spacing time error.  Positive or negative
    double dRateOfChange;           //! This derivative calculation output is the short term rate of change measurement
    double dProportionalTerm;
    double dIntegratorTerm;
    double dDerivativeTerm;
    double dPidOutputTime;          //! The PID output time calculation, after all constant and calculated terms have been applied, but the limits not checked
    double dAverageTipSpacing;

    arith_uint256 uintPrevDiffCalculated;      //! The Factorial filter mining difficulty as seen over the tip times periods
    arith_uint256 uintTipDiffCalculatedUp;       //CSlave: Weighted difficulty on the n last block of the tip
    arith_uint256 uintTipDiffCalculatedDown;       
    arith_uint256 uintTargetBeforeLimits;          //! Final result, the new retarget output value, before the min proof-of-work has been checked.
    arith_uint256 uintPrevDiffForLimitsLast;     //CSlave: Previous difficulty of the last block
    arith_uint256 uintPrevDiffForLimitsTipUp;      //CSlave: Previous difficulty calculated on the partial tip blocks selected for diff UP
    arith_uint256 uintPrevDiffForLimitsTipDown;      
    arith_uint256 uintPrevDiffForLimitsIncreaseLast;
    arith_uint256 uintPrevDiffForLimitsDecreaseLast;
    arith_uint256 uintPrevDiffForLimitsIncreaseTip;
    arith_uint256 uintPrevDiffForLimitsDecreaseTip;
    arith_uint256 uintDiffAtMaxIncreaseLast;
    arith_uint256 uintDiffAtMaxDecreaseLast;
    arith_uint256 uintDiffAtMaxIncreaseTip;      //CSlave: The final limit of the weighted average used for diff increase
    arith_uint256 uintDiffAtMaxDecreaseTip;
    arith_uint256 uintTargetAfterLimits;        //! Final result, the new retarget output value, after all limits have been checked.
    //! This value allows starting TestNet out with a pre-determined difficulty, although the 1st mocktime blocks are mined fast at the minimum
    //! difficulty, this value will be stored in the block as the compact nBits version of this big number value and it allows the experiment'er to
    //! not have to wait for test chains to first settle on a difficulty that typically will be much harder than 1x the minimum.
    arith_uint256 uintTestNetStartingDifficulty;

    //! We keep as many as two Tip Filters, depending on if 'fUsesHeader' is true.  The 2nd one will never be used, if that is false.
    //! Neither really need to be saved, once the calculations have been run, although vIndexTipFilter is needed repeatably to create
    //! the vTipFilterWithHeader filter data when miners are requesting new blocktemplates. Primarily these are kept for reporting
    //! purposes, and the vIndexTipFilter is setup each time only when called upon by UpdateTipsFilter() as the height changes.
    std::vector<FilterPoint> vIndexTipFilter;
    std::vector<FilterPoint> vTipFilterWithHeader;

    // New Derivative term RateOfChange filter design weight vector
    // std:vector<uint16_t> vRocFilterWeights;

    //! Updates the individual and total spacing errors and rate of change values based on the given filter reference.
    bool UpdateFilterTimingResults( std::vector<FilterPoint>& vFilterPoints );
    //! Primary calculation for SetBlockTimeError, also can be used to calculate any on-the-fly TipTime value, given a fixed integrator period.
    bool CalcBlockTimeErrors( const int64_t nTipTime );
    //! Returns false if the current calculation saved was for the last block index height and the lastest block time
    bool IsPidUpdateRequired( const CBlockIndex* pIndex, const CBlockHeader* pBlockHeader );
    //! Sets the important block timing error near the tip.  Returns true if it can be calculated
    bool SetBlockTimeError( const CBlockIndex* pIndex, const CBlockHeader* pBlockHeader );
    //! Limit an output difficulty calculation change
    bool LimitOutputDifficultyChange( arith_uint256& uintResult, const arith_uint256& uintCalculated, const arith_uint256& uintPOWlimit, const CBlockIndex* pIndex );

public:
    CRetargetPidController( const double dProportionalGainIn, const int64_t nIntegratorTimeIn, const double dIntegratorGainIn, const double dDerivativeGainIn, const Consensus::Params& params );
    ~CRetargetPidController() {}

    //! Returns the PID terms the controller is set to
    void GetPidTerms( double* pProportionalGainOut, int64_t* pIntegratorTimeOut, double* pIntegratorGainOut, double* pDerivativeGainOut );
    //! Runs the Control loop update and calculates the new output difficulty, should only be called with LOCK set from GetNextWorkRequired()
    bool UpdateOutput( const CBlockIndex* pIndex, const CBlockHeader* pBlockHeader, const Consensus::Params& params );
    //! Returns true if the full integration period block times and difficulty values were able to be setup.  Should only be called with LOCK set
    bool ChargeIntegrator( const CBlockIndex* pIndex );
    //! Updates the filter based on on the BlockIndex, used to calculate instantaneous block spacing, rate of changes & limits. Should only be called with LOCK set
    bool UpdateIndexTipFilter( const CBlockIndex* pIndex );
    //! Debug.log entries, retarget.csv and diffcurves.csv code now runs in its own process code
    void RunReports( const CBlockIndex* pIndex, const CBlockHeader *pBlockHeader, const Consensus::Params& params );
    //! Returns the number of blocks used by the Tip Filter
    int32_t GetTipFilterBlocks();
    //! Returns true if this retargetpid uses headers in the output difficulty calculations
    bool UsesHeader();
    //! Returns the testnet difficulty used to initialize the chain
    arith_uint256 GetTestNetStartingDifficulty();
    //! Return the output result as an uint256 difficulty value
    arith_uint256 GetRetargetOutput();
    //! Returns the number of seconds (int64_t) per block, can be computed after summing the
    //! integration time span, although not as accurately as dIntegratorBlockTime.
    double GetIntegrationBlockTime( void );

    /***
     * These next methods handle the CS_retarget LOCK for you and are available for general use
     */
    //! Returns the number of blocks required to integrate over 1 full Integration Time period.
    uint32_t CalcBlockIndexRequired( const CBlockIndex* pIndex );
    //! Returns the correct TipFilterSize, used by gethetworkhashps rpc query, or any other routine needing it.
    int32_t GetTipFilterSize( void );
    //! Returns all the information about the current state of the Retarget Engine, or false if the data for the given height could not be calculated
    //! Height=0 is the same as the current calculated height
    bool GetRetargetStats( RetargetStats& RetargetState, uint32_t& nHeight, const CBlockIndex* pIndexAtTip, const Consensus::Params& params );
};

//!
//! Data space variables and constants defined here:
//!
extern const int64_t nTargetSpacing;            //! Defines modern Anoncoin target block time spacing
extern CRetargetPidController *pRetargetPid;    //! One retarget object is used as primary, for all the next-work-required calculations

//!
//! Global in scope, these routines are implemented in pow.cpp:
//!






unsigned int GetNextWorkRequired2(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader, const Consensus::Params& params);

unsigned int GetNextWorkRequired_Bitcoin(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int KimotoGravityWell(const CBlockIndex* pindexLast, const CBlockHeader *pblock, uint64_t TargetBlocksSpacingSeconds, uint64_t PastBlocksMin, uint64_t PastBlocksMax, const Consensus::Params& params);
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);
unsigned int GetNextWorkRequired2(const CBlockIndex* pindexLast, const CBlockHeader* pBlockHeader, const Consensus::Params& params);
bool SetRetargetToBlock( const CBlockIndex* pIndex, const Consensus::Params& params );
void RetargetPidReset( std::string strParams, const CBlockIndex* pIndex, const Consensus::Params& params );

#endif // BITCOIN_POW_H
