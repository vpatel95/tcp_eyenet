#ifndef __UTILS_H__
#define __UTILS_H__

#include <cassert>
#include <chrono>
#include <ctime>
#include <deque>
#include <iostream>
#include <limits>
#include <math.h>
#include <queue>
#include <random>
#include <tuple>
#include <unistd.h>
#include <vector>

#include <boost/random/exponential_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <sys/types.h>

using namespace std;

#define DOUBLE_MAX numeric_limits<double_t>::max()
#define DOUBLE_MIN numeric_limits<double_t>::min()
#define ran boost::random
#define epoch_count std::chrono::system_clock::now().time_since_epoch().count()

#define msleep(__x) usleep(__x * 1000)

extern ran::mt19937 &PRNG();

class rand {
    private:
        default_random_engine rand_gen;

    public:
        rand() : rand_gen(epoch_count) {}

        double_t uniform_dist(double_t x, double_t y) {
            uniform_real_distribution<double_t> d(x,y);
            return d(rand_gen);
        }

        double_t exponential_dist(double_t x) {
            exponential_distribution<double_t> d(1/x);
            return d(rand_gen);
        };
};

class expo {
    private:
        ran::mt19937                    &prng;
        ran::exponential_distribution<> dist;

    public:
        expo( double_t rate, ran::mt19937 &_prng) : prng(_prng), dist(rate) {}
        double_t dist_sample(void) { return dist(prng); }
};

class ewma {
    private:
        double_t    e;
        double_t    a;

    public:
        bool        is_valid;

        ewma(const double_t a) :
            e(),
            a(a),
            is_valid(false)
        {}

        void set(const double_t val) {
            is_valid = true;
            e = val;
        }

        void update(const double_t val) {
            is_valid = true;
            e = ((a * val) + (1 - a) * e);
        }

        void clear() {
            e = 0.0;
        }

        operator double_t() const { return e; }
};

class win {
    private:
        double_t    sz;
        deque <pair <double_t, double_t> > val;
        double_t tail;

        void update_win(double_t now) {
            bool again = false;

            while (val.size() > 1 &&
                    val.front().first < (now - sz)) {
                if ((is_min && val.front().second <= tail) ||
                    (!is_min && val.front().second >= tail))
                    again = true;

                val.pop_front();
            }

            if (again) {
                tail = val.front().second;
            }
        }

    public:
        bool        is_min;
        win(bool is_min) :
            sz(10e3),
            val(),
            tail(is_min ? DOUBLE_MAX : DOUBLE_MIN),
            is_min(is_min)
        {}

        void clear() {
            sz = 10e3;
            val.clear();
            tail = is_min ? DOUBLE_MAX : DOUBLE_MIN;
        }

        void add(double_t v, double_t ts) {
            while (!val.empty() &&
                    ((is_min && val.back().second > v) ||
                     (!is_min && val.back().second < v)))
                val.pop_back();

            val.push_back(make_pair(ts, v));
            if ((is_min && (v < tail)) || (!is_min && (v > tail)))
                tail = v;

            update_win(ts);
        }

        void update_sz(double_t s) { sz = s; }
        operator double_t() const { return tail; }

};

class rtt_win {
    private:
        double_t        srtt;
        const double    a;
        win             eyenet_min;
        win             eyenet_max;

    public:
        double_t        curr_rtt;
        win             min_rtt;
        win             virgin_rtt;

        rtt_win():
            srtt(0.0),
            a(1.0 / 16.0),
            eyenet_min(true),
            eyenet_max(false),
            curr_rtt(0.0),
            min_rtt(true),
            virgin_rtt(true)
        {}

        void clear() {
            srtt = 0.0;
            min_rtt.clear();
            virgin_rtt.clear();
            eyenet_min.clear();
            eyenet_max.clear();
        }

        void add(double_t v, double_t ts) {
            double_t sz;
            if (srtt == 0.0)
                srtt = v;

            srtt = ((a * v) + (1 - a) * srtt);
            curr_rtt = v;

            sz = max(10E3, 20 * min_rtt);
            min_rtt.update_sz(sz);
            virgin_rtt.update_sz(min(sz, 0.5 * srtt));
            eyenet_min.update_sz(min(sz, 4 * srtt));
            eyenet_max.update_sz(min(sz, 4 * srtt));

            min_rtt.add(v, ts);
            virgin_rtt.add(v, ts);
            eyenet_min.add(v, ts);
            eyenet_max.add(v, ts);
        }

        double_t switch_mode() const {
            double_t thres = min_rtt + (0.1 * (eyenet_max - min_rtt));
            return eyenet_min >= thres;
        }
};

class data_rank {
    private:
        double_t                percentile;
        // make type generic
        queue<double_t>         window;
        static constexpr int    win_len = 100;

    public:
        data_rank(double_t val):
            percentile(val),
            window()
        {}

        // make generic
        void add(double_t val) {
            window.push(val);
            if (window.size() > win_len)
                window.pop();
        }

        double_t get_rank() {
            int i, j,
                n = (1 - percentile) * win_len;
            double_t largest[win_len + 1];

            for (i = 0; i < n; i++) largest[i] = -1;

            if (window.size() == 0)
                return 0;

            for (i = 0; i < win_len; i++) {
                double_t val = window.front(),
                         tmp = val;
                window.pop();
                for (j = 0; j < n; j++) {
                    if (tmp <= largest[n-1]) continue;
                    if (tmp > largest[j]) {
                        double_t t = largest[j];
                        largest[j] = tmp;
                        tmp = t;
                    }
                }
                window.push(val);
            }
            return largest[n-1];
        }

        void clear() {
            while (!window.empty())
                window.pop();
        }
};

class loss_reduce {
    private:
        int32_t     nloss,
                    npkts;
        double_t    last_win_ts;

    public:
        loss_reduce():
            nloss(0),
            npkts(0),
            last_win_ts(0.0)
        {}

        bool update(bool loss, double_t curr_ts, double_t rtt) {
            if (loss)
                ++nloss;
            ++npkts;

            if ((curr_ts > last_win_ts + (2 * rtt)) && npkts > 20) {
                double_t loss_rate;
                loss_rate = 1.0 * (nloss / npkts);
                last_win_ts = curr_ts;
                nloss = 0;
                npkts = 0;
                if (loss_rate > 0.3)
                    return true;
            }

            return false;
        }

        void clear() {
            nloss = 0;
            npkts = 0;
            last_win_ts = 0.0;
        }
};

ran::mt19937 &PRNG(void) {
    static ran::mt19937 gen(time(NULL) ^ getpid());
    return gen;
}

#endif
