import math
import numpy as np
import os
import re
import sys

algo_map = {
    "eyenet" : "Eyenet",
    "cubic" : "Cubic"
}

def parse_tcptrace(num_tcp, num_x, udp_x, filename):
    assert(type(num_tcp) is int)
    assert(type(num_x) is int)
    assert(type(udp_x) is bool)
    assert(type(filename) is str)

    # Compile various regexes
    re_address = re.compile('\s*host \w*:\s*(?P<addr>[\d\w.-]*):(?P<port>[0-9]+)')
    re_conn_type = re.compile('(?P<type>TCP|UDP) connection [\d]*:')
    re_throughput = re.compile('\s*throughput:\s*(?P<tpt1>\d*) Bps\s*throughput:\s*(?P<tpt2>\d*) Bps.*')
    re_elapsed_time = re.compile('\s*elapsed time:\s*(?P<hrs>\d+):(?P<mins>\d+):(?P<secs>[\d.]+)')

    connections = []
    cur_conn = {}
    f = open(filename, 'r')
    for line in f.readlines():
        mtch_address = re_address.match(line)
        mtch_conn_type = re_conn_type.match(line)
        mtch_throughput = re_throughput.match(line)
        mtch_elapsed_time = re_elapsed_time.match(line)
        if mtch_conn_type != None:
            if cur_conn != {}:
                connections += [cur_conn]
            cur_conn = {'type': mtch_conn_type.group('type')}
        elif mtch_address != None:
            if mtch_address.group('port') not in ['5010', '5011'] and cur_conn['type'] == 'TCP':
                continue
            if mtch_address.group('port') not in ['8888', '9000'] and cur_conn['type'] == 'UDP':
                continue
            assert('port' not in cur_conn or cur_conn['type'] == 'UDP')
            if mtch_address.group('addr') not in ["192.168.33.3"]:
                print("Did not match the receiver ip address")
                continue
            #assert(mtch_address.group('addr') == 'localhost')
            cur_conn['port'] = int(mtch_address.group('port'))
        elif mtch_throughput != None:
            tpt = max(int(mtch_throughput.group('tpt1')), int(mtch_throughput.group('tpt2')))
            cur_conn['tpt'] = int(tpt) * 8e-6
        elif mtch_elapsed_time != None:
            hrs = int(mtch_elapsed_time.group('hrs'))
            mins = int(mtch_elapsed_time.group('mins'))
            secs = float(mtch_elapsed_time.group('secs'))
            cur_conn['elapsed_time'] = 3600 * hrs + 60 * mins + secs

    if cur_conn != {}:
        connections += [cur_conn]

    tcp_tpts, x_tpts = [], []
    for conn in connections:
        if 'type' not in conn or 'port' not in conn or 'tpt' not in conn or 'elapsed_time' not in conn:
            continue
        if conn['elapsed_time'] < 5: continue
        if conn['type'] == 'TCP' and conn['port'] == 5010:
            tcp_tpts += [conn['tpt']]
        elif conn['type'] == 'UDP' and udp_x:
            x_tpts += [conn['tpt']]
        elif conn['type'] == 'TCP' and conn['port'] == 5011 and not udp_x:
            assert(not udp_x)
            x_tpts += [conn['tpt']]
        else:
            pass

    tcp_tpts.sort(reverse=True)
    x_tpts.sort(reverse=True)
    if len(tcp_tpts) < num_tcp or len(x_tpts) < num_x:
        return ([], [])
    tcp_tpts = tcp_tpts[:num_tcp]
    x_tpts = x_tpts[:num_x]
    return (tcp_tpts, x_tpts)

def parse_experiment(dirname):
    re_dirname = re.compile(".*net-(?P<num_tcp>\d+)-(?P<num_x>\d+)-(?P<delay>\d+)-(?P<tpt>\d+)-(?P<buf>\d+)")
    mtch_dirname = re_dirname.match(dirname)
    num_tcp = int(mtch_dirname.group('num_tcp'))
    num_x = int(mtch_dirname.group('num_x'))
    delay = int(mtch_dirname.group('delay'))
    tpt = float(mtch_dirname.group('tpt'))
    buf = float(mtch_dirname.group('buf'))
    bdp = 2 * delay * tpt * 1e3 / 8

    ideal_tpt = tpt / (num_tcp + num_x)

    dirs = [f for f in os.listdir(dirname) if os.path.isdir(os.path.join(dirname, f))]
    res = {}
    for alg in dirs:
        if alg not in ["eyenet", "cubic", "reno"]:
            print("Unknown algorithm {}".format(alg))
            exit()

        if not os.path.isfile(os.path.join(dirname, alg, 'flow.stats')):
            return {}


    for alg in dirs:
        if not os.path.isfile(os.path.join(dirname, alg, 'flow.stats')):
            print("Could not find file '%s/%s'" % (dirname, alg))
            continue
        tcp_tpts, x_tpts = parse_tcptrace(num_tcp,
                                          num_x,
                                          alg in ["eyenet", "pcc"],
                                          os.path.join(dirname, alg, 'flow.stats'))
        x_lst = [x / ideal_tpt for x in x_tpts]
        tcp_lst = [x / ideal_tpt for x in tcp_tpts]
        if math.isnan(np.mean(x_lst)) or math.isnan(np.mean(tcp_lst)):
            continue

        res[alg] = x_lst
        res[alg + "-cubic"] = tcp_lst
    return res

if __name__ == "__main__":
    dirname = sys.argv[1]
    dirs = [f for f in os.listdir(dirname) if os.path.isdir(os.path.join(dirname, f))]
    res = {}
    for d in dirs:
        parsed = parse_experiment(os.path.join(dirname, d))
        for alg in parsed:
              if alg not in res:
                  res[alg] = parsed[alg]
              else:
                  res[alg].extend(parsed[alg])

    data_file = os.path.join(dirname, "coexist.dat")
    gnuplot_file = os.path.join(dirname, "coexist.gnuplot")
    op_file = os.path.join(dirname, "coexist.png")

    with open(data_file, "w") as f:
        f.write("Algorithm,Compared,std,Cubic,std\n")

        for alg in res:
            if '-' in alg:
                continue
            f.write(algo_map[alg] + "," +
                    str(round(np.mean(res[alg]), 2)) + "," +
                    str(round(np.std(res[alg]), 2)) + "," +
                    str(round(np.mean(res[alg + "-cubic"]), 2)) + "," +
                    str(round(np.std(res[alg + "-cubic"]), 2)) + "\n")

    with open(gnuplot_file, "w") as f:
        f.write("set terminal pngcairo\n")
        f.write("set output '" + op_file + "'\n")
        f.write("set key left\n")
        f.write("set title 'Coexistence of multiple type'\n\n")

        f.write("set style data histogram\n")
        f.write("set style histogram cluster errorbars\n")
        f.write("set errorbars linecolor black\n")
        f.write("set style fill solid border rgb 'black'\n\n")

        f.write("set ylabel 'Average Throughput'\n")
        f.write("set auto x\n")
        f.write("set yrange [0 : *]\n")
        f.write("set datafile separator ','\n\n")

        f.write("set object 1 rectangle from screen 0,0 to screen 1,1 fillcolor rgb '#ffffff' behind\n")
        f.write("set arrow from graph 0,first 1 to graph 1,first 1 nohead lc rgb '#000000' lw 1 lt 0 front\n\n")

        f.write("plot for [i=2:5:2] '" + data_file + "' using i:i+1:xtic(1) title col(i), 1/0 t 'Ideal Throughput' lt 0\n")
