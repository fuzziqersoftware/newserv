#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

DIR="tests/saves-gci"

echo "... decrypt Ep1&2 charfile"
$EXECUTABLE decrypt-gci-save $DIR/8P-GPOJ-PSO_CHARACTER.gci $DIR/8P-GPOJ-PSO_CHARACTER.gcid --sys=$DIR/8P-GPOJ-PSO_SYSTEM.gci
diff $DIR/8P-GPOJ-PSO_CHARACTER-expected.gcid $DIR/8P-GPOJ-PSO_CHARACTER.gcid
echo "... decrypt Ep1&2 guildfile"
$EXECUTABLE decrypt-gci-save $DIR/8P-GPOJ-PSO_GUILDCARD.gci $DIR/8P-GPOJ-PSO_GUILDCARD.gcid --sys=$DIR/8P-GPOJ-PSO_SYSTEM.gci
diff $DIR/8P-GPOJ-PSO_GUILDCARD-expected.gcid $DIR/8P-GPOJ-PSO_GUILDCARD.gcid

echo "... decrypt Ep3 charfile"
$EXECUTABLE decrypt-gci-save $DIR/8P-GPSJ-PSO3_CHARACTER.gci $DIR/8P-GPSJ-PSO3_CHARACTER.gcid --sys=$DIR/8P-GPSJ-PSO3_SYSTEM.gci
diff $DIR/8P-GPSJ-PSO3_CHARACTER-expected.gcid $DIR/8P-GPSJ-PSO3_CHARACTER.gcid
echo "... decrypt Ep3 guildfile"
$EXECUTABLE decrypt-gci-save $DIR/8P-GPSJ-PSO3_GUILDCARD.gci $DIR/8P-GPSJ-PSO3_GUILDCARD.gcid --sys=$DIR/8P-GPSJ-PSO3_SYSTEM.gci
diff $DIR/8P-GPSJ-PSO3_GUILDCARD-expected.gcid $DIR/8P-GPSJ-PSO3_GUILDCARD.gcid

echo "... clean up"
rm -f $DIR/8P-GPOJ-PSO_CHARACTER.gcid $DIR/8P-GPOJ-PSO_GUILDCARD.gcid $DIR/8P-GPSJ-PSO3_CHARACTER.gcid $DIR/8P-GPSJ-PSO3_GUILDCARD.gcid
