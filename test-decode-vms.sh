#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode vms/lionel-v1.vms"
$EXECUTABLE decode-vms tests/vms/lionel-v1.vms
diff tests/vms/lionel-v1.dec tests/vms/lionel-v1.vms.dec
echo "... decode vms/lionel-v2.vms"
$EXECUTABLE decode-vms tests/vms/lionel-v2.vms --seed=D0231610
diff tests/vms/lionel-v2.dec tests/vms/lionel-v2.vms.dec

echo "... clean up"
rm tests/vms/*.vms.dec
