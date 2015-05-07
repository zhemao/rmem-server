UBM_DIR=$(readlink -f $(dirname $0))
RMEM_DIR=$(readlink -f "$UBM_DIR/../..")
RAMC_DIR="$RMEM_DIR/evaluation/ramcloud"

CLIENT=f1
SERVER=f2
PORT=1234

$UBM_DIR/run-benchmarks-rm.sh
$UBM_DIR/run-benchmarks-rc.sh
