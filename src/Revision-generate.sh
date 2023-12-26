#!/bin/sh

OUTPUT_FILENAME="$1"
OUTPUT_DIRNAME=$(dirname $OUTPUT_FILENAME)

GIT_REVISION_HASH=$(git rev-parse --short HEAD)
TIMESTAMP_SECS=$(date +%s)

if [ -z "$GIT_REVISION_HASH" ]; then
  GIT_REVISION_HASH="????"
else
  if ! git diff-index --quiet HEAD -- ; then
    GIT_REVISION_HASH="$GIT_REVISION_HASH+"
  fi
fi

mkdir -p "$OUTPUT_DIRNAME"
cat > $OUTPUT_FILENAME <<EOF
#include "Revision.hh"

const char* GIT_REVISION_HASH = "$GIT_REVISION_HASH";
const uint64_t BUILD_TIMESTAMP = static_cast<uint64_t>($TIMESTAMP_SECS) * 1000000;
EOF
