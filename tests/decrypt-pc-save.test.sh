#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR="tests/saves-pc"

echo "... decrypt $DIR/pc_gud.bin"
$EXECUTABLE decrypt-pc-save $DIR/pc_gud.bin --seed=1705B11E
diff $DIR/pc_gud.dec $DIR/pc_gud.bin.dec
echo "... decrypt $DIR/pc_sys.bin"
$EXECUTABLE decrypt-pc-save $DIR/pc_sys.bin --seed=1705B11E
diff $DIR/pc_sys.dec $DIR/pc_sys.bin.dec

echo "... clean up"
rm $DIR/pc_*.bin.dec
