#!/bin/sh

set -e

SCHEME=$1

EXECUTABLE="$2"
if [ "$EXECUTABLE" == "" ]; then
  EXECUTABLE="./newserv"
fi


echo "... decompress"
$EXECUTABLE decompress-prs system/ep3/card-definitions.mnr card-defs.mnrd

echo "... compress with level=-1 (no compression)"
$EXECUTABLE compress-$SCHEME --compression-level=-1 card-defs.mnrd card-defs.mnrd.$SCHEME.lN
echo "... compress with level=0"
$EXECUTABLE compress-$SCHEME --compression-level=0 card-defs.mnrd card-defs.mnrd.$SCHEME.l0
echo "... compress with level=1"
$EXECUTABLE compress-$SCHEME --compression-level=1 card-defs.mnrd card-defs.mnrd.$SCHEME.l1
echo "... compress optimally"
$EXECUTABLE compress-$SCHEME --optimal card-defs.mnrd card-defs.mnrd.$SCHEME.opt

echo "... decompress from level=-1 (no compression)"
$EXECUTABLE decompress-$SCHEME card-defs.mnrd.$SCHEME.lN card-defs.mnrd.$SCHEME.lN.dec
echo "... decompress from level=0"
$EXECUTABLE decompress-$SCHEME card-defs.mnrd.$SCHEME.l0 card-defs.mnrd.$SCHEME.l0.dec
echo "... decompress from level=1"
$EXECUTABLE decompress-$SCHEME card-defs.mnrd.$SCHEME.l1 card-defs.mnrd.$SCHEME.l1.dec
echo "... decompress from optimal"
$EXECUTABLE decompress-$SCHEME card-defs.mnrd.$SCHEME.opt card-defs.mnrd.$SCHEME.opt.dec

echo "... check result from level=-1 (no compression)"
diff card-defs.mnrd card-defs.mnrd.$SCHEME.lN.dec
echo "... check result from level=0"
diff card-defs.mnrd card-defs.mnrd.$SCHEME.l0.dec
echo "... check result from level=1"
diff card-defs.mnrd card-defs.mnrd.$SCHEME.l1.dec
echo "... check result from optimal"
diff card-defs.mnrd card-defs.mnrd.$SCHEME.opt.dec

echo "... clean up"
rm card-defs.mnrd \
    card-defs.mnrd.$SCHEME.lN \
    card-defs.mnrd.$SCHEME.l0 \
    card-defs.mnrd.$SCHEME.l1 \
    card-defs.mnrd.$SCHEME.opt \
    card-defs.mnrd.$SCHEME.lN.dec \
    card-defs.mnrd.$SCHEME.l0.dec \
    card-defs.mnrd.$SCHEME.l1.dec \
    card-defs.mnrd.$SCHEME.opt.dec
