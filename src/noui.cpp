// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2013-2014 The Anoncoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "noui.h"

#include "ui_interface.h"
#include "util.h"

#include <stdint.h>
#include <string>

static bool noui_ThreadSafeMessageBox(const std::string& message, const std::string& caption, unsigned int style)
{
    std::string strCaption;
    // Check for usage of predefined caption
    switch (style) {
    case CClientUIInterface::MSG_ERROR:
        strCaption += _("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        strCaption += _("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        strCaption += _("Information");
        break;
    default:
        strCaption += caption; // Use supplied caption (can be empty)
    }

    LogPrintf("%s: %s\n", strCaption, message);
    fprintf(stderr, "%s: %s\n", strCaption.c_str(), message.c_str());
    return false;
}

#ifdef ENABLE_I2PSAM
static bool noui_ThreadSafeShowGeneratedI2PAddress(const std::string& caption, const std::string& pub, const std::string& priv, const std::string& b32, const std::string& configFileName)
{
    std::string msg = "\nIf you want to use a permanent I2P-address you have to set a \'mydestination\' option in the configuration file:\n";
    msg += configFileName;

    msg += "\nAddress + private key (save this text in the configuration file and keep it secret):\n";
    msg += priv;

    msg += "\n\nAddress (you can make it public):\n";
    msg += pub;

    msg += "\n\nShort base32-address:\n";
    msg += b32;
    msg += "\n\n";

    printf("%s: %s\n", caption.c_str(), msg.c_str());
    LogPrintf("%s: %s\n", caption.c_str(), msg.c_str());
}
#endif // ENABLE_I2PSAM

static void noui_InitMessage(const std::string &message)
{
    LogPrintf("init message: %s\n", message);
}

void noui_connect()
{
    // Connect anoncoind signal handlers
    uiInterface.ThreadSafeMessageBox.connect(noui_ThreadSafeMessageBox);
    uiInterface.InitMessage.connect(noui_InitMessage);
#ifdef ENABLE_I2PSAM
    uiInterface.ThreadSafeShowGeneratedI2PAddress.connect(noui_ThreadSafeShowGeneratedI2PAddress);
#endif
}
