#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

$EXECUTABLE check-quest-opcodes
