#!/bin/bash

cmd=$1
nsend=$2
traffic_type=$3
run_time=$4
on_duration=$5
off_duration=$6

case $traffic_type in
    exponential)
		params="traffic_params=exponential onduration=$on_duration offduration=$off_duration"
        ;;
    continuous)
		params="traffic_params=deterministic,num_cycles=1 onduration=$run_time offduration=1"
        ;;
    *)
		echo "Unidentified traffic_type. Control shouldn't reach here."
        ;;
esac

cmd="$cmd $params"

for (( i=0; i < $nsend; i++ )); do
		$cmd &
		pids="$pids $!"
done

sleep `expr $run_time / 1000`
kill $pids > /dev/null 2>&1
