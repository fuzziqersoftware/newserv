#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode saves/quest-ep3.gci"
$EXECUTABLE decode-gci tests/saves/quest-ep3.gci
diff tests/saves/quest-ep3.dec tests/saves/quest-ep3.gci.dec
echo "... decode saves/quest-unencrypted.gci"
$EXECUTABLE decode-gci tests/saves/quest-unencrypted.gci
diff tests/saves/quest-unencrypted.dec tests/saves/quest-unencrypted.gci.dec
echo "... decode saves/quest-with-key.gci"
$EXECUTABLE decode-gci tests/saves/quest-with-key.gci
diff tests/saves/quest-with-key.dec tests/saves/quest-with-key.gci.dec
echo "... decode saves/quest-without-key.gci"
$EXECUTABLE decode-gci tests/saves/quest-without-key.gci --seed=1705B11E
diff tests/saves/quest-without-key.dec tests/saves/quest-without-key.gci.dec

echo "... re-encrypt saves/save-charfile.gci"
$EXECUTABLE encrypt-gci-save --sys=tests/saves/save-system.gci tests/saves/save-charfile.gcid tests/saves/save-charfile.gci
$EXECUTABLE decrypt-gci-save --sys=tests/saves/save-system.gci tests/saves/save-charfile.gci tests/saves/save-charfile-redec.gcid
hexdump -vC tests/saves/save-charfile.gcid > tests/saves/save-charfile.gcid.hex
hexdump -vC tests/saves/save-charfile-redec.gcid > tests/saves/save-charfile-redec.gcid.hex
# There should be differences on two lines: the checksum and the round2 seed
NUM_DIFF_LINES=$(diff -y --suppress-common-lines tests/saves/save-charfile.gcid.hex tests/saves/save-charfile-redec.gcid.hex | wc -l)
if [[ $NUM_DIFF_LINES -ne 2 ]]; then
  diff -U3 tests/saves/save-charfile.gcid.hex tests/saves/save-charfile-redec.gcid.hex
  exit 1
fi

echo "... clean up"
rm tests/saves/*.gci.dec tests/saves/save-charfile.gci tests/saves/save-charfile-redec.gcid tests/saves/*.gcid.hex
