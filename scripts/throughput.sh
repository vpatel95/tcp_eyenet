#!/bin/bash

TCP="eyenet cubic reno ledbat"

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

        for tcp in $TCP;
        do
            sudo tcpdump -i $intf -n -w $output/$tcp-pcap-trace &

            case $tcp in
                cubic|reno|ledbat)
                    sudo sysctl -w net.ipv4.tcp_congestion_control=${tcp}
                    sleep 2
                    ;;
                *)
                    ::
            esac

            sender_pids=""
            for (( i=0; i < $flows; i++ ));
            do
                onduration=`expr 2 \* $intersend \* \( $flows - $i - 1 \) + 1`
                case $tcp in
                    cubic|reno|ledbat)
                        iperf -c $rip -Z $tcp -t $onduration -p 5001 &
                        sender_pids="$sender_pids $!"
                        ;;
                    eyenet)
                        $sender cctype=eyenet serverip=$rip offduration=0 traffic_params=deterministic,num_cycles=1 delta_conf=do_ss:auto:0.5 onduration=`expr $onduration \* 1000`  &
                        sender_pids="$sender_pids $!"
                        ;;
                    *)
                        ;;
                esac
                sleep $intersend
            done

            sleep `expr $flows \* $intersend - 1`

            kill $sender_pids > /dev/null 2>&1
            pkill tcpdump > /dev/null 2>&1
            sudo chown -R vagrant:vagrant $output
        done
        sudo sysctl -w net.ipv4.tcp_congestion_control=cubic
        ;;

    graph)
        shift

        if [[ $# -ne 1 ]];
        then
            echo "Usage : sudo bash $0 graph <output directory>"
            exit
        fi

        output=$1

        if [[ ! -d $output ]]; then
            echo "Directory '$output' not found"
            exit
        fi

        for inp in $output/*-pcap-trace;
        do
            python3 pcap-tpt-graph.py $inp
        done

        tputplot=$output/tput

        gplot_script="
        set style fill transparent solid 0.5 noborder;
        set xlabel 'Time (s)';
        set ylabel 'Throughput (Mbits/s)';
        set xrange [0:19];
        set yrange [1:100];
        set logscale y;
        set terminal png;"

        gplot_tcp="plot '${output}/eyenet-pcap-trace-tptpoly.dat' using 1:2 with filledcurves title 'Eyenet'"
        for tcp in $TCP;
        do
            case $tcp in
                cubic|reno|ledbat)
                    gplot_output="set title 'Throughput : Eyenet vs. ${tcp^}'; set output '${tputplot}-${tcp}.png';"
                    gplot="${gplot_script} ${gplot_output} ${gplot_tcp}, '${output}/${tcp}-pcap-trace-tptpoly.dat' using 1:2 with filledcurves title '${tcp^}"
                    ;;
                *)
                    ;;
            esac
            echo ${gplot} > ${tputplot}-${tcp}.gnuplot
        done

        jainplot=$output/jain-plot
        gplot_script="
        set title 'Jain Index';
        set key bottom;
        set style fill transparent solid 0.5 noborder;
        set xlabel 'Time (s)';
        set ylabel 'jain Index';
        set xrange [0:20];
        set yrange [0:1];
        set terminal png;
        set output '${jainplot}.png';
        plot "

        for tcp in $TCP;
        do
            gplot_script="${gplot_script} '${output}/${tcp}-pcap-trace-jain.dat' using 1:2 with lines title '${tcp^}', "
        done

        echo ${gplot_script} > $jainplot.gnuplot

        for plot in $output/*.gnuplot;
        do
            gnuplot -p ${plot}
        done
        ;;

    *)
        echo "Usage : sudo bash $0 [ run <output_directory> <intf> <ip> <flows> <sender binary> | graph <output directory> ]"
        ;;
esac
