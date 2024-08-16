#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

echo "... assemble system/quests/retrieval/q058-gc-e.bin.txt"
$EXECUTABLE assemble-quest-script --optimal system/quests/retrieval/q058-gc-e.bin.txt tests/q058-gc-e-test.bin
diff tests/q058-gc-e-test.bin tests/q058-gc-e.bin

echo "... clean up"
rm tests/q058-gc-e-test.bin
