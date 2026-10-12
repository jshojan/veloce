#pragma once
#ifdef VELOCE_N64_PROFILE
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace n64 {
// Host-only counters for one local profiling process. They are never part of
// native state or netplay. Normal builds compile out every read and timer.
struct N64Profile {
    uint64_t cpu_steps=0,idle_clocks=0,active_clocks=0,inactive_clocks=0,ordinary_clocks=0;
    uint64_t rsp_steps=0,rdp_draws=0,active_ns=0,rdp_ns=0;
    ~N64Profile(){
        std::fprintf(stderr,
            "N64_PROFILE cpu_steps=%llu idle_clocks=%llu active_clocks=%llu inactive_clocks=%llu ordinary_clocks=%llu rsp_steps=%llu rdp_draws=%llu active_ms=%.3f rdp_ms=%.3f\n",
            (unsigned long long)cpu_steps,(unsigned long long)idle_clocks,
            (unsigned long long)active_clocks,(unsigned long long)inactive_clocks,
            (unsigned long long)ordinary_clocks,(unsigned long long)rsp_steps,
            (unsigned long long)rdp_draws,double(active_ns)/1e6,double(rdp_ns)/1e6);
    }
};
inline N64Profile n64_profile;
struct N64ProfileTimer {
    uint64_t& elapsed;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    ~N64ProfileTimer(){
        elapsed+=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now()-start).count());
    }
};
}
#endif
