#pragma once
#include "engine/ipa/update_manifest.h"
#include <windows.h>
namespace PhoneticIpa::Updates
{
bool Automatic();
void SetAutomatic(bool enabled);
std::optional<Feed> Available();
struct Result
{
    std::optional<Feed> latest;
    std::wstring status;
};
Result Check(bool manual);
void StartMonitor();
void StopMonitor();
} // namespace PhoneticIpa::Updates
