#!/bin/sh

set -e

SCHEME=$1

EXECUTABLE="$2"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decompress card definitions"
$EXECUTABLE decompress-prs system/ep3/card-definitions.mnr card-defs.mnrd
echo "... compress card definitions"
$EXECUTABLE compress-$SCHEME card-defs.mnrd card-defs.mnr.$SCHEME
echo "... check compressed card definitions"
$EXECUTABLE decompress-$SCHEME card-defs.mnr.$SCHEME - | diff card-defs.mnrd -

echo "... clean up"
rm card-defs.mnrd card-defs.mnr.$SCHEME
