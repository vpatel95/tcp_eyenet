#include "vns_utils.hh"

double_t random_generator::uniform_dist(double_t x, double_t y) {
    uniform_real_distribution<double_t> d(x,y);
    return d(rand_gen);
}

double_t random_generator::exponential_dist(double_t x) {
    exponential_distribution<double_t> d(1/x);
    return d(rand_gen);
}

double_t expo::dist_sample(void) { return dist(prng); }


ewma::ewma(const double_t a) :
    e(),
    a(a),
    is_valid(false)
{}

void ewma::set(const double_t val) {
    is_valid = true;
    e = val;
}

void ewma::update(const double_t val) {
    is_valid = true;
    e = ((a * val) + (1 - a) * e);
}

void ewma::clear() {
    e = 0.0;
}

void win::update_win(double_t now) {
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

win::win(bool is_min) :
    sz(10e3),
    val(),
    tail(is_min ? DOUBLE_MAX : DOUBLE_MIN),
    is_min(is_min)
{}

void win::clear() {
    sz = 10e3;
    val.clear();
    tail = is_min ? DOUBLE_MAX : DOUBLE_MIN;
}

void win::add(double_t v, double_t ts) {
    while (!val.empty() &&
            ((is_min && val.back().second > v) ||
             (!is_min && val.back().second < v)))
        val.pop_back();

    val.push_back(make_pair(ts, v));
    if ((is_min && (v < tail)) || (!is_min && (v > tail)))
        tail = v;

    update_win(ts);
}

void win::update_sz(double_t s) { sz = s; }


rtt_win::rtt_win():
    srtt(0.0),
    a(1.0 / 16.0),
    eyenet_min(true),
    eyenet_max(false),
    curr_rtt(0.0),
    min_rtt(true),
    virgin_rtt(true)
{}

void rtt_win::clear() {
    srtt = 0.0;
    min_rtt.clear();
    virgin_rtt.clear();
    eyenet_min.clear();
    eyenet_max.clear();
}

void rtt_win::add(double_t v, double_t ts) {
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

double_t rtt_win::mode_eyenet() const {
    double_t thres = min_rtt + (0.1 * (eyenet_max - min_rtt));
    return eyenet_min < thres;
}

void data_rank::add(double_t val) {
    window.push(val);
    if (window.size() > win_len)
        window.pop();
}

double_t data_rank::get_rank() {
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

void data_rank::clear() {
    while (!window.empty())
        window.pop();
}

bool loss_reduce::update(bool loss, double_t curr_ts, double_t rtt) {
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

void loss_reduce::clear() {
    nloss = 0;
    npkts = 0;
    last_win_ts = 0.0;
}

ran::mt19937 &PRNG(void) {
    static ran::mt19937 gen(time(NULL) ^ getpid());
    return gen;
}
