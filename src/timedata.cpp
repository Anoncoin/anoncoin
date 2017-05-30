// Copyright (c) 2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "timedata.h"

#include "netbase.h"
#include "sync.h"
#include "ui_interface.h"
#include "util.h"

#include <boost/foreach.hpp>

using namespace std;

//! Our Median filter is now odd, and addresses the btc issue #4521 with a solution so that it keeps working properly, once the filter is full.
#define MEDIAN_FILTER_SIZE 199
//! How many node connections must we have had, before an offset is worth considering?  Good question.
//! This design inserts 2 samples when the software is 1st started, +/- the Max Allowed.  So we wait for at least 5 real connections, and set this to 7
#define MIN_OFFSETS_BEFORE_CONSIDERATION 7
//! Only let other nodes change our time by so much, this is now set to +/- 15 minutes
#define MAX_ALLOWED_PEER_OFFSET 900
//! We leave this max to 5 minutes, if all of the peers think our sense of time is off by more than this amount, then we flag the situation as a problem
//! and ask the user to fix the system clock so that it agrees with the network peers.  If even one peer thinks our sense of time is within this limit,
//! then it is considered as not a warning condition.  This is also a +/- value.
#define MAX_WARNING_OFFSET_FOUND 300

static CCriticalSection cs_nTimeOffset;
static int64_t nTimeOffset = 0;

static bool fWarningReported = false;
static uint16_t nAnalysisCount = 0;
//! This flag is set false, if the peers do not agree with our Adjusted time,
//! miners (both RPC and Internal) can be stopped or not allowed to start is
//! one example of how this can be used.
static bool fPeersAgreeWithOurAdjustedTime = true;

//! We keep track of which nodes have contacted us and provided TimeData,
//! although it would be nice to just update the samples, it could allow
//! one node to repeatably apply correction to our time, so must not be
//! allowed.
static set<CNetAddr> setKnown;
//! Data will be added to this MedianFilter object, we start out with 1 sample and size value, what is stored is maximum offset allowed.
static CMedianFilter<int64_t> vTimeOffsets( MEDIAN_FILTER_SIZE, MAX_ALLOWED_PEER_OFFSET );

/**
 * "Never go to sea with two chronometers; take one or three."
 * Our three time sources are:
 *  - System clock
 *  - Median of other nodes clocks
 *  - The user (asking the user to fix the system clock if the first two disagree)
 */
int64_t GetTimeOffset()
{
    LOCK(cs_nTimeOffset);
    return nTimeOffset;
}

int64_t GetAdjustedTime()
{
    return GetTime() + GetTimeOffset();
}

bool PeersAgreeWithOurAdjustedTime()
{
    LOCK(cs_nTimeOffset);
    return fPeersAgreeWithOurAdjustedTime;
}

void AddTimeData(const CNetAddr& ip, int64_t nTime)
{
    int64_t nOffsetSample = nTime - GetTime();

    LOCK(cs_nTimeOffset);

    if (!setKnown.insert(ip).second) {
        LogPrint( "timedata", "%s : Rejected a previously sampled peers time data, multiple updates not allowed.\n", __func__ );
        return;
    }
    //! Limit the size of the known node addresses, with I2P this can grow rather large, and it does not hurt to update a repeat connection once in awhile...
    if( setKnown.size() > MEDIAN_FILTER_SIZE / 2 ) setKnown.erase( setKnown.begin() );

    //! Input to the filter removes the oldest entry(if needed), and sorts it as well...
    //! If the offset value given to us by a peer is greater than, or less than -MAX_ALLOWED_PEER_OFFSET, we do not believe them, and store only the limit.
    //! Once MIN_OFFSETS_BEFORE_CONSIDERATION connections have been made, red flags will be going off all over the place, and it need not be any larger
    //! or smaller than we have coded as the limit values here.  An attacker could well study this code and try to exploit it, such that a good share of
    //! peers on the network have their time offsets changed to some value more to their liking, it would still be possible.  What is now impossible for
    //! them to effect is the peer clocks on nodes run by operator(s) such as myself, with an NTC server connection, something very easy to setup, I have
    //! valid UTC time kept and updated regularly outside of this software.  By simply setting utctimeisknown=1 in the anoncoin.conf file, time offset
    //! attacks become a non-issue.  Any serious miner(s), merchant(s) and other interested enthusiasts are also going to be doing the same.  As more
    //! peers run this way, Anoncoin will become more locked to valid UTC time, and resistant to any such effort made by a would be attacker.
    //! In effect this code does adjust peer clock times for those users that need small adjustments made, and have less sophisticated means at their disposal.
    //! We want our node software to start having a much better sense of what UTC time really is, by achieving that we can do a great many other new things.
    //! Such as stop validating new blocks that have come from the future, legacy code allows for up to 2hrs!  Retarget with Headers is then possible for a
    //! much smoother and more evenly spaced block spacing, many other ideas also come to mind.  Millions of devices are now locked to UTC time and run the
    //! world, it has become a part of modern life that Internet technology has brought to us.  Likely many have already taken the time to setup an NTC server
    //! connection for themselves, if that is the case Anoncoin now offers you a personal choice to remove the would be attacker from the equation.  Be warned
    //! though, this code still does not believe you, if that feature has been turned on.  Your systems sense of time, could still be wrong.  In the code
    //! sections below, you will see that peer consensus is still required, checked for and handled with that possibility in mind...GR
    int64_t nStoredValue = nOffsetSample;
    if( nOffsetSample < -MAX_ALLOWED_PEER_OFFSET )
        nStoredValue = -MAX_ALLOWED_PEER_OFFSET;
    else if( nOffsetSample > MAX_ALLOWED_PEER_OFFSET )
        nStoredValue = MAX_ALLOWED_PEER_OFFSET;

    vTimeOffsets.input( nStoredValue );
    //! If an attacker had MEDIAN_FILTER_SIZE/2 different IP connection addresses to connect from, they would have to use them all before the offset sorted
    //! to the median was picked and used as the new offset by as many peers as was possible to adjust the times on. Given the new above limits imposed and
    //! other safeguards, to what end that effort had as a goal becomes questionable.

    uint32_t nSamplesSize = vTimeOffsets.size();
    //! Check if this is the first sample being placed into the filter
    if( nSamplesSize == 2 ) {
        //! If it is, then we store a 2nd sample with the opposite extreme to the most positive maximum,
        //! this allows our fPeersAgreeWithOurAdjustedTime logic to work, because we must initialize the
        //! filter with at least 1 sample, and if it is 0, then for ~200 connections it will always think
        //! that at least 1 peer agrees with our adjusted time, which may NOT be the case.
        vTimeOffsets.input( -MAX_ALLOWED_PEER_OFFSET );
        //! After startup and when that is inserted, we then bump the number of samples to 3, which is our
        //! starting out 1st connection minimum number of data points.
        nSamplesSize++;
    }
    LogPrint( "timedata", "%s : Added a new peer, offset %+d seconds, now ", __func__, nOffsetSample );
    if( nSamplesSize == MEDIAN_FILTER_SIZE )
        LogPrint( "timedata", "using most recent samples.\n" );
    else
        LogPrint( "timedata", "have %d samples.\n", nSamplesSize );

    if( nStoredValue != nOffsetSample )
        LogPrint( "timedata", "%s : WARNING - Stored time offset out of bounds, and limited to %+ds.  Someones clock is way off.\n", __func__, nStoredValue );

    bool fOffsetChanged = false;
    //! Once we have at least MIN_OFFSETS_BEFORE_CONSIDERATION samples, we start using the median filters 'median' value as our offset,
    //! due to coding changes in the filter, this could be changed every update, for both even and odd number of samples.  However
    //! we do not use that feature, when the number of samples is low in the beginning, that concept of averaging 2 samples near the
    //! middle may not be a good idea, so we only update here if the number of connections added is odd, as the old original code did.
    //! We also check for and report to the user, if everyone else thinks our time is set wrong, we keep bugging the user to fix that.
    //! If the peers we are really connected to think that is the case, this is why we can not start out the filter with an offset of
    //! 0.  If we did that, then for MEDIAN_FILTER_SIZE connections we would never think anyone possibly disagreed with our sense of
    //! time, until that initial sample of 0 offset was removed from the filter.  So now the filter gets preloaded with +/- limit values instead...
    if( ( (nSamplesSize < MEDIAN_FILTER_SIZE) && (nSamplesSize & 1 ) ) && nSamplesSize >= MIN_OFFSETS_BEFORE_CONSIDERATION ) {
        int64_t nMedian = vTimeOffsets.median();
        vector<int64_t> vSorted = vTimeOffsets.sorted();
        //! This code has been developed so the user can decide if UTC time is something they take care of on the system without our
        //! need to worry about the clock being correct, however like peers, users can lie and or have technical problems for which
        //! this aspect of their configuration is not working properly, so this code was developed with that in mind, it does not assume
        //! that the user is correct, it continues to check that the peers also agree our time is correct, if that is found by them to
        //! not be the case, our warning logic will start and keep bugging the user to fix the problem, at least in QT builds.
        //! ToDo: Check how well anoncoind builds handle this error, the debug.log will at least for sure be updated.
        bool fUTCisKnown = GetBoolArg( "-utctimeisknown", false );

        // if( !fUTCisKnown && abs64(nMedian) < MAX_ALLOWED_PEER_OFFSET ) {
        if( !fUTCisKnown ) {
            if( nTimeOffset != nMedian ) {
                nTimeOffset = nMedian;
                fOffsetChanged = true;
            }
        } else {
            //! Although this next line of code should not be needed, it is possible the software could elsewhere turn the flag on and off, and the offset is not 0.
            nTimeOffset = 0;
            //! ToDo: More could be done here, such as alerting the QT build user of the discovery... What value should be the min 0,1,2, 3 secs or more?
            if( abs64(nMedian) > 1 )
                LogPrintf( "timedata", "%s : WARNING - Although you have indicated UTC time is known, peer timing analysis would suggest a %+ds offset.\n", __func__, nMedian );
        }
        //! If everyone thinks our time is wrong, then we must, as a minimum, draw the users attention to what must be a
        //! clock date/time setting problem on the computer this node is running from.
        //! This is not easy to make have happen, every connection most disagree with our sense of time, should even one
        //! peer think it is ok, that is good enough for this logic and makes it nearly impossible to produce by intent
        //! from a sole attacker.  So with that in mind, we can now start using and considering the new flag produced, if
        //! fPeersAgreeWithOurAdjustedTime should ever be false, we could for example shut down the internal miners,
        //! disallow RPC miner commands from working, until the system clock problem is fixed.  Obviously it must be a clock
        //! setting error for our node if all the peers disagree with its value.  As a minimum, we should not be producing
        //! blocks with block times that are in fact not valid UTC times +/- a few seconds...
        fPeersAgreeWithOurAdjustedTime = false;
        BOOST_FOREACH(int64_t nOffset, vSorted)
            if( abs64(nOffset) < MAX_WARNING_OFFSET_FOUND ) { fPeersAgreeWithOurAdjustedTime = true; break; }

        if( !fPeersAgreeWithOurAdjustedTime ) {
            if( !fWarningReported ) {
                string strMessage = _("Warning: All peers agree, our sense of network time is wrong. Please check that your computer's date and time are correct! If our clock is wrong Anoncoin Core will not work properly.");
                strMiscWarning = strMessage;
                LogPrintf("%s : *** %s\n", __func__, strMessage);
                uiInterface.ThreadSafeMessageBox(strMessage, "", CClientUIInterface::MSG_WARNING);
                fWarningReported = true;
            }
        }
        //! nAnalysisCount will only really get bumped every other connection, until the filter fills up, so each of its counts represents 2 new node connections,
        //! modulo 3 then represents actually 6 connections have past since we last reported the warning to the user.  If the filter has filled up, we really
        //! get aggressive about how often we report a clock being set wrong, it will then start happening twice as often.
        if( nAnalysisCount++ % 3 == 0 ) fWarningReported = false;

        if( fOffsetChanged ) {
            if( fDebug ) {
                LogPrintf( "%s : Sorted time offset filter: ", __func__ );
                BOOST_FOREACH(int64_t n, vSorted)
                    LogPrintf( "%+d ", n);
                LogPrintf( "\n" );
            }
            LogPrint( "timedata", "%s : Adjusted time now set to %+d seconds.\n", __func__, nTimeOffset );
        } else {
            if( fUTCisKnown )
                LogPrint( "timedata", "%s : Peer data ignored, system UTC time as defined by the user preferred.\n", __func__ );
            else
                LogPrint( "timedata", "%s : Peer data indicates no change need be made to our offset.\n", __func__ );
        }
    }
}
