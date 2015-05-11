UBM_DIR=$(readlink -f $(dirname $0))
RMEM_DIR=$(readlink -f "$UBM_DIR/../..")

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

PAGE_NUMS="1 10 20 50 100 200 500 1000 2000 5000 10000"

ARCH=$(uname -m)

for pn in $PAGE_NUMS; do
    printf "%d" $pn
    for trial in {1..3}; do
        start_rmem_server
        result=$(ssh $CLIENT "setarch $ARCH -R $UBM_DIR/commit-bm-rm $SERVER $PORT $pn" | tail -n 1)
        printf ",%f" $result
        stop_rmem_server
    done
    printf "\n"
done > commit-results-rm.csv

for pn in $PAGE_NUMS; do
    printf "%d" $pn
    for trial in {1..3}; do
        start_rmem_server
        result=$(ssh $CLIENT "setarch $ARCH -R $UBM_DIR/recovery-bm-rm $SERVER $PORT $pn" | tail -n 1)
        printf ",%f" $result
        stop_rmem_server
    done
    printf "\n"
done > recovery-results-rm.csv
