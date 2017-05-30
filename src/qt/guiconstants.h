// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2013-2017 The Anoncoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GUICONSTANTS_H
#define GUICONSTANTS_H

//! Constants should not allocate storage as static constants in a header file,
//! This file could be used as part of a library(s) build, accidentally linking
//! with wrong value(s) from one copy and another set of values from a different
//! library leads to debugging nightmares, also some compilers detect static
//! constants defined in header files and complain with linker warnings due to
//! multiple copies being created for each source file that includes it.
//! ...GR

/** Milliseconds between model updates */
#define MODEL_UPDATE_DELAY 250
/** AskPassphraseDialog -- Maximum passphrase length */
#define MAX_PASSPHRASE_SIZE 1024
/** AnoncoinGUI -- Size of icons in status bar */
#define STATUSBAR_ICONSIZE 16
/** Tooltips longer than this (in characters) are converted into rich text, so that they can be word-wrapped. */
#define TOOLTIP_WRAP_THRESHOLD 80
/** Maximum allowed URI length */
#define MAX_URI_LENGTH 255
/** Maximum somewhat-sane size of a payment request file (bytes)*/
#define MAX_PAYMENT_REQUEST_SIZE 50000

/** Invalid field background style */
#define STYLE_INVALID "background:#FF8080"

/** Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(128, 128, 128)
/** Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(255, 0, 0)
/** Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(140, 140, 140)
/** Transaction list -- TX status decoration - open until date */
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(64, 64, 255)
/** Transaction list -- TX status decoration - offline */
#define COLOR_TX_STATUS_OFFLINE QColor(192, 192, 192)
/** Transaction list -- TX status decoration - default color */
#define COLOR_BLACK QColor(0, 0, 0)
/** QRCodeDialog -- size of exported QR Code image */
#define EXPORT_IMAGE_SIZE 256
/** Number of frames in spinner animation */
#define SPINNER_FRAMES 35

#define QAPP_ORG_NAME "Anoncoin"
#define QAPP_ORG_DOMAIN "anoncoin.net"
#define QAPP_APP_NAME_DEFAULT "Anoncoin-Qt"
#define QAPP_APP_NAME_TESTNET "Anoncoin-Qt-testnet"

#endif // GUICONSTANTS_H
