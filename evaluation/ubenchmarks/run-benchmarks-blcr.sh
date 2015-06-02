PAGE_NUMS="1 10 20 50 100 200 500 1000 2000 5000 10000"

COMMIT_FILE=commit-results-blcr.csv
RECOVERY_FILE=recovery-results-blcr.csv

rm $COMMIT_FILE
rm $RECOVERY_FILE

for pn in $PAGE_NUMS; do
    echo $pn
    printf "%d" $pn >> $COMMIT_FILE
    printf "%d" $pn >> $RECOVERY_FILE

    for trial in {1..3}; do
        commit_result=$(cr_run ./blcr-bm $pn)
        /usr/bin/time -f %e -o rec-res.txt cr_restart gen.blcr > /dev/null
        recovery_result=$(cat rec-res.txt)
        printf ",%f" $commit_result >> $COMMIT_FILE
        printf ",%f" $recovery_result >> $RECOVERY_FILE
    done
    printf "\n" >> $COMMIT_FILE
    printf "\n" >> $RECOVERY_FILE
done
