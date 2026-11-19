#pragma once

#ifdef _WIN32
#include <guiddef.h>

namespace PhoneticIpaProduct
{
// Keep in sync across TSF DLL registration (Globals.cpp) and Watchdog (watchdog/main.cpp).
inline constexpr CLSID kImeClsid = {0xd95efabf, 0x8aec, 0x5396, {0xae, 0xa4, 0xa4, 0xe4, 0x7d, 0x5f, 0x21, 0xbf}};
inline constexpr GUID kImeProfileGuid = {0x881bb948, 0x8042, 0x590e, {0xaa, 0xc7, 0x77, 0xd3, 0x8b, 0xaf, 0xb1, 0xe6}};
} // namespace PhoneticIpaProduct
#endif
