#!/bin/bash

usage() {
    echo "Usage:"
    echo "  $0 [run <output> <sip> <dip> <trace> <mdelay> <nsend> <sender>"
    echo "  $0 [graph <output>]"
    echo "  $0 [clean <output>]"
    echo ""
    echo "Explanation:"
    echo "    sip : source ip"
    echo "    dip : destination ip"
    echo "    trace: If found, used for both uplink and downlink, else filename.up and filename.down are used."
    echo "    mdelay: Minimum delay in ms."
    echo "    output: Directory where both raw data and graphs are dumped."
    echo "    nsend: Number of senders"
    echo "    sender: Eyenet sender binary"
    exit
}

case $1 in
    run)
        shift

        if [[ $# -lt 7 ]]; then
            usage
        fi

        output=$1; shift
        sip=$1; shift
        dip=$1; shift
        trace=$1; shift
        mdelay=$1; shift
        nsend=$1; shift
        sender=$1; shift

        qlen=`expr 2 \* $mdelay \* 12000000 / 8000`
        traffic=continuous

        for tcp in "eyenet" "cubic"; do
            for (( i = 0; i < 11; i=i+1 )); do
                loss_rate=`awk -v i=$i 'END{if(i < 5)print i * 0.002; else print (i-4) * 0.01}' /dev/null`
                echo "Running on loss rate = $loss_rate"

                if [[ $tcp == "eyenet" ]];
                then
                    delta_conf="do_ss:constant_delta:0.5"
                    bash run.sh run $output $tcp $sip $dip $trace $mdelay $loss_rate $nsend $qlen $delta_conf $traffic $sender

                elif [[ $tcp == "cubic" ]];
                then
                    bash run.sh run $output $tcp $sip $dip $trace $mdelay $loss_rate $nsend $qlen
                fi

                if [[ -d ${output}/${tcp}::${loss_rate} ]]; then
                    trash ${output}/${tcp}::${loss_rate}
                fi
                mv ${output}/${tcp} $output/${tcp}::${loss_rate}
            done
        done
        ;;

    graph)
        shift

        if [[ $# -ne 1 ]]; then
            usage
        fi

        output=$1

        if [[ ! -d $output ]]; then
            echo "Directory '$output' not found"
            exit
        fi

        if [[ -d $output/graphs ]]; then
            trash $output/graphs
        fi
        mkdir $output/graphs

        for tcp_dir in $output/*::*;
        do
            tcp=`expr "$tcp_dir" : ".*/\([^/]*\)::[0-9.]*"`
            loss=`expr "$tcp_dir" : ".*::\([0-9.]*\)"`

            # Gather statistics
            mm-throughput-graph 100 $tcp_dir/$tcp.uplink > $output/graphs/${tcp}_${loss}.svg 2> $tcp_dir/$tcp.stats
            inkscape --export-png=${output}/graphs/${tcp}_${loss}.png -b "#ffffff" -D ${output}/graphs/${tcp}_${loss}.svg
            throughput=`grep throughput $tcp_dir/$tcp.stats | awk -F ' ' '{print $3}'`
            delay=`grep 95.*queueing $tcp_dir/$tcp.stats | awk -F ' ' '{print $6}'`
            echo $loss $throughput $delay >> $output/graphs/$tcp.dat
        done

        # Create gnuplot script
        gnuplot_script="
        set title 'Throughput vs Loss : Eyenet vs Cubic';
        set xlabel 'Loss Rate'; set ylabel 'Throughput (Mbps)';
        set terminal svg;
        set object 1 rectangle from screen 0,0 to screen 1,1 fillcolor rgb '#ffffff' behind;
        set output '$output/graphs/tput-vs-loss.svg';
        plot " >> $output/graphs/tput-vs-loss.gnuplot
        for tcp in $output/graphs/*.dat; do
            tcp_nice=`expr "$tcp" : ".*/\([^/]*\).dat"`
            echo $tcp $tcp_nice
            gnuplot_script="$gnuplot_script '$tcp' using 1:2 title '$tcp_nice' with lines, "
        done
        echo $gnuplot_script > $output/graphs/tput-vs-loss.gnuplot

        gnuplot -p $output/graphs/tput-vs-loss.gnuplot
        inkscape --export-png=$output/graphs/tput-vs-loss.png -b '#ffffff' -D $output/graphs/tput-vs-loss.svg
        ;;

    clean)
        shift

        if [[ $# -ne 1 ]]; then
            usage
        fi

        trash $output
        ;;
    *)
        usage
        ;;
esac
