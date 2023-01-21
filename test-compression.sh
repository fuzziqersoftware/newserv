#!/bin/sh

set -e

make

echo "... decompress card definitions"
./newserv decompress-prs system/ep3/card-definitions.mnr card-defs.mnrd
echo "... compress card definitions"
./newserv compress-prs card-defs.mnrd card-defs.mnr
echo "... check compressed card definitions"
./newserv decompress-prs card-defs.mnr - | diff card-defs.mnrd -

echo "... recompress executable with PRS"
./newserv compress-prs newserv newserv.prs
./newserv decompress-prs newserv.prs
diff newserv newserv.prs.dec

echo "... recompress executable with BC0"
./newserv compress-bc0 newserv newserv.bc0
./newserv decompress-bc0 newserv.bc0
diff newserv newserv.bc0.dec

echo "... clean up"
rm card-defs.mnrd card-defs.mnr newserv.prs newserv.prs.dec newserv.bc0 newserv.bc0.dec
