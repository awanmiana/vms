#include "optimize/SystemHealth.h"

#ifdef _WIN32

#include <windows.h>

namespace vms::optimize {

namespace {
unsigned long long asU64(const FILETIME& f) {
    return (static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime;
}
}  // namespace

SystemSample WindowsSystemHealthProbe::sample() {
    SystemSample s;

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        s.totalRamMb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0);
        s.availRamMb = static_cast<double>(mem.ullAvailPhys) / (1024.0 * 1024.0);
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    s.logicalCores = si.dwNumberOfProcessors;

    // System CPU load from GetSystemTimes deltas. Kernel time INCLUDES idle, so
    // busy = (kernel + user) - idle over the interval since the last sample.
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        const unsigned long long i = asU64(idle);
        const unsigned long long k = asU64(kernel);
        const unsigned long long u = asU64(user);
        if (havePrev_) {
            const unsigned long long di = i - lastIdle_;
            const unsigned long long total = (k - lastKernel_) + (u - lastUser_);
            if (total > 0)
                s.systemCpuLoadPct =
                    100.0 * static_cast<double>(total - di) / static_cast<double>(total);
        }
        lastIdle_ = i;
        lastKernel_ = k;
        lastUser_ = u;
        havePrev_ = true;
    }

    s.valid = s.totalRamMb > 0.0;
    return s;
}

} // namespace vms::optimize

#endif // _WIN32
