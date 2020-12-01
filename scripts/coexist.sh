#!/bin/bash

usage() {
    echo "Usage : $0 <run | graph> <output directory>"
    exit
}

freq=100 # freq value of kernel
sender=/home/vagrant/simulator/sender
t_run=10000

intf=enp0s8
rip=192.168.33.3 #127.0.0.1
cubic_port=5010
cong_port=5011

t_sleep=$(( ${t_run} / 1000 ))

random_network() {
    cubic_iter=$(( ( $RANDOM % 4 )  + 1 ))
    cong_iter=$(( ( $RANDOM % 4 )  + 1 ))
    delay=$(( ( $RANDOM % 49 ) + 1 )) # RTT / 2 (ms)
    tput=$(( ( $RANDOM % 49) + 1 )) # Mbps
    bdp=`awk -v b=${tput} -v d=${delay} 'BEGIN{print 2 * b * d * 1E3 / 8}'` # bytes
    queue_2n_bdp=$(( ( $RANDOM % 9 ) + 1 ))
    queue=`awk -v n_bdp=${queue_2n_bdp} -v bdp=${bdp} 'BEGIN{print bdp * n_bdp / 2}'`
    loss=0
    echo TCP ${cubic_iter} Cong ${cong_iter} Delay ${delay} Throughput ${tput} BDP ${bdp} Queue ${queue}
}

setup_qdisc() {
    op_netem=add
    op_tbf=add
    if tc qdisc show dev ${intf} | grep -q netem; then op_netem=change; fi
    if tc qdisc show dev ${intf} | grep -q tbf; then op_tbf=change; fi

    burst=`awk -v r=${tput} -v hz=${freq} 'END{print 2 * r * 1E6 / (hz * 8)}' /dev/null`
    sudo ifconfig ${intf} mtu 1600
    sudo tc qdisc ${op_netem} dev ${intf} root handle 1:1 netem delay $(echo ${delay})ms loss ${loss}
    sudo tc qdisc ${op_tbf} dev ${intf} parent 1:1 handle 10: tbf rate $(echo ${tput})mbit limit ${queue} burst ${queue}
}

case $1 in
    run)
        shift

        if [[ $# -ne 1 ]]; then
            usage
        fi

        output=$1

        mkdir -p ${output}

        for netid in {1..100}; do
            random_network
            setup_qdisc
            net_dir=${output}/net-${cubic_iter}-${cong_iter}-${delay}-${tput}-${queue}

            if [[ -d ${net_dir} ]]; then
                trash ${net_dir}
            fi
            mkdir ${net_dir}

            sleep 5

            for cong_alg in "eyenet" "cubic"; do
                dir=${net_dir}/${cong_alg}
                mkdir ${dir}

                if [[ -f /tmp/tcp-coexist.pcap ]]; then
                    sudo rm /tmp/tcp-coexist.pcap
                fi
                sudo tcpdump -w /tmp/tcp-coexist.pcap -i ${intf} -n &

                sender_pids=""
                sudo sysctl -w net.ipv4.tcp_congestion_control=cubic > /dev/null 2>&1
                sleep 5
                for (( j=0; $j < ${cubic_iter}; j++ )); do
                    of_name=${dir}/tcp-${j}
                    iperf -c ${rip} -p ${cubic_port} -t ${t_sleep} -Z cubic 1> ${of_name}.stdout 2> ${of_name}.stderr &
                    sender_pids="${sender_pids} $!"
                done

                echo "Congestion : ${cong_alg}"
                case ${cong_alg} in
                    cubic|reno|ledbat)
                        sudo sysctl -w net.ipv4.tcp_congestion_control=${cong_alg}
                        sleep 5
                        ;;
                    *)
                        ;;
                esac

                for (( j=0; $j < ${cong_iter}; j++ )); do
                    of_name=${dir}/cong-${j}
                    case ${cong_alg} in
                        eyenet)
                            export MIN_RTT=10000000
                            ${sender} serverip=${rip} traffic_params=deterministic,num_cycles=1 onduration=${t_run} offduration=1 cctype=markovian delta_conf=do_ss:auto:0.5  1> ${of_name}.stdout 2> ${of_name}.stderr &
                            sender_pids="${sender_pids} $!"
                            ;;
                        cubic|reno|ledbat)
                            iperf -c ${rip} -p ${cong_port} -t ${t_sleep} -Z ${cong_alg} 1> ${of_name}.stdout 2> ${of_name}.stderr &
                            sender_pids="${sender_pids} $!"
                    esac
                done

                sleep ${t_sleep}
                echo "Finishing run"
                sudo pkill tcpdump
                sudo pkill tcpdump
                kill -9 ${sender_pids}
                tcptrace -lu /tmp/tcp-coexist.pcap > ${dir}/flow.stats
                sudo rm /tmp/tcp-coexist.pcap
            done
        done
        ;;

    graph)
        shift

        if [[ $# -ne 1 ]]; then
            usage
        fi

        output=$1

        python3 coexist-plot.py $output > /dev/null 2>&1
        gnuplot -p $output/coexist.gnuplot

        ;;

    *)
        usage
        ;;
esac

