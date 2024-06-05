#!/bin/sh

set -e

SCHEME=$1

EXECUTABLE="$2"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

BASENAME="card-defs-test-$SCHEME"

echo "... decompress-prs"
$EXECUTABLE decompress-prs system/ep3/card-definitions.mnr $BASENAME.mnrd

echo "... compress with level=-1 (no compression)"
$EXECUTABLE compress-$SCHEME --compression-level=-1 $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.lN
echo "... compress with level=0"
$EXECUTABLE compress-$SCHEME --compression-level=0 $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.l0
echo "... compress with level=1"
$EXECUTABLE compress-$SCHEME --compression-level=1 $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.l1
echo "... compress optimally"
$EXECUTABLE compress-$SCHEME --optimal $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.lo
echo "... compress pessimally"
$EXECUTABLE compress-$SCHEME --pessimal $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.lp

echo "... decompress from level=-1 (no compression)"
$EXECUTABLE decompress-$SCHEME $BASENAME.mnrd.$SCHEME.lN $BASENAME.mnrd.$SCHEME.lN.dec
echo "... decompress from level=0"
$EXECUTABLE decompress-$SCHEME $BASENAME.mnrd.$SCHEME.l0 $BASENAME.mnrd.$SCHEME.l0.dec
echo "... decompress from level=1"
$EXECUTABLE decompress-$SCHEME $BASENAME.mnrd.$SCHEME.l1 $BASENAME.mnrd.$SCHEME.l1.dec
echo "... decompress from optimal"
$EXECUTABLE decompress-$SCHEME $BASENAME.mnrd.$SCHEME.lo $BASENAME.mnrd.$SCHEME.lo.dec
echo "... decompress from pessimal"
$EXECUTABLE decompress-$SCHEME $BASENAME.mnrd.$SCHEME.lp $BASENAME.mnrd.$SCHEME.lp.dec

echo "... check result from level=-1 (no compression)"
diff $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.lN.dec
echo "... check result from level=0"
diff $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.l0.dec
echo "... check result from level=1"
diff $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.l1.dec
echo "... check result from optimal"
diff $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.lo.dec
echo "... check result from pessimal"
diff $BASENAME.mnrd $BASENAME.mnrd.$SCHEME.lp.dec

echo "... clean up"
rm $BASENAME.mnrd \
    $BASENAME.mnrd.$SCHEME.lN \
    $BASENAME.mnrd.$SCHEME.l0 \
    $BASENAME.mnrd.$SCHEME.l1 \
    $BASENAME.mnrd.$SCHEME.lo \
    $BASENAME.mnrd.$SCHEME.lp \
    $BASENAME.mnrd.$SCHEME.lN.dec \
    $BASENAME.mnrd.$SCHEME.l0.dec \
    $BASENAME.mnrd.$SCHEME.l1.dec \
    $BASENAME.mnrd.$SCHEME.lo.dec \
    $BASENAME.mnrd.$SCHEME.lp.dec
