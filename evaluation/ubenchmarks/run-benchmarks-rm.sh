UBM_DIR=$(readlink -f $(dirname $0))
RMEM_DIR=$(readlink -f "$UBM_DIR/../..")
RAMC_DIR="$RMEM_DIR/evaluation/ramcloud"

CLIENT=f1
SERVER=f2
PORT=1234

function start_rmem_server {
    ssh -f $SERVER "$RMEM_DIR/rmem-server $PORT &> /dev/null"
    sleep 1
}

function stop_rmem_server {
    scp "$SERVER:/tmp/rmem-server-${PORT}.pid" /tmp/rmem-server.pid
    ssh $SERVER "kill `cat /tmp/rmem-server.pid`"
    sleep 1
}

for i in {1..20}; do
    printf "%d" $i
    for trial in {1..3}; do
        start_rmem_server
        result=$(ssh $CLIENT "$UBM_DIR/commit-bm-rm $SERVER $PORT $i" | tail -n 1)
        printf ",%f" $result
        stop_rmem_server
    done
    printf "\n"
done > commit-results-rm.csv

for i in {1..20}; do
    printf "%d" $i
    for trial in {1..3}; do
        start_rmem_server
        result=$(ssh $CLIENT "$UBM_DIR/recovery-bm-rm $SERVER $PORT $i" | tail -n 1)
        printf ",%f" $result
        stop_rmem_server
    done
    printf "\n"
done > recovery-results-rm.csv
