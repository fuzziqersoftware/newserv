#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

echo "... assemble system/quests/retrieval/q058-gc-e.bin.txt"
$EXECUTABLE assemble-quest-script --decompressed system/quests/retrieval/q058-gc-e.bin.txt tests/q058-gc-e-test.bind
diff tests/q058-gc-e-test.bind tests/q058-gc-e.bind

echo "... clean up"
rm tests/q058-gc-e-test.bind
