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

#define msleep(__x) this_thread::sleep_for(duration<double_t, milli>(unsigned(__x)))

extern ran::mt19937 &PRNG();

class random_generator {
    private:
        default_random_engine rand_gen;

    public:
        random_generator() : rand_gen(epoch_count) {}

        double_t uniform_dist(double_t x, double_t y);
        double_t exponential_dist(double_t x);

};

class expo {
    private:
        ran::mt19937                    &prng;
        ran::exponential_distribution<> dist;

    public:
        expo( double_t rate, ran::mt19937 &_prng) : prng(_prng), dist(rate) {}
        double_t dist_sample(void);
};

class ewma {
    private:
        double_t    e;
        double_t    a;

    public:
        bool        is_valid;

        ewma(const double_t a);
        void set(const double_t val);
        void update(const double_t val);
        void clear();

        operator double_t() const { return e; }
};

class win {
    private:
        double_t    sz;
        deque <pair <double_t, double_t> > val;
        double_t tail;

        void update_win(double_t now);

    public:
        bool        is_min;

        win(bool is_min);
        void clear();
        void add(double_t v, double_t ts);
        void update_sz(double_t s);
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

        rtt_win();
        void clear();
        void add(double_t v, double_t ts);
        double_t mode_eyenet() const;
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
        void add(double_t val);
        double_t get_rank();
        void clear();
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

        bool update(bool loss, double_t curr_ts, double_t rtt);
        void clear();
};


#endif
