#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR="tests/saves-gci-quest"

echo "... decode $DIR/quest-ep3.gci"
$EXECUTABLE decode-gci $DIR/quest-ep3.gci
diff $DIR/quest-ep3.dec $DIR/quest-ep3.gci.dec
echo "... decode $DIR/quest-unencrypted.gci"
$EXECUTABLE decode-gci $DIR/quest-unencrypted.gci
diff $DIR/quest-unencrypted.dec $DIR/quest-unencrypted.gci.dec
echo "... decode $DIR/quest-with-key.gci"
$EXECUTABLE decode-gci $DIR/quest-with-key.gci
diff $DIR/quest-with-key.dec $DIR/quest-with-key.gci.dec
echo "... decode $DIR/quest-without-key.gci"
$EXECUTABLE decode-gci $DIR/quest-without-key.gci --seed=1705B11E
diff $DIR/quest-without-key.dec $DIR/quest-without-key.gci.dec

echo "... clean up"
rm -f $DIR/*.gci.dec $DIR/*.gcid.hex
