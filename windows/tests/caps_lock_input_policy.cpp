#include "Key/CapsLockInputPolicy.h"
#include "../../engine/contracts/ipa_input.h"
#include <cstdio>
#include <initializer_list>

int main()
{
    for (unsigned key = 'A'; key <= 'Z'; ++key)
    {
        if (NormalizeIpaCapsLockCharacter(true, key, key) != key + 'a' - 'A' ||
            NormalizeIpaCapsLockCharacter(true, key, key + 'a' - 'A') != key ||
            NormalizeIpaCapsLockCharacter(false, key, key) != key)
            return 1;
    }
    for (wchar_t c : {L'1', L'!', L'/', L'ə', L'\0'})
        if (NormalizeIpaCapsLockCharacter(true, 'A', c) != c)
            return 2;
    for (unsigned key = '0'; key <= '9'; ++key)
        for (unsigned modifiers = 0; modifiers < 8; ++modifiers)
            if (PhoneticIpaKey::IsToneShortcut(key, modifiers) != (key >= '1' && key <= '5' && modifiers == 1))
                return 3;
    std::puts("PASS: output-mode Caps Lock preserves letters/Shift layer; tone shortcuts 1-5 only");
    return 0;
}
