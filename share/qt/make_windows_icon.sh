#!/bin/bash
# create multiresolution windows icon
ICON_SRC=../../src/qt/res/icons/anoncoin.png
ICON_DST=../../src/qt/res/icons/anoncoin.ico
convert ${ICON_SRC} -resize 16x16 anoncoin-16.png
convert ${ICON_SRC} -resize 32x32 anoncoin-32.png
convert ${ICON_SRC} -resize 48x48 anoncoin-48.png
convert anoncoin-16.png anoncoin-32.png anoncoin-48.png ${ICON_DST}

