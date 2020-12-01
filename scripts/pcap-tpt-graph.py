import dpkt
import math
import numpy as np
import os
import sys

BKT_SZ = 0.1 # in seconds

pcap = dpkt.pcap.Reader(open(sys.argv[1], mode="rb"))
bucket_start = -1
buckets, bucket, jain = {}, {}, {}
prev_bucket = []
flows = {}
for ts, buf in pcap:
    if bucket_start + BKT_SZ <= ts or bucket_start == -1:
        for x in bucket:
            bucket[x] /= ts - bucket_start
        if bucket != {}:
            buckets[ts] = bucket
        tpts = [bucket[x] for x in bucket]
        if len(prev_bucket) != len(bucket):
            pass
        else:
            if tpts != []:
                jain[ts] = sum(tpts) ** 2 / (len(tpts) * sum([x ** 2 for x in tpts]))
            else: jain[ts] = 0
        prev_bucket = bucket
        bucket_start = ts
        bucket = {}
    try:
        eth = dpkt.ethernet.Ethernet(buf)
    except:
        continue

    try:
       ip = dpkt.ip.IP(buf)
    except:
       continue
    eth.data = ip

    try:
        if (type(eth.data.data) == dpkt.udp.UDP):
            udp_eth = eth.data.data
        elif (type(eth.data.data) == dpkt.tcp.TCP):
            udp_eth = eth.data.data
        else:
            udp_eth = dpkt.udp.UDP(eth.data.data)
    except:
        continue

    if udp_eth.dport in [5001, 8888, 9000]:
        if udp_eth.sport not in flows:
            flows[udp_eth.sport] = 1
        if udp_eth.sport not in bucket:
            bucket[udp_eth.sport] = 0
        bucket[udp_eth.sport] += len(buf)

tptfilename = sys.argv[1] + "-tpt.dat"
tptpolyfilename = sys.argv[1] + "-tptpoly.dat"
jainfilename = sys.argv[1] + "-jain.dat"
tptfile = open(tptfilename, 'w')
tptpolyfile = open(tptpolyfilename, 'w')
jainfile = open(jainfilename, 'w')
timestamps = [x for x in buckets]
timestamps.sort()
flows = [x for x in flows]
flows.sort()
start_time = timestamps[0]
for ts in timestamps:
    out = str(ts - start_time) + " "
    for x in flows:
        if x in buckets[ts]:
            out += str(buckets[ts][x] * 8e-6) + " "
        else:
            out += "0 "
    tptfile.write(out + "\n")
    if ts in jain:
        jainfile.write(str(ts - start_time) + " " + str(jain[ts]) + "\n")

for ts in timestamps:
    tpts = [buckets[ts][x] for x in buckets[ts]]
    pltpt = 8e-6 * (np.mean(tpts) + np.std(tpts))
    if math.isnan(pltpt): continue
    if pltpt < 0: pltpt = 0
    tptpolyfile.write("%f %f\n" % (ts - start_time, pltpt))
for ts in timestamps[::-1]:
    tpts = [buckets[ts][x] for x in buckets[ts]]
    pltpt = 8e-6 * (np.mean(tpts) - np.std(tpts))
    if math.isnan(pltpt): continue
    if pltpt < 0: pltpt = 0
    tptpolyfile.write("%f %f\n" % (ts - start_time, pltpt))
