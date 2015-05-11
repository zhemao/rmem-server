UBM_DIR=$(readlink -f $(dirname $0))
RMEM_DIR=$(readlink -f "$UBM_DIR/../..")
RAMC_DIR="$RMEM_DIR/evaluation/ramcloud"

CLIENT=f1
SERVER=f2
PORT=11100

function start_ramcloud {
    echo "$RAMC_DIR/manage_ramcloud.py start &> ramcloud.log"
    $RAMC_DIR/manage_ramcloud.py start &> ramcloud.log
    sleep 3
}

function stop_ramcloud {
    $RAMC_DIR/manage_ramcloud.py stop
    sleep 3
}

RAMC_LIB=/nscratch/joao/ramcloud/obj.master

for i in {1..20}; do
    printf "%d" $i
    for trial in {1..3}; do
        start_ramcloud
        result=$(ssh $CLIENT "LD_LIBRARY_PATH=$RAMC_LIB $UBM_DIR/commit-bm-rc $SERVER $PORT $i" | tail -n 1)
        printf ",%f" $result
        stop_ramcloud
    done
    printf "\n"
done > commit-results-rc.csv

for i in {1..20}; do
    printf "%d" $i
    for trial in {1..3}; do
        start_ramcloud
        result=$(ssh $CLIENT "LD_LIBRARY_PATH=$RAMC_LIB $UBM_DIR/recovery-bm-rc $SERVER $PORT $i" | tail -n 1)
        printf ",%f" $result
        stop_ramcloud
    done
    printf "\n"
done > recovery-results-rc.csv
