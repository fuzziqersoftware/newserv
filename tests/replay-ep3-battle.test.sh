#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

$EXECUTABLE --config=tests/config.json replay-ep3-battle-record --compressed tests/replay-ep3-battle-input.mzr
