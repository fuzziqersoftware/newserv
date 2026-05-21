#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR=tests/game-tables
PMT_PREFIX=$DIR/item-parameter-table

echo "... (battle-params)"
$EXECUTABLE decode-battle-params tests/game-tables/battle-params-ep1-on.dat tests/game-tables/battle-params-ep2-on.dat tests/game-tables/battle-params-ep4-on.dat tests/game-tables/battle-params-ep1-off.dat tests/game-tables/battle-params-ep2-off.dat tests/game-tables/battle-params-ep4-off.dat tests/game-tables/battle-params.json
$EXECUTABLE encode-battle-params tests/game-tables/battle-params.json tests/game-tables/battle-params-encoded
bindiff tests/game-tables/battle-params-ep1-on.dat tests/game-tables/battle-params-encoded_on.dat
bindiff tests/game-tables/battle-params-ep2-on.dat tests/game-tables/battle-params-encoded_lab_on.dat
bindiff tests/game-tables/battle-params-ep4-on.dat tests/game-tables/battle-params-encoded_ep4_on.dat
bindiff tests/game-tables/battle-params-ep1-off.dat tests/game-tables/battle-params-encoded.dat
bindiff tests/game-tables/battle-params-ep2-off.dat tests/game-tables/battle-params-encoded_lab.dat
bindiff tests/game-tables/battle-params-ep4-off.dat tests/game-tables/battle-params-encoded_ep4.dat

echo "... (level-table) BB"
$EXECUTABLE decode-level-table --bb-v4 $DIR/level-table-bb-v4.expected.bin --decompressed $DIR/level-table-bb-v4.json --hex
$EXECUTABLE encode-level-table-v4 $DIR/level-table-bb-v4.json $DIR/level-table-bb-v4.encoded.bin --decompressed
bindiff $DIR/level-table-bb-v4.expected.bin $DIR/level-table-bb-v4.encoded.bin

echo "... (item-parameter-table) DC NTE"
$EXECUTABLE decode-item-parameter-table --dc-nte $PMT_PREFIX-dc-nte.expected.bin --decompressed $PMT_PREFIX-dc-nte.json --hex
$EXECUTABLE encode-item-parameter-table --dc-nte $PMT_PREFIX-dc-nte.json $PMT_PREFIX-dc-nte.encoded.bin --decompressed
bindiff $PMT_PREFIX-dc-nte.expected.bin $PMT_PREFIX-dc-nte.encoded.bin

echo "... (item-parameter-table) DC 11/2000"
$EXECUTABLE decode-item-parameter-table --dc-11-2000 $PMT_PREFIX-dc-11-2000.expected.bin --decompressed $PMT_PREFIX-dc-11-2000.json --hex
$EXECUTABLE encode-item-parameter-table --dc-11-2000 $PMT_PREFIX-dc-11-2000.json $PMT_PREFIX-dc-11-2000.encoded.bin --decompressed
bindiff $PMT_PREFIX-dc-11-2000.expected.bin $PMT_PREFIX-dc-11-2000.encoded.bin

echo "... (item-parameter-table) DC V1"
$EXECUTABLE decode-item-parameter-table --dc-v1 $PMT_PREFIX-dc-v1.expected.bin --decompressed $PMT_PREFIX-dc-v1.json --hex
$EXECUTABLE encode-item-parameter-table --dc-v1 $PMT_PREFIX-dc-v1.json $PMT_PREFIX-dc-v1.encoded.bin --decompressed
bindiff $PMT_PREFIX-dc-v1.expected.bin $PMT_PREFIX-dc-v1.encoded.bin

echo "... (item-parameter-table) DC V2"
$EXECUTABLE decode-item-parameter-table --dc-v2 $PMT_PREFIX-dc-v2.expected.bin --decompressed $PMT_PREFIX-dc-v2.json --hex
$EXECUTABLE encode-item-parameter-table --dc-v2 $PMT_PREFIX-dc-v2.json $PMT_PREFIX-dc-v2.encoded.bin --decompressed
bindiff $PMT_PREFIX-dc-v2.expected.bin $PMT_PREFIX-dc-v2.encoded.bin

echo "... (item-parameter-table) PC NTE"
$EXECUTABLE decode-item-parameter-table --pc-nte $PMT_PREFIX-pc-nte.expected.bin --decompressed $PMT_PREFIX-pc-nte.json --hex
$EXECUTABLE encode-item-parameter-table --pc-nte $PMT_PREFIX-pc-nte.json $PMT_PREFIX-pc-nte.encoded.bin --decompressed
bindiff $PMT_PREFIX-pc-nte.expected.bin $PMT_PREFIX-pc-nte.encoded.bin

echo "... (item-parameter-table) PC V2"
$EXECUTABLE decode-item-parameter-table --pc-v2 $PMT_PREFIX-pc-v2.expected.bin --decompressed $PMT_PREFIX-pc-v2.json --hex
$EXECUTABLE encode-item-parameter-table --pc-v2 $PMT_PREFIX-pc-v2.json $PMT_PREFIX-pc-v2.encoded.bin --decompressed
bindiff $PMT_PREFIX-pc-v2.expected.bin $PMT_PREFIX-pc-v2.encoded.bin

echo "... (item-parameter-table) GC NTE"
$EXECUTABLE decode-item-parameter-table --gc-nte $PMT_PREFIX-gc-nte.expected.bin --decompressed $PMT_PREFIX-gc-nte.json --hex
$EXECUTABLE encode-item-parameter-table --gc-nte $PMT_PREFIX-gc-nte.json $PMT_PREFIX-gc-nte.encoded.bin --decompressed
bindiff $PMT_PREFIX-gc-nte.expected.bin $PMT_PREFIX-gc-nte.encoded.bin

echo "... (item-parameter-table) GC V3"
$EXECUTABLE decode-item-parameter-table --gc-v3 $PMT_PREFIX-gc-v3.expected.bin --decompressed $PMT_PREFIX-gc-v3.json --hex
$EXECUTABLE encode-item-parameter-table --gc-v3 $PMT_PREFIX-gc-v3.json $PMT_PREFIX-gc-v3.encoded.bin --decompressed
bindiff $PMT_PREFIX-gc-v3.expected.bin $PMT_PREFIX-gc-v3.encoded.bin

echo "... (item-parameter-table) GC Ep3 NTE"
$EXECUTABLE decode-item-parameter-table --gc-ep3-nte $PMT_PREFIX-gc-ep3-nte.expected.bin --decompressed $PMT_PREFIX-gc-ep3-nte.json --hex
$EXECUTABLE encode-item-parameter-table --gc-ep3-nte $PMT_PREFIX-gc-ep3-nte.json $PMT_PREFIX-gc-ep3-nte.encoded.bin --decompressed
bindiff $PMT_PREFIX-gc-ep3-nte.expected.bin $PMT_PREFIX-gc-ep3-nte.encoded.bin

echo "... (item-parameter-table) GC Ep3"
$EXECUTABLE decode-item-parameter-table --gc-ep3 $PMT_PREFIX-gc-ep3.expected.bin --decompressed $PMT_PREFIX-gc-ep3.json --hex
$EXECUTABLE encode-item-parameter-table --gc-ep3 $PMT_PREFIX-gc-ep3.json $PMT_PREFIX-gc-ep3.encoded.bin --decompressed
bindiff $PMT_PREFIX-gc-ep3.expected.bin $PMT_PREFIX-gc-ep3.encoded.bin

echo "... (item-parameter-table) XB"
$EXECUTABLE decode-item-parameter-table --xb-v3 $PMT_PREFIX-xb-v3.expected.bin --decompressed $PMT_PREFIX-xb-v3.json --hex
$EXECUTABLE encode-item-parameter-table --xb-v3 $PMT_PREFIX-xb-v3.json $PMT_PREFIX-xb-v3.encoded.bin --decompressed
bindiff $PMT_PREFIX-xb-v3.expected.bin $PMT_PREFIX-xb-v3.encoded.bin

echo "... (item-parameter-table) BB"
$EXECUTABLE decode-item-parameter-table --bb-v4 $PMT_PREFIX-bb-v4.expected.bin --decompressed $PMT_PREFIX-bb-v4.json --hex
$EXECUTABLE encode-item-parameter-table --bb-v4 $PMT_PREFIX-bb-v4.json $PMT_PREFIX-bb-v4.encoded.bin --decompressed
bindiff $PMT_PREFIX-bb-v4.expected.bin $PMT_PREFIX-bb-v4.encoded.bin

echo "... clean up"
rm -f $DIR/*.encoded.bin $DIR/*.json $DIR/battle-params.json.enc* $DIR/battle-params-encoded*
