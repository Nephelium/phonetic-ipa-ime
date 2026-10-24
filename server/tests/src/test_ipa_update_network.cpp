#include "utils/ipa_update.h"
#include <iostream>
int main()
{
    // Explicit online smoke check. Never creates UI or starts the update monitor.
    const auto result = PhoneticIpa::Updates::Check(true);
    const bool current = !result.latest && result.status == L"已是最新版本";
    std::cout << "Both live update sources current: " << (current ? "PASS" : "FAIL") << '\n';
    return current ? 0 : 1;
}
