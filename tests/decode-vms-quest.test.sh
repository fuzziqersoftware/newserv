#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR=tests/saves-vms-quest

echo "... decode $DIR/lionel-v1.vms"
$EXECUTABLE decode-vms $DIR/lionel-v1.vms
diff $DIR/lionel-v1.dec $DIR/lionel-v1.vms.dec
echo "... decode $DIR/lionel-v2.vms"
$EXECUTABLE decode-vms $DIR/lionel-v2.vms --seed=D0231610
diff $DIR/lionel-v2.dec $DIR/lionel-v2.vms.dec

echo "... clean up"
rm $DIR/*.vms.dec
