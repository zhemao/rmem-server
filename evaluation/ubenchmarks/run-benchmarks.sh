UBM_DIR=$(readlink -f $(dirname $0))
RMEM_DIR=$(readlink -f "$UBM_DIR/../..")

CLIENT=f1
SERVER=f2
PORT=1234

for i in {1..20}; do
    ssh -f $SERVER "$RMEM_DIR/rmem-server $PORT &> /dev/null"
    sleep 1
    ssh $CLIENT "$UBM_DIR/commit-bm $SERVER $PORT $i" | tail -n 1
    ssh $SERVER 'kill `cat /tmp/rmem-server.pid`'
    # wait for server process to die
    while true; do
        SERVER_PID=$(lsof -t -i TCP@$SERVER:22)
        if [ -n $SERVER_PID ]; then
            break
        fi
    done
done > commit-results.csv

for i in {1..20}; do
    ssh -f $SERVER "$RMEM_DIR/rmem-server $PORT &> /dev/null"
    sleep 1
    ssh $CLIENT "$UBM_DIR/recovery-bm $SERVER $PORT $i" | tail -n 1
    ssh $SERVER 'kill `cat /tmp/rmem-server.pid`'
    # wait for server process to die
    while true; do
        SERVER_PID=$(lsof -t -i TCP@$SERVER:22)
        if [ -n $SERVER_PID ]; then
            break
        fi
    done
done > recovery-results.csv
