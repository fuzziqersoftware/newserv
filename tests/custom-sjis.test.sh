#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode-sjis"
$EXECUTABLE transcode-text --from=sjis --to=utf8 tests/custom-sjis.txt tests/custom-sjis.utf8.txt
echo "... encode-sjis"
$EXECUTABLE transcode-text --from=utf8 --to=sjis tests/custom-sjis.utf8.txt tests/custom-sjis.recoded.txt

diff tests/custom-sjis.txt tests/custom-sjis.recoded.txt

echo "... clean up"
rm tests/custom-sjis.utf8.txt tests/custom-sjis.recoded.txt
