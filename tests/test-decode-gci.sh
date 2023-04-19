#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode gci/quest-ep3.gci"
$EXECUTABLE decode-gci tests/gci/quest-ep3.gci
diff tests/gci/quest-ep3.dec tests/gci/quest-ep3.gci.dec
echo "... decode gci/quest-unencrypted.gci"
$EXECUTABLE decode-gci tests/gci/quest-unencrypted.gci
diff tests/gci/quest-unencrypted.dec tests/gci/quest-unencrypted.gci.dec
echo "... decode gci/quest-with-key.gci"
$EXECUTABLE decode-gci tests/gci/quest-with-key.gci
diff tests/gci/quest-with-key.dec tests/gci/quest-with-key.gci.dec
echo "... decode gci/quest-without-key.gci"
$EXECUTABLE decode-gci tests/gci/quest-without-key.gci --seed=1705B11E
diff tests/gci/quest-without-key.dec tests/gci/quest-without-key.gci.dec

echo "... re-encrypt gci/save-charfile.gci"
$EXECUTABLE encrypt-gci-save --sys=tests/gci/save-system.gci tests/gci/save-charfile.gcid tests/gci/save-charfile.gci
$EXECUTABLE decrypt-gci-save --sys=tests/gci/save-system.gci tests/gci/save-charfile.gci tests/gci/save-charfile-redec.gcid
hexdump -vC tests/gci/save-charfile.gcid > tests/gci/save-charfile.gcid.hex
hexdump -vC tests/gci/save-charfile-redec.gcid > tests/gci/save-charfile-redec.gcid.hex
# There should be differences on two lines: the checksum and the round2 seed
NUM_DIFF_LINES=$(diff -y --suppress-common-lines tests/gci/save-charfile.gcid.hex tests/gci/save-charfile-redec.gcid.hex | wc -l)
if [[ $NUM_DIFF_LINES -ne 2 ]]; then
  diff -U3 tests/gci/save-charfile.gcid.hex tests/gci/save-charfile-redec.gcid.hex
  exit 1
fi

echo "... clean up"
rm tests/gci/*.gci.dec tests/gci/save-charfile.gci tests/gci/save-charfile-redec.gcid tests/gci/*.hex
