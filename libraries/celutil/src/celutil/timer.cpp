#include "timer.h"
#include <chrono>

class TimerImpl : public Timer {
public:
    using time_point = std::chrono::time_point<std::chrono::high_resolution_clock>;
    using ms = std::chrono::milliseconds;

    TimerImpl() {
        reset();
    }

    static time_point now() {
        return std::chrono::high_resolution_clock::now();
    }

    void reset() override {
        start = now();
    }

    double getTime() const override {
        auto resultInt = std::chrono::duration_cast<ms>(now() - start).count();
        double result = static_cast<double>(resultInt);
        result /= 1000.0;
        return result;
    }

    time_point start;
};

TimerPtr CreateTimer() {
    return std::make_shared<TimerImpl>();
}
