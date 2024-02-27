#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/odyncash.png
ICON_DST=../../src/qt/res/icons/odyncash.ico
convert ${ICON_SRC} -resize 16x16 odyncash-16.png
convert ${ICON_SRC} -resize 32x32 odyncash-32.png
convert ${ICON_SRC} -resize 48x48 odyncash-48.png
convert odyncash-16.png odyncash-32.png odyncash-48.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/odyncash_testnet.png
ICON_DST=../../src/qt/res/icons/odyncash_testnet.ico
convert ${ICON_SRC} -resize 16x16 odyncash-16.png
convert ${ICON_SRC} -resize 32x32 odyncash-32.png
convert ${ICON_SRC} -resize 48x48 odyncash-48.png
convert odyncash-16.png odyncash-32.png odyncash-48.png ${ICON_DST}
rm odyncash-16.png odyncash-32.png odyncash-48.png
