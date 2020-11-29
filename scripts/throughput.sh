#!/bin/bash

case $1 in
    run)
        shift

        if [[ $# -ne 5 ]];
        then
            echo "Usage : sudo bash $0 run <output directory> <intf> <ip> <flows> <sender binary>"
            exit
        fi

        output=$1
        intf=$2
        rip=$3
        flows=$4
        sender=$5
        intersend=1 # In seconds

        mkdir -p $output

        for tcp in "eyenet" "cubic";
        do
            tcpdump -i $intf -n -w $output/$tcp-pcap-trace &

            sender_pids=""
            for (( i=0; i < $flows; i++ ));
            do
                onduration=`expr 2 \* $intersend \* \( $flows - $i - 1 \) + 1`
                if [[ $tcp == "cubic" ]];
                then
                    iperf -c $rip -Z $tcp -t $onduration -p 5001 &
                    sender_pids="$sender_pids $!"
                elif [[ $tcp == "eyenet" ]];
                then
                    $sender cctype=eyenet serverip=$rip offduration=0 traffic_params=deterministic,num_cycles=1 delta_conf=do_ss:auto:0.5 onduration=`expr $onduration \* 1000`  &
                    sender_pids="$sender_pids $!"
                fi
                sleep $intersend
            done

            sleep `expr $flows \* $intersend - 1`

            kill $sender_pids > /dev/null 2>&1
            pkill tcpdump > /dev/null 2>&1
        done
        ;;

    graph)
        shift

        if [[ $# -ne 1 ]];
        then
            echo "Usage : sudo bash $0 graph <output directory>"
            exit
        fi

        output=$1

        for inp in $output/*-pcap-trace;
        do
            python3 pcap-tpt-graph.py $inp
        done

        tputplot=$output/$output-tput-plot
        cat > $tputplot.gnuplot <<- EOM
set output "${tputplot}.svg"
set title "Throughput : Eyenet vs Cubic"
set style fill transparent solid 0.5 noborder
set xlabel "Time (s)"
set ylabel "Throughput (Mbits/s)"
set xrange [0:19]
set yrange [1:100]
set logscale y

plot '${output}/cubic-pcap-trace-tptpoly.dat' using 1:2 with filledcurves title 'Cubic' lt 7, '${output}/eyenet-pcap-trace-tptpoly.dat' using 1:2 with filledcurves title 'Eyenet', 'Ideal' using 1:2 with lines lt 1 title "Ideal"
EOM

        gnuplot -p $tputplot.gnuplot

        jainplot=$output/$output-jain-plot
        cat > $jainplot.gnuplot <<- EOM
set output "${jainplot}.svg"
set title "Jain Index : Eyenet vs Cubic"
set xlabel "Time (s)"
set ylabel "Jain Index"
set xrange [0:20]
set yrange [0:1.5]
set logscale y

plot '${output}/cubic-pcap-trace-jain.dat' using 1:2 with lines title 'Cubic' lt 7, '${output}/eyenet-pcap-trace-jain.dat' using 1:2 with lines title 'Eyenet'
EOM

        gnuplot -p $jainplot.gnuplot
        ;;

    *)
        echo "Usage : sudo bash $0 [ run <output_directory> <intf> <ip> <flows> <sender binary> | graph <output directory> ]"
        ;;
esac
