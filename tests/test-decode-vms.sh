#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode saves/lionel-v1.vms"
$EXECUTABLE decode-vms tests/saves/lionel-v1.vms
diff tests/saves/lionel-v1.dec tests/saves/lionel-v1.vms.dec
echo "... decode saves/lionel-v2.vms"
$EXECUTABLE decode-vms tests/saves/lionel-v2.vms --seed=D0231610
diff tests/saves/lionel-v2.dec tests/saves/lionel-v2.vms.dec

echo "... clean up"
rm tests/saves/*.vms.dec
