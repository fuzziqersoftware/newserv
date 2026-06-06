#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR=tests/game-tables
PMT_PREFIX=$DIR/item-parameter-table
MMT_PREFIX=$DIR/mag-metadata-table

echo "... (armor-random-shop-set)"
$EXECUTABLE decode-armor-shop-random-set --big-endian $DIR/armor-shop-random-set.expected.bin $DIR/armor-shop-random-set.json
$EXECUTABLE encode-armor-shop-random-set --big-endian $DIR/armor-shop-random-set.json $DIR/armor-shop-random-set.encoded.bin
bindiff $DIR/armor-shop-random-set.expected.bin $DIR/armor-shop-random-set.encoded.bin

echo "... (tool-random-shop-set)"
$EXECUTABLE decode-tool-shop-random-set --big-endian $DIR/tool-shop-random-set.expected.bin $DIR/tool-shop-random-set.json
$EXECUTABLE encode-tool-shop-random-set --big-endian $DIR/tool-shop-random-set.json $DIR/tool-shop-random-set.encoded.bin
bindiff $DIR/tool-shop-random-set.expected.bin $DIR/tool-shop-random-set.encoded.bin

echo "... (weapon-random-shop-set-normal)"
$EXECUTABLE decode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-normal.expected.bin $DIR/weapon-shop-random-set-normal.json
$EXECUTABLE encode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-normal.json $DIR/weapon-shop-random-set-normal.encoded.bin
bindiff $DIR/weapon-shop-random-set-normal.expected.bin $DIR/weapon-shop-random-set-normal.encoded.bin

echo "... (weapon-random-shop-set-hard)"
$EXECUTABLE decode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-hard.expected.bin $DIR/weapon-shop-random-set-hard.json
$EXECUTABLE encode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-hard.json $DIR/weapon-shop-random-set-hard.encoded.bin
bindiff $DIR/weapon-shop-random-set-hard.expected.bin $DIR/weapon-shop-random-set-hard.encoded.bin

echo "... (weapon-random-shop-set-very-hard)"
$EXECUTABLE decode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-very-hard.expected.bin $DIR/weapon-shop-random-set-very-hard.json
$EXECUTABLE encode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-very-hard.json $DIR/weapon-shop-random-set-very-hard.encoded.bin
bindiff $DIR/weapon-shop-random-set-very-hard.expected.bin $DIR/weapon-shop-random-set-very-hard.encoded.bin

echo "... (weapon-random-shop-set-ultimate)"
$EXECUTABLE decode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-ultimate.expected.bin $DIR/weapon-shop-random-set-ultimate.json
$EXECUTABLE encode-weapon-shop-random-set --big-endian $DIR/weapon-shop-random-set-ultimate.json $DIR/weapon-shop-random-set-ultimate.encoded.bin
bindiff $DIR/weapon-shop-random-set-ultimate.expected.bin $DIR/weapon-shop-random-set-ultimate.encoded.bin

echo "... (tekker-adjustment-set)"
$EXECUTABLE decode-tekker-adjustment-set --big-endian $DIR/tekker-adjustment-set.expected.bin $DIR/tekker-adjustment-set.json
$EXECUTABLE encode-tekker-adjustment-set --big-endian $DIR/tekker-adjustment-set.json $DIR/tekker-adjustment-set.encoded.bin
bindiff $DIR/tekker-adjustment-set.expected.bin $DIR/tekker-adjustment-set.encoded.bin

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

echo "... (mag-metadata-table) DC 11/2000"
$EXECUTABLE decode-mag-metadata-table --dc-11-2000 $MMT_PREFIX-dc-11-2000.expected.bin --decompressed $MMT_PREFIX-dc-11-2000.json --hex
$EXECUTABLE encode-mag-metadata-table --dc-11-2000 $MMT_PREFIX-dc-11-2000.json $MMT_PREFIX-dc-11-2000.encoded.bin --decompressed
bindiff $MMT_PREFIX-dc-11-2000.expected.bin $MMT_PREFIX-dc-11-2000.encoded.bin

echo "... (mag-metadata-table) DC V1"
$EXECUTABLE decode-mag-metadata-table --dc-v1 $MMT_PREFIX-dc-v1.expected.bin --decompressed $MMT_PREFIX-dc-v1.json --hex
$EXECUTABLE encode-mag-metadata-table --dc-v1 $MMT_PREFIX-dc-v1.json $MMT_PREFIX-dc-v1.encoded.bin --decompressed
bindiff $MMT_PREFIX-dc-v1.expected.bin $MMT_PREFIX-dc-v1.encoded.bin

echo "... (mag-metadata-table) DC V2"
$EXECUTABLE decode-mag-metadata-table --dc-v2 $MMT_PREFIX-dc-v2.expected.bin --decompressed $MMT_PREFIX-dc-v2.json --hex
$EXECUTABLE encode-mag-metadata-table --dc-v2 $MMT_PREFIX-dc-v2.json $MMT_PREFIX-dc-v2.encoded.bin --decompressed
bindiff $MMT_PREFIX-dc-v2.expected.bin $MMT_PREFIX-dc-v2.encoded.bin

echo "... (mag-metadata-table) PC NTE"
$EXECUTABLE decode-mag-metadata-table --pc-nte $MMT_PREFIX-pc-nte.expected.bin --decompressed $MMT_PREFIX-pc-nte.json --hex
$EXECUTABLE encode-mag-metadata-table --pc-nte $MMT_PREFIX-pc-nte.json $MMT_PREFIX-pc-nte.encoded.bin --decompressed
bindiff $MMT_PREFIX-pc-nte.expected.bin $MMT_PREFIX-pc-nte.encoded.bin

echo "... (mag-metadata-table) PC V2"
$EXECUTABLE decode-mag-metadata-table --pc-v2 $MMT_PREFIX-pc-v2.expected.bin --decompressed $MMT_PREFIX-pc-v2.json --hex
$EXECUTABLE encode-mag-metadata-table --pc-v2 $MMT_PREFIX-pc-v2.json $MMT_PREFIX-pc-v2.encoded.bin --decompressed
bindiff $MMT_PREFIX-pc-v2.expected.bin $MMT_PREFIX-pc-v2.encoded.bin

echo "... (mag-metadata-table) GC NTE"
$EXECUTABLE decode-mag-metadata-table --gc-nte $MMT_PREFIX-gc-nte.expected.bin --decompressed $MMT_PREFIX-gc-nte.json --hex
$EXECUTABLE encode-mag-metadata-table --gc-nte $MMT_PREFIX-gc-nte.json $MMT_PREFIX-gc-nte.encoded.bin --decompressed
bindiff $MMT_PREFIX-gc-nte.expected.bin $MMT_PREFIX-gc-nte.encoded.bin

echo "... (mag-metadata-table) GC V3"
$EXECUTABLE decode-mag-metadata-table --gc-v3 $MMT_PREFIX-gc-v3.expected.bin --decompressed $MMT_PREFIX-gc-v3.json --hex
$EXECUTABLE encode-mag-metadata-table --gc-v3 $MMT_PREFIX-gc-v3.json $MMT_PREFIX-gc-v3.encoded.bin --decompressed
bindiff $MMT_PREFIX-gc-v3.expected.bin $MMT_PREFIX-gc-v3.encoded.bin

echo "... (mag-metadata-table) GC Ep3 NTE"
$EXECUTABLE decode-mag-metadata-table --gc-ep3-nte $MMT_PREFIX-gc-ep3-nte.expected.bin --decompressed $MMT_PREFIX-gc-ep3-nte.json --hex
$EXECUTABLE encode-mag-metadata-table --gc-ep3-nte $MMT_PREFIX-gc-ep3-nte.json $MMT_PREFIX-gc-ep3-nte.encoded.bin --decompressed
bindiff $MMT_PREFIX-gc-ep3-nte.expected.bin $MMT_PREFIX-gc-ep3-nte.encoded.bin

echo "... (mag-metadata-table) GC Ep3"
$EXECUTABLE decode-mag-metadata-table --gc-ep3 $MMT_PREFIX-gc-ep3.expected.bin --decompressed $MMT_PREFIX-gc-ep3.json --hex
$EXECUTABLE encode-mag-metadata-table --gc-ep3 $MMT_PREFIX-gc-ep3.json $MMT_PREFIX-gc-ep3.encoded.bin --decompressed
bindiff $MMT_PREFIX-gc-ep3.expected.bin $MMT_PREFIX-gc-ep3.encoded.bin

echo "... (mag-metadata-table) XB"
$EXECUTABLE decode-mag-metadata-table --xb-v3 $MMT_PREFIX-xb-v3.expected.bin --decompressed $MMT_PREFIX-xb-v3.json --hex
$EXECUTABLE encode-mag-metadata-table --xb-v3 $MMT_PREFIX-xb-v3.json $MMT_PREFIX-xb-v3.encoded.bin --decompressed
bindiff $MMT_PREFIX-xb-v3.expected.bin $MMT_PREFIX-xb-v3.encoded.bin

echo "... (mag-metadata-table) BB"
$EXECUTABLE decode-mag-metadata-table --bb-v4 $MMT_PREFIX-bb-v4.expected.bin --decompressed $MMT_PREFIX-bb-v4.json --hex
$EXECUTABLE encode-mag-metadata-table --bb-v4 $MMT_PREFIX-bb-v4.json $MMT_PREFIX-bb-v4.encoded.bin --decompressed
bindiff $MMT_PREFIX-bb-v4.expected.bin $MMT_PREFIX-bb-v4.encoded.bin

echo "... clean up"
rm -f $DIR/*.encoded.bin $DIR/*.json $DIR/battle-params.json.enc* $DIR/battle-params-encoded*
