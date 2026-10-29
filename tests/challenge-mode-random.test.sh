#!/bin/sh

set -e

EXECUTABLE="$1"
if [ -z "$EXECUTABLE" ]; then
  EXECUTABLE="./newserv"
fi

echo "... challenge-ep1/c88101-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88101-gc.dat ./tests/c88101-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88101-gc-00000000.datd ./tests/c88101-gc-00000000.datd
rm ./tests/c88101-gc-00000000.datd

echo "... challenge-ep1/c88102-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88102-gc.dat ./tests/c88102-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88102-gc-00000000.datd ./tests/c88102-gc-00000000.datd
rm ./tests/c88102-gc-00000000.datd

echo "... challenge-ep1/c88103-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88103-gc.dat ./tests/c88103-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88103-gc-00000000.datd ./tests/c88103-gc-00000000.datd
rm ./tests/c88103-gc-00000000.datd

echo "... challenge-ep1/c88104-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88104-gc.dat ./tests/c88104-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88104-gc-00000000.datd ./tests/c88104-gc-00000000.datd
rm ./tests/c88104-gc-00000000.datd

echo "... challenge-ep1/c88105-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88105-gc.dat ./tests/c88105-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88105-gc-00000000.datd ./tests/c88105-gc-00000000.datd
rm ./tests/c88105-gc-00000000.datd

echo "... challenge-ep1/c88106-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88106-gc.dat ./tests/c88106-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88106-gc-00000000.datd ./tests/c88106-gc-00000000.datd
rm ./tests/c88106-gc-00000000.datd

echo "... challenge-ep1/c88107-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88107-gc.dat ./tests/c88107-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88107-gc-00000000.datd ./tests/c88107-gc-00000000.datd
rm ./tests/c88107-gc-00000000.datd

echo "... challenge-ep1/c88108-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88108-gc.dat ./tests/c88108-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88108-gc-00000000.datd ./tests/c88108-gc-00000000.datd
rm ./tests/c88108-gc-00000000.datd

echo "... challenge-ep1/c88109-gc.dat"
$EXECUTABLE materialize-map --decompressed system/quests/challenge-ep1/c88109-gc.dat ./tests/c88109-gc-00000000.datd --seed=00000000
diff tests/challenge-maps-materialized/c88109-gc-00000000.datd ./tests/c88109-gc-00000000.datd
rm ./tests/c88109-gc-00000000.datd
