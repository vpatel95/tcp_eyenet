#ifndef __UTILS_H__
#define __UTILS_H__

#include <chrono>
#include <ctime>
#include <random>
#include <unistd.h>

#include <boost/random/exponential_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

#include <sys/types.h>

#define ran boost::random
#define epoch_count std::chrono::system_clock::now().time_since_epoch().count()

#define msleep(__x) usleep(__x * 1000)

using std::default_random_engine;
using std::exponential_distribution;
using std::uniform_int_distribution;
using std::uniform_real_distribution;

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

ran::mt19937 &PRNG(void) {
    static ran::mt19937 gen(time(NULL) ^ getpid());
    return gen;
}

#endif
