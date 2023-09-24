#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decrypt saves/pc_gud.bin"
$EXECUTABLE decrypt-pc-save tests/saves/pc_gud.bin --seed=1705B11E
diff tests/saves/pc_gud.dec tests/saves/pc_gud.bin.dec
echo "... decrypt saves/pc_sys.bin"
$EXECUTABLE decrypt-pc-save tests/saves/pc_sys.bin --seed=1705B11E
diff tests/saves/pc_sys.dec tests/saves/pc_sys.bin.dec

echo "... clean up"
rm tests/saves/pc_*.bin.dec
