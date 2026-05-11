#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR=tests/item-parameter-tables

echo "... DC NTE"
$EXECUTABLE decode-item-parameter-table --dc-nte $DIR/dc-nte.expected.bin --decompressed $DIR/dc-nte.json --hex
$EXECUTABLE encode-item-parameter-table --dc-nte $DIR/dc-nte.json $DIR/dc-nte.encoded.bin --decompressed
bindiff $DIR/dc-nte.expected.bin $DIR/dc-nte.encoded.bin

echo "... DC 11/2000"
$EXECUTABLE decode-item-parameter-table --dc-11-2000 $DIR/dc-11-2000.expected.bin --decompressed $DIR/dc-11-2000.json --hex
$EXECUTABLE encode-item-parameter-table --dc-11-2000 $DIR/dc-11-2000.json $DIR/dc-11-2000.encoded.bin --decompressed
bindiff $DIR/dc-11-2000.expected.bin $DIR/dc-11-2000.encoded.bin

echo "... DC V1"
$EXECUTABLE decode-item-parameter-table --dc-v1 $DIR/dc-v1.expected.bin --decompressed $DIR/dc-v1.json --hex
$EXECUTABLE encode-item-parameter-table --dc-v1 $DIR/dc-v1.json $DIR/dc-v1.encoded.bin --decompressed
bindiff $DIR/dc-v1.expected.bin $DIR/dc-v1.encoded.bin

echo "... DC V2"
$EXECUTABLE decode-item-parameter-table --dc-v2 $DIR/dc-v2.expected.bin --decompressed $DIR/dc-v2.json --hex
$EXECUTABLE encode-item-parameter-table --dc-v2 $DIR/dc-v2.json $DIR/dc-v2.encoded.bin --decompressed
bindiff $DIR/dc-v2.expected.bin $DIR/dc-v2.encoded.bin

echo "... PC NTE"
$EXECUTABLE decode-item-parameter-table --pc-nte $DIR/pc-nte.expected.bin --decompressed $DIR/pc-nte.json --hex
$EXECUTABLE encode-item-parameter-table --pc-nte $DIR/pc-nte.json $DIR/pc-nte.encoded.bin --decompressed
bindiff $DIR/pc-nte.expected.bin $DIR/pc-nte.encoded.bin

echo "... PC V2"
$EXECUTABLE decode-item-parameter-table --pc-v2 $DIR/pc-v2.expected.bin --decompressed $DIR/pc-v2.json --hex
$EXECUTABLE encode-item-parameter-table --pc-v2 $DIR/pc-v2.json $DIR/pc-v2.encoded.bin --decompressed
bindiff $DIR/pc-v2.expected.bin $DIR/pc-v2.encoded.bin

echo "... GC NTE"
$EXECUTABLE decode-item-parameter-table --gc-nte $DIR/gc-nte.expected.bin --decompressed $DIR/gc-nte.json --hex
$EXECUTABLE encode-item-parameter-table --gc-nte $DIR/gc-nte.json $DIR/gc-nte.encoded.bin --decompressed
bindiff $DIR/gc-nte.expected.bin $DIR/gc-nte.encoded.bin

echo "... GC V3"
$EXECUTABLE decode-item-parameter-table --gc-v3 $DIR/gc-v3.expected.bin --decompressed $DIR/gc-v3.json --hex
$EXECUTABLE encode-item-parameter-table --gc-v3 $DIR/gc-v3.json $DIR/gc-v3.encoded.bin --decompressed
bindiff $DIR/gc-v3.expected.bin $DIR/gc-v3.encoded.bin

echo "... GC Ep3 NTE"
$EXECUTABLE decode-item-parameter-table --gc-ep3-nte $DIR/gc-ep3-nte.expected.bin --decompressed $DIR/gc-ep3-nte.json --hex
$EXECUTABLE encode-item-parameter-table --gc-ep3-nte $DIR/gc-ep3-nte.json $DIR/gc-ep3-nte.encoded.bin --decompressed
bindiff $DIR/gc-ep3-nte.expected.bin $DIR/gc-ep3-nte.encoded.bin

echo "... GC Ep3"
$EXECUTABLE decode-item-parameter-table --gc-ep3 $DIR/gc-ep3.expected.bin --decompressed $DIR/gc-ep3.json --hex
$EXECUTABLE encode-item-parameter-table --gc-ep3 $DIR/gc-ep3.json $DIR/gc-ep3.encoded.bin --decompressed
bindiff $DIR/gc-ep3.expected.bin $DIR/gc-ep3.encoded.bin

echo "... XB"
$EXECUTABLE decode-item-parameter-table --xb-v3 $DIR/xb-v3.expected.bin --decompressed $DIR/xb-v3.json --hex
$EXECUTABLE encode-item-parameter-table --xb-v3 $DIR/xb-v3.json $DIR/xb-v3.encoded.bin --decompressed
bindiff $DIR/xb-v3.expected.bin $DIR/xb-v3.encoded.bin

echo "... BB"
$EXECUTABLE decode-item-parameter-table --bb-v4 $DIR/bb-v4.expected.bin --decompressed $DIR/bb-v4.json --hex
$EXECUTABLE encode-item-parameter-table --bb-v4 $DIR/bb-v4.json $DIR/bb-v4.encoded.bin --decompressed
bindiff $DIR/bb-v4.expected.bin $DIR/bb-v4.encoded.bin

echo "... clean up"
rm -f tests/item-parameter-tables/*.encoded.bin tests/item-parameter-tables/*.json
