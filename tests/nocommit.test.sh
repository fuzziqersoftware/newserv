#!/bin/sh

set -e

grep -nHR NOCOMMIT src/ || exit 0
echo "Failed: one or more NOCOMMIT comments were found"
exit 1
