#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode LionelV1.vms"
$EXECUTABLE decode-vms tests/LionelV1.vms
diff tests/LionelV1.dec tests/LionelV1.vms.dec
echo "... decode LionelV2.vms"
$EXECUTABLE decode-vms tests/LionelV2.vms --seed=D0231610
diff tests/LionelV2.dec tests/LionelV2.vms.dec

echo "... clean up"
rm tests/*.vms.dec
