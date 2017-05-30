// Copyright (c) 2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_TIMEDATA_H
#define ANONCOIN_TIMEDATA_H

#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <vector>

class CNetAddr;

/**
 * Median filter over a stream of values.
 * Returns the median of the last N numbers
 */
template <typename T>
class CMedianFilter
{
private:
    std::vector<T> vValues;
    std::vector<T> vSorted;
    unsigned int nSize;

public:
    CMedianFilter(unsigned int size) : nSize(size)
    {
        vValues.resize(size);
        vSorted.resize(size);
    }

    CMedianFilter(unsigned int size, T initial_value) : nSize(size)
    {
        vValues.reserve(size);
        vValues.push_back(initial_value);
        vSorted = vValues;
    }

    void sort(void)
    {
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    void input(T value)
    {
        if (vValues.size() == nSize) {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const
    {
        int size = vSorted.size();
        assert(size > 0);
        if (size & 1) // Odd number of elements
        {
            return vSorted[size / 2];
        } else // Even number of elements
        {
            return (vSorted[size / 2 - 1] + vSorted[size / 2]) / 2;
        }
    }

    int size() const
    {
        return vValues.size();
    }

    std::vector<T> sorted() const
    {
        return vSorted;
    }

    T getvalue( const int8_t nIndex ) const
    {
        assert( nIndex >= 0 && nIndex < vValues.size() );
        return vValues[ nIndex ];
    }

    void setvalue( const int8_t nIndex, const T nNewValue )
    {
        assert( nIndex >= 0 && nIndex < vValues.size() );
        vValues[ nIndex ] = nNewValue;
    }
};

/** Functions to keep track of adjusted P2P time */
extern int64_t GetTimeOffset();
extern int64_t GetAdjustedTime();
extern void AddTimeData(const CNetAddr& ip, int64_t nTime);
//! If the peers do not agree with our Adjusted time, miners (both RPC and Internal) can be stopped or not allowed to start because of this condition.
extern bool PeersAgreeWithOurAdjustedTime();

#endif // ANONCOIN_TIMEDATA_H
