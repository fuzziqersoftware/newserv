#!/bin/sh

set -e

EXECUTABLE="$1"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decompress card definitions"
$EXECUTABLE decompress-prs system/ep3/card-definitions.mnr card-defs.mnrd
echo "... compress card definitions"
$EXECUTABLE compress-prs card-defs.mnrd card-defs.mnr
echo "... check compressed card definitions"
$EXECUTABLE decompress-prs card-defs.mnr - | diff card-defs.mnrd -

echo "... recompress executable with PRS"
$EXECUTABLE compress-prs $EXECUTABLE newserv.prs
$EXECUTABLE decompress-prs newserv.prs
diff $EXECUTABLE newserv.prs.dec

echo "... recompress executable with BC0"
$EXECUTABLE compress-bc0 $EXECUTABLE newserv.bc0
$EXECUTABLE decompress-bc0 newserv.bc0
diff $EXECUTABLE newserv.bc0.dec

echo "... clean up"
rm card-defs.mnrd card-defs.mnr newserv.prs newserv.prs.dec newserv.bc0 newserv.bc0.dec
