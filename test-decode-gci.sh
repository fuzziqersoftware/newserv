#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode GCIEpisode3.gci"
$EXECUTABLE decode-gci tests/GCIEpisode3.gci
diff tests/GCIEpisode3.dec tests/GCIEpisode3.gci.dec
echo "... decode GCIWithoutEncryption.gci"
$EXECUTABLE decode-gci tests/GCIWithoutEncryption.gci
diff tests/GCIWithoutEncryption.dec tests/GCIWithoutEncryption.gci.dec
echo "... decode GCIWithEmbeddedKey.gci"
$EXECUTABLE decode-gci tests/GCIWithEmbeddedKey.gci
diff tests/GCIWithEmbeddedKey.dec tests/GCIWithEmbeddedKey.gci.dec
echo "... decode GCIWithoutEmbeddedKey.gci"
$EXECUTABLE decode-gci tests/GCIWithoutEmbeddedKey.gci --seed=1705B11E
diff tests/GCIWithoutEmbeddedKey.dec tests/GCIWithoutEmbeddedKey.gci.dec

echo "... re-encrypt GCICharFile.gci"
./newserv encrypt-gci-save --sys=tests/GCISystemFile.gci tests/GCICharFile.gcid tests/GCICharFile.gci
./newserv decrypt-gci-save --sys=tests/GCISystemFile.gci tests/GCICharFile.gci tests/GCICharFile-redec.gcid
hexdump -vC tests/GCICharFile.gcid > tests/GCICharFile.gcid.hex
hexdump -vC tests/GCICharFile-redec.gcid > tests/GCICharFile-redec.gcid.hex
# There should be differences on two lines: the checksum and the round2 seed
NUM_DIFF_LINES=$(diff -y --suppress-common-lines tests/GCICharFile.gcid.hex tests/GCICharFile-redec.gcid.hex | wc -l)
if [[ $NUM_DIFF_LINES -ne 2 ]]; then
  diff -U3 tests/GCICharFile.gcid.hex tests/GCICharFile-redec.gcid.hex
  exit 1
fi

echo "... clean up"
rm tests/*.gci.dec tests/GCICharFile.gci tests/GCICharFile-redec.gcid tests/*.hex
