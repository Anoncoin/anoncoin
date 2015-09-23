#!/bin/bash
# create multiresolution windows icon for testnet
ICON_SRC=../../src/qt/res/src/Anon-logo-testnet_400.png
ICON_DST=../../src/qt/res/icons/anoncoin_testnet.ico
convert ${ICON_SRC} -resize 16x16 anoncoin_testnet-16.png
convert ${ICON_SRC} -resize 24x24 anoncoin_testnet-24.png
convert ${ICON_SRC} -resize 32x32 anoncoin_testnet-32.png
convert ${ICON_SRC} -resize 48x48 anoncoin_testnet-48.png
convert ${ICON_SRC} -resize 64x64 anoncoin_testnet-64.png
convert ${ICON_SRC} -resize 256x256 anoncoin_testnet-256.png
convert anoncoin-16.png anoncoin-24.png anoncoin-32.png anoncoin-48.png anoncoin-64.png anoncoin-256.png ${ICON_DST}

