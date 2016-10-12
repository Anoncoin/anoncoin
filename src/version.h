// Copyright (c) 2012 The Bitcoin developers
// Copyright (c) 2013-2015 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef ANONCOIN_VERSION_H
#define ANONCOIN_VERSION_H

//! client version settings has moved to clientversion.h/cpp

//! Network protocol version is all that is left in this file, and so it does not allocate a different constant in one library
//! verses another accidentally for you, it is now simply a define, and will take on whatever default data type you need it
//! to be.  Any other values you expected to find here have been moved to where they are used, if at all anymore...GR
#define PROTOCOL_VERSION 70012

#endif
