#pragma once

// Caps Lock selects the output script. Undo its casing effect once after
// ToUnicode, while retaining the physical Shift key's uppercase CIN layer.
inline wchar_t NormalizeIpaCapsLockCharacter(bool capsLockOn, unsigned key, wchar_t character)
{
    if (!capsLockOn || key < 'A' || key > 'Z')
        return character;
    if (character >= L'A' && character <= L'Z')
        return character + L'a' - L'A';
    if (character >= L'a' && character <= L'z')
        return character - L'a' + L'A';
    return character;
}
