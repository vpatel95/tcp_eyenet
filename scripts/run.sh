#!/bin/bash

usage() {
    echo "Usage:"
    echo "  $0 [run <output> <cong> <sip> <dip> <trace> <mdelay> <loss> <nsend> <qlen> [<delta> <traffic> <sender>]"
    echo ""
    echo "Explanation:"
    echo "    cong: One of markovian, remy, sprout, pcc, pcp, cubic and reno."
    echo "    trace: If found, used for both uplink and downlink, else filename.up and filename.down are used."
    echo "    mdelay: Minimum delay in ms."
    echo "    loss: Fraction of packets lost between 0 and 1."
    echo "    output: Directory where both raw data and graphs are dumped."
    echo "    nsend: Number of senders"
    echo "    traffic: Type of traffic. exponential|continuous. If exponential, predetermined exponential distribution will be used, else a predetermined continuous run will be used."
    echo "    qlen: Droptail queue length in bytes. If 0, infinite queue will be used."
    exit
}

output_file_name() {
        of_dir=$output/$cong
        of_name=$of_dir/$cong
}

case $1 in
    run)
        shift

        if [[ $# -lt 9 ]]; then
            usage
        fi

        output=$1; shift
        cong=$1; shift
        sip=$1; shift
        dip=$1; shift
        trace=$1; shift
        mdelay=$1; shift
        loss=$1; shift
        nsend=$1; shift
        qlen=$1; shift

        run_time=100000 # in ms
        on_duration=100000 # in ms
        off_duration=1 # in ms

        if [[ -f $trace ]]; then
            trace_uplink=$trace
            trace_downlink=$trace
        else
            echo "Could not find specified trace file"
            exit
        fi

        mkdir -p $output

        if [[ $qlen == "0" ]]; then
            queue_length_params="--uplink-queue=infinite --downlink-queue=infinite"
        else
            queue_length_params="--uplink-queue=droptail --uplink-queue-args=\"bytes=$qlen\" --downlink-queue=droptail --downlink-queue-args=\"bytes=$qlen\""
        fi

        output_file_name
        mkdir -p $of_dir

        if [[ $cong == "eyenet" ]]; then

            if [[ $# -ne 3 ]]; then
                usage
            fi

            delta_conf=$1; shift
            traffic=$1; shift
            sender=$1

            export MIN_RTT=`awk -v mdelay=$mdelay 'END{print 2*mdelay;}' /dev/null`
            mm-delay $mdelay \
                mm-loss uplink $loss \
                mm-link $trace_uplink $trace_downlink --uplink-log $of_name.uplink --downlink-log $of_name.downlink $queue_length_params \
                ./run-sender.sh "$sender sourceip=$sip serverip=$dip cctype=eyenet delta_conf=$delta_conf" $nsend $traffic $run_time $on_duration $off_duration \
                1> $of_name.stdout 2> $of_name.stderr

        elif [[ $cong == "cubic" ]]; then
            mm-delay $mdelay \
                mm-loss uplink $loss \
                mm-link $trace_uplink $trace_downlink --uplink-log $of_name.uplink --downlink-log $of_name.downlink $queue_length_params \
                ./run-iperf-sender.sh $dip $on_duration $cong $nsend \
                1> $of_name.stdout 2> $of_name.stderr

        else
            echo "Could not find cong '$cong'. It is either unsupported or not yet implemented"
        fi
        ;;

    *)
        usage
        ;;
esac
