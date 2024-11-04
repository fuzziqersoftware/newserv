#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR="tests/saves-vms"

echo "... decrypt v1 charfile"
$EXECUTABLE decrypt-vms-save $DIR/d1-sys.vms $DIR/d1-sys.vmsd --serial-number=77777777
diff $DIR/d1-sys-expected.vmsd $DIR/d1-sys.vmsd

echo "... decrypt v2 charfile"
$EXECUTABLE decrypt-vms-save $DIR/e1-sys.vms $DIR/e1-sys.vmsd --serial-number=DBA61FAC
diff $DIR/e1-sys-expected.vmsd $DIR/e1-sys.vmsd

echo "... decrypt v1/v2 guildfile"
$EXECUTABLE decrypt-vms-save $DIR/a1-2gc.vms $DIR/a1-2gc.vmsd --serial-number=DBA61FAC
diff $DIR/a1-2gc-expected.vmsd $DIR/a1-2gc.vmsd

echo "... clean up"
rm -f $DIR/d1-sys.vmsd $DIR/e1-sys.vmsd $DIR/a1-2gc.vmsd
