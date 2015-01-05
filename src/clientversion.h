#ifndef CLIENTVERSION_H
#define CLIENTVERSION_H

#if defined(HAVE_CONFIG_H)
#include "bitcoin-config.h"
#else
//
// client versioning and copyright year
//

// These need to be macros, as version.cpp's and bitcoin-qt.rc's voodoo requires it
#define CLIENT_VERSION_MAJOR       0
#define CLIENT_VERSION_MINOR       9
#define CLIENT_VERSION_REVISION    3
#define CLIENT_VERSION_BUILD       0

// Set to true for release, false for prerelease or test build
#define CLIENT_VERSION_IS_RELEASE  false

// Copyright year (2009-this)
// Todo: update this when changing our copyright comments in the source
#define COPYRIGHT_YEAR 2014

#endif //HAVE_CONFIG_H

// Regardless of HAVE_CONFIG_H's state, these need to be declared, if they are not already.
//
// primecoin client version - ToDo: GR note - Needs review - copied these in so anc will compile the 'developer' branch.
//                                            They should go here though, for ease in future upgrades.  IMO LightCoin should also be listed
#if !defined(HAVE_PRIMECOIN_CONFIG_H)
#define HAVE_PRIMECOIN_CONFIG_H

#define PRIMECOIN_VERSION_MAJOR 0
#define PRIMECOIN_VERSION_MINOR 1
#define PRIMECOIN_VERSION_REVISION 2
#define PRIMECOIN_VERSION_BUILD 0
#endif

// Converts the parameter X to a string after macro replacement on X has been performed.
// Don't merge these into one macro!
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#endif // CLIENTVERSION_H

