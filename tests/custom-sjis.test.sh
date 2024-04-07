#!/bin/sh

set -e

SCHEME=$1

EXECUTABLE="$2"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi

echo "... decode-sjis"
$EXECUTABLE decode-sjis tests/custom-sjis.txt tests/custom-sjis.utf8.txt
echo "... encode-sjis"
$EXECUTABLE encode-sjis tests/custom-sjis.utf8.txt tests/custom-sjis.recoded.txt

diff tests/custom-sjis.txt tests/custom-sjis.recoded.txt

echo "... clean up"
rm tests/custom-sjis.utf8.txt tests/custom-sjis.recoded.txt
