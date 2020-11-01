#ifndef __DEFAULT_H__
#define __DEFAULT_H__

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>

using namespace std;

class default_cong {
    private:
        string      dip;

    public:
        default_cong(string ip) : dip(ip) {
            string kern_version = "echo \"    Kernel Version : `uname -r`\"";
            string cong_alg = "echo \"    Congestion Control Algorithm : `sysctl -a 2> /dev/null | grep net.ipv4.tcp_congestion_control | awk '{print $3}'`\"";
            cout << "Selected Kernel's default congestion control" << endl;
            system(kern_version.c_str());
            system(cong_alg.c_str());
            cout << endl << endl;
        }

        void send_data (double_t fsz, bool byte_switcher,
                        int32_t fid __attribute((unused)),
                        int32_t sid __attribute((unused))) {

            string      perf_cmd;

            if (byte_switcher) {
                perf_cmd = "iperf -c " + dip + " -n " + to_string(fsz).c_str();
            } else {
                perf_cmd = "iperf -c " + dip + " -t " + to_string(fsz / 1000).c_str();
            }

            system(perf_cmd.c_str());

            cout << "CMD : " << perf_cmd << endl;
        }
};

#endif
