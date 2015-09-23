#!/bin/bash
# create multiresolution windows icon
ICON_SRC=../../src/qt/res/src/Anon-logo_400.png
ICON_DST=../../src/qt/res/icons/anoncoin.ico
ICON_DST2=../pixmaps/anoncoin.ico
convert ${ICON_SRC} -resize 16x16 anoncoin-16.png
convert ${ICON_SRC} -resize 24x24 anoncoin-24.png
convert ${ICON_SRC} -resize 32x32 anoncoin-32.png
convert ${ICON_SRC} -resize 48x48 anoncoin-48.png
convert ${ICON_SRC} -resize 64x64 anoncoin-64.png
convert ${ICON_SRC} -resize 256x256 anoncoin-256.png
convert anoncoin-16.png anoncoin-24.png anoncoin-32.png anoncoin-48.png anoncoin-64.png anoncoin-256.png ${ICON_DST}
convert anoncoin-16.png anoncoin-24.png anoncoin-32.png anoncoin-48.png anoncoin-64.png anoncoin-256.png ${ICON_DST2}

