#ifndef LOCALFLASHSHARED_H
#define LOCALFLASHSHARED_H

#include <cstdint>

namespace LocalFlash {

enum class HelperExitCode {
    InvalidUsage = 1,
    InitFailed = 2,
    DetectFailed = 3,
    InputOpenFailed = 4,
    InstallRulesFailed = 5,
    ReloadRulesAfterInstallFailed = 6,
    RemoveRulesFailed = 7,
    ReloadRulesAfterRemoveFailed = 8,
    ReadFailed = 9,
    OutputOpenFailed = 10,
    OutputWriteFailed = 11,
    InputReadFailed = 12,
    EraseFailed = 13,
    WriteFailed = 14,
    InvalidExpectedSize = 15,
    EmptyImage = 16,
    ImageSizeMismatch = 17,
};

inline std::uint64_t flashSizeFromCapacityCode(std::uint8_t capacityCode) {
    return (capacityCode >= 0x13 && capacityCode <= 0x21) ? (std::uint64_t{1} << capacityCode) : 0;
}

} // namespace LocalFlash

#endif // LOCALFLASHSHARED_H
