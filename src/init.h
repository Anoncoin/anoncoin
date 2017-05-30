// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ANONCOIN_INIT_H
#define ANONCOIN_INIT_H

#include <string>

class CWallet;

namespace boost
{
class thread_group;
} // namespace boost

// Developer warning - If you create any variables in init.cpp and want to declare them external here be warned!
// It will cause linker errors while building the test_anoncoin executable, if used outside of the init.cpp source
// code file, such as in main.cpp  Suddenly you will be faced with StartShutdown, ShutdownRequested, pwalletMain
// any others being doubly defined with definitions in test_anoncoin.cpp.
// ToDo: re-design the libraries so this does not happen.

extern CWallet* pwalletMain;

void StartShutdown();
bool ShutdownRequested();
void Shutdown();
bool AppInit2(boost::thread_group& threadGroup);

/** The help message mode determines what help message to show */
enum HelpMessageMode
{
    HMM_ANONCOIND,
    HMM_ANONCOIN_QT
};

/** Help for options shared between UI and daemon (for -help) */
std::string HelpMessage(HelpMessageMode mode);
/** Returns licensing information (for -version) */
std::string LicenseInfo();

#endif
