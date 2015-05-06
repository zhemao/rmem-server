UBM_DIR=$(readlink -f $(dirname $0))
RMEM_DIR=$(readlink -f "$UBM_DIR/../..")

CLIENT=f1
SERVER=f2
PORT=1234

function start_server {
    ssh -f $SERVER "$RMEM_DIR/rmem-server $PORT &> /dev/null"
    sleep 1
}

function stop_server {
    ssh $SERVER 'kill `cat /tmp/rmem-server.pid`'
    # wait for server process to die
    while true; do
        SERVER_PID=$(lsof -t -i TCP@$SERVER:22)
        if [ -n $SERVER_PID ]; then
            break
        fi
    done
}

for i in {1..20}; do
    printf "%d" $i
    for trial in {1..3}; do
        start_server
        result=$(ssh $CLIENT "$UBM_DIR/commit-bm-rm $SERVER $PORT $i" | tail -n 1)
        printf ",%f" $result
        stop_server
    done
    printf "\n"
done > commit-results.csv

for i in {1..20}; do
    printf "%d" $i
    for trial in {1..3}; do
        start_server
        result=$(ssh $CLIENT "$UBM_DIR/recovery-bm-rm $SERVER $PORT $i" | tail -n 1)
        printf ",%f" $result
        stop_server
    done
    printf "\n"
done > recovery-results.csv
