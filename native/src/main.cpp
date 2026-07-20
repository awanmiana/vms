#include "hardware/HardwareProbe.h"

#include <iostream>

// Increment 1: print the detected machine profile as JSON.
// This is the foundation of per-machine tuning — it tells the future governor
// which codecs decode in hardware and how much headroom the machine has.
int main() {
    const vms::MachineProfile profile = vms::ProbeMachine();
    std::cout << profile.toJson();
    return 0;
}
