#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/cash.png
ICON_DST=../../src/qt/res/icons/cash.ico
convert ${ICON_SRC} -resize 16x16 cash-16.png
convert ${ICON_SRC} -resize 32x32 cash-32.png
convert ${ICON_SRC} -resize 48x48 cash-48.png
convert cash-16.png cash-32.png cash-48.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/cash_testnet.png
ICON_DST=../../src/qt/res/icons/cash_testnet.ico
convert ${ICON_SRC} -resize 16x16 cash-16.png
convert ${ICON_SRC} -resize 32x32 cash-32.png
convert ${ICON_SRC} -resize 48x48 cash-48.png
convert cash-16.png cash-32.png cash-48.png ${ICON_DST}
rm cash-16.png cash-32.png cash-48.png
