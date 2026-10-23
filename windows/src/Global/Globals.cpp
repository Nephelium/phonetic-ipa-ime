#include "Globals.h"
#include "Private.h"
#include "resource.h"
#include "define.h"
#include "MetasequoiaIMEBaseStructure.h"
#include <unordered_set>
#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include "FanyUtils.h"

namespace Global
{
HINSTANCE dllInstanceHandle;

LONG dllRefCount = -1;

CRITICAL_SECTION CS;
HFONT defaultlFontHandle; // Global font object we use everywhere

//---------------------------------------------------------------------
// MetasequoiaIME CLSID
//---------------------------------------------------------------------
extern const CLSID MetasequoiaIMECLSID = {0xd95efabf, 0x8aec, 0x5396, {0xae, 0xa4, 0xa4, 0xe4, 0x7d, 0x5f, 0x21, 0xbf}};

//---------------------------------------------------------------------
// Profile GUID
//---------------------------------------------------------------------
extern const GUID MetasequoiaIMEGuidProfile = {
    0x881bb948, 0x8042, 0x590e, {0xaa, 0xc7, 0x77, 0xd3, 0x8b, 0xaf, 0xb1, 0xe6}};

//---------------------------------------------------------------------
// PreserveKey GUID
//---------------------------------------------------------------------
extern const GUID MetasequoiaIMEGuidImeModePreserveKey = {
    0x507ddfb8, 0x7a70, 0x5b31, {0xbf, 0x0a, 0x19, 0x62, 0x07, 0x8e, 0x70, 0xd1}};

extern const GUID MetasequoiaIMEGuidImeModePreserveKey02 = {
    0x8d62849f, 0x84db, 0x5fba, {0x97, 0x87, 0x07, 0xdb, 0x57, 0x71, 0xe4, 0x3f}};

extern const GUID MetasequoiaIMEGuidImeModePreserveKey03 = {
    0xbe6195f6, 0x93db, 0x5aa8, {0x8b, 0xa9, 0x1d, 0xda, 0x80, 0x57, 0x42, 0xdb}};

extern const GUID MetasequoiaIMEGuidEnglishInputModePreserveKey = {
    0xea48764d, 0xcd08, 0x5c03, {0x94, 0x72, 0x59, 0x62, 0x0e, 0x28, 0x2a, 0x7c}};

extern const GUID MetasequoiaIMEGuidDoubleSingleBytePreserveKey = {
    0xb9789247, 0xa3cc, 0x5135, {0x8b, 0xb2, 0x4c, 0x85, 0xff, 0xea, 0x45, 0xe1}};

extern const GUID MetasequoiaIMEGuidPunctuationPreserveKey = {
    0xe8ed9ac0, 0x5f15, 0x5b3a, {0xb0, 0xfc, 0x42, 0xa2, 0x4b, 0x24, 0x0f, 0xb3}};

//---------------------------------------------------------------------
// Compartments
//---------------------------------------------------------------------
extern const GUID MetasequoiaIMEGuidCompartmentDoubleSingleByte = {
    0x3a409733, 0xc447, 0x58f7, {0x86, 0x68, 0xcd, 0xc9, 0x11, 0x40, 0xf8, 0x4b}};

extern const GUID MetasequoiaIMEGuidCompartmentPunctuation = {
    0x97b9b005, 0x994b, 0x513e, {0xbc, 0xf1, 0x03, 0xc1, 0xd8, 0x02, 0xf1, 0x4d}};

//---------------------------------------------------------------------
// LanguageBars
//---------------------------------------------------------------------

extern const GUID MetasequoiaIMEGuidLangBarIMEMode = {
    0x4a3eda51, 0xe01e, 0x55d3, {0xaf, 0x3e, 0x4a, 0xbd, 0xf2, 0x8c, 0x4e, 0xed}};

extern const GUID MetasequoiaIMEGuidLangBarDoubleSingleByte = {
    0x17302d9b, 0xf950, 0x5cb1, {0xb8, 0x60, 0x4f, 0x42, 0xfa, 0x22, 0x39, 0xeb}};

extern const GUID MetasequoiaIMEGuidLangBarPunctuation = {
    0x65e50525, 0x2b29, 0x503e, {0x9c, 0xde, 0x4d, 0x2c, 0x78, 0x63, 0xbb, 0xaa}};

extern const GUID MetasequoiaIMEGuidDisplayAttributeInput = {
    0xfa89c450, 0xd3d4, 0x5ada, {0x96, 0x55, 0xf8, 0x6e, 0x8e, 0x28, 0x28, 0x04}};

extern const GUID MetasequoiaIMEGuidDisplayAttributeConverted = {
    0x387ca902, 0x8591, 0x5463, {0xbf, 0x26, 0xf2, 0xe5, 0xf5, 0x7a, 0x91, 0x60}};

//---------------------------------------------------------------------
// UI element
//---------------------------------------------------------------------

extern const GUID MetasequoiaIMEGuidCandUIElement = {
    0x58011c5d, 0x7ea4, 0x5332, {0x8e, 0xe1, 0xd1, 0x4b, 0x5e, 0xf1, 0x13, 0x01}};

//---------------------------------------------------------------------
// Unicode byte order mark
//---------------------------------------------------------------------
extern const WCHAR UnicodeByteOrderMark = 0xFEFF;

//---------------------------------------------------------------------
// dictionary table delimiter
//---------------------------------------------------------------------
extern const WCHAR KeywordDelimiter = L'=';
extern const WCHAR StringDelimiter = L'\"';

//---------------------------------------------------------------------
// defined item in setting file table [PreservedKey] section
//---------------------------------------------------------------------
extern const WCHAR ImeModeDescription[] = L"Chinese/English input (Shift)";
extern const WCHAR ImeModeDescription02[] = L"Chinese/English input (Ctrl+Alt+Space)";
extern const WCHAR ImeModeDescription03[] = L"Chinese/English input (Ctrl)";
extern const WCHAR EnglishInputModeDescription[] = L"国际音标输入（Ctrl+Shift+I）";
extern const int ImeModeOnIcoIndex = IME_MODE_ON_ICON_INDEX;
extern const int ImeModeOffIcoIndex = IME_MODE_OFF_ICON_INDEX;

extern const WCHAR DoubleSingleByteDescription[] = L"Double/Single byte";
extern const int DoubleSingleByteOnIcoIndex = IME_DOUBLE_ON_INDEX;
extern const int DoubleSingleByteOffIcoIndex = IME_DOUBLE_OFF_INDEX;

extern const WCHAR PunctuationDescription[] = L"Chinese/English punctuation";
extern const int PunctuationOnIcoIndex = IME_PUNCTUATION_ON_INDEX;
extern const int PunctuationOffIcoIndex = IME_PUNCTUATION_OFF_INDEX;

//---------------------------------------------------------------------
// defined item in setting file table [LanguageBar] section
//---------------------------------------------------------------------
extern const WCHAR LangbarImeModeDescription[] = L"Conversion mode";
extern const WCHAR LangbarDoubleSingleByteDescription[] = L"Character width";
extern const WCHAR LangbarPunctuationDescription[] = L"Punctuation";

//---------------------------------------------------------------------
// defined full width characters for Double/Single byte conversion
//---------------------------------------------------------------------
extern const WCHAR FullWidthCharTable[] = {
    0x3000, // Full width space
    0xFF01, // ！
    0xFF02, // ＂
    0xFF03, // ＃
    0xFF04, // ＄
    0xFF05, // ％
    0xFF06, // ＆
    0xFF07, // ＇
    0xFF08, // （
    0xFF09, // ）
    0xFF0A, // ＊
    0xFF0B, // ＋
    0xFF0C, // ，
    0xFF0D, // －
    0xFF0E, // ．
    0xFF0F, // ／

    0xFF10, // ０
    0xFF11, // １
    0xFF12, // ２
    0xFF13, // ３
    0xFF14, // ４
    0xFF15, // ５
    0xFF16, // ６
    0xFF17, // ７
    0xFF18, // ８
    0xFF19, // ９
    0xFF1A, // ：
    0xFF1B, // ；
    0xFF1C, // ＜
    0xFF1D, // ＝
    0xFF1E, // ＞
    0xFF1F, // ？

    0xFF20, // ＠
    0xFF21, // Ａ
    0xFF22, // Ｂ
    0xFF23, // Ｃ
    0xFF24, // Ｄ
    0xFF25, // Ｅ
    0xFF26, // Ｆ
    0xFF27, // Ｇ
    0xFF28, // Ｈ
    0xFF29, // Ｉ
    0xFF2A, // Ｊ
    0xFF2B, // Ｋ
    0xFF2C, // Ｌ
    0xFF2D, // Ｍ
    0xFF2E, // Ｎ
    0xFF2F, // Ｏ

    0xFF30, // Ｐ
    0xFF31, // Ｑ
    0xFF32, // Ｒ
    0xFF33, // Ｓ
    0xFF34, // Ｔ
    0xFF35, // Ｕ
    0xFF36, // Ｖ
    0xFF37, // Ｗ
    0xFF38, // Ｘ
    0xFF39, // Ｙ
    0xFF3A, // Ｚ
    0xFF3B, // ［
    0xFF3C, // ＼
    0xFF3D, // ］
    0xFF3E, // ＾
    0xFF3F, // ＿

    0xFF40, // ｀
    0xFF41, // ａ
    0xFF42, // ｂ
    0xFF43, // ｃ
    0xFF44, // ｄ
    0xFF45, // ｅ
    0xFF46, // ｆ
    0xFF47, // ｇ
    0xFF48, // ｈ
    0xFF49, // ｉ
    0xFF4A, // ｊ
    0xFF4B, // ｋ
    0xFF4C, // ｌ
    0xFF4D, // ｍ
    0xFF4E, // ｎ
    0xFF4F, // ｏ

    0xFF50, // ｐ
    0xFF51, // ｑ
    0xFF52, // ｒ
    0xFF53, // ｓ
    0xFF54, // ｔ
    0xFF55, // ｕ
    0xFF56, // ｖ
    0xFF57, // ｗ
    0xFF58, // ｘ
    0xFF59, // ｙ
    0xFF5A, // ｚ
    0xFF5B, // ｛
    0xFF5C, // ｜
    0xFF5D, // ｝
    0xFF5E  // ～
};

//---------------------------------------------------------------------
// defined punctuation characters
//---------------------------------------------------------------------
// '<' and '>' are deliberately absent: they belong to the nest pair set up in
// SetupPunctuationPair, and GetPunctuation searches this table first. Listing them here
// would shadow the nest pair, and the nested marks would never be produced.
extern const struct _PUNCTUATION PunctuationTable[23] = {
    {L'`', L"·"},   // ·
    {L'~', L"~"},   // ~
    {L'!', L"！"},  // ！
    {L'@', L"@"},   // @
    {L'#', L"#"},   // #
    {L'$', L"￥"},  // ￥
    {L'%', L"%"},   // %
    {L'^', L"……"},  // ……
    {L'&', L"&"},   // &
    {L'*', L"*"},   // *
    {L'(', L"（"},  // （
    {L')', L"）"},  // ）
    {L'_', L"——"},  // ——
    {L'[', L"【"},  // 【
    {L']', L"】"},  // 】
    {L'{', L"{"},   // {
    {L'}', L"}"},   // }
    {L'\\', L"、"}, // 、
    {L';', L"；"},  // ；
    {L':', L"："},  // ：
    {L',', L"，"},  // ，
    {L'.', L"。"},  // 。
    {L'?', L"？"},  // ？
};

//
// Will commit the highlighted candidate string with a punctuation character.
//
extern const std::unordered_set<WCHAR> CommitWithHighlightedCandPunc = {
    L'`',  //
    L'!',  //
    L'@',  //
    L'#',  //
    L'$',  //
    L'%',  //
    L'^',  //
    L'&',  //
    L'*',  //
    L'(',  //
    L')',  //
    L'-',  //
    L'_',  //
    L'=',  //
    L'+',  //
    L'[',  //
    L']',  //
    L'\\', //
    L'/',  //
    L';',  //
    L':',  //
    L'\'', //
    L'"',  //
    L',',  //
    L'<',  //
    L'.',  //
    L'>',  //
    L'?'   //
};

//+---------------------------------------------------------------------------
//
// CheckModifiers
//
//----------------------------------------------------------------------------

#define TF_MOD_ALLALT (TF_MOD_RALT | TF_MOD_LALT | TF_MOD_ALT)
#define TF_MOD_ALLCONTROL (TF_MOD_RCONTROL | TF_MOD_LCONTROL | TF_MOD_CONTROL)
#define TF_MOD_ALLSHIFT (TF_MOD_RSHIFT | TF_MOD_LSHIFT | TF_MOD_SHIFT)
#define TF_MOD_RLALT (TF_MOD_RALT | TF_MOD_LALT)
#define TF_MOD_RLCONTROL (TF_MOD_RCONTROL | TF_MOD_LCONTROL)
#define TF_MOD_RLSHIFT (TF_MOD_RSHIFT | TF_MOD_LSHIFT)

#define CheckMod(m0, m1, mod)                                                                                          \
    if (m1 & TF_MOD_##mod##)                                                                                           \
    {                                                                                                                  \
        if (!(m0 & TF_MOD_##mod##))                                                                                    \
        {                                                                                                              \
            return FALSE;                                                                                              \
        }                                                                                                              \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
        if ((m1 ^ m0) & TF_MOD_RL##mod##)                                                                              \
        {                                                                                                              \
            return FALSE;                                                                                              \
        }                                                                                                              \
    }

BOOL CheckModifiers(UINT modCurrent, UINT mod)
{
    mod &= ~TF_MOD_ON_KEYUP;

    if (mod & TF_MOD_IGNORE_ALL_MODIFIER)
    {
        return TRUE;
    }

    if (modCurrent == mod)
    {
        return TRUE;
    }

    if (modCurrent && !mod)
    {
        return FALSE;
    }

    CheckMod(modCurrent, mod, ALT);
    CheckMod(modCurrent, mod, SHIFT);
    CheckMod(modCurrent, mod, CONTROL);

    return TRUE;
}

//+---------------------------------------------------------------------------
//
// UpdateModifiers
//
//    wParam - virtual-key code
//    lParam - [0-15]  Repeat count
//  [16-23] Scan code
//  [24]    Extended key
//  [25-28] Reserved
//  [29]    Context code
//  [30]    Previous key state
//  [31]    Transition state
//----------------------------------------------------------------------------

thread_local USHORT ModifiersValue = 0;
thread_local BOOL IsShiftKeyDownOnly = FALSE;
thread_local BOOL IsControlKeyDownOnly = FALSE;
thread_local BOOL IsAltKeyDownOnly = FALSE;
thread_local BOOL PureShiftKeyDown = FALSE;
thread_local BOOL PureShiftKeyUp = FALSE;

BOOL UpdateModifiers(WPARAM wParam, LPARAM lParam)
{
    // high-order bit : key down
    // low-order bit  : toggled
    SHORT sksMenu = GetKeyState(VK_MENU);
    SHORT sksCtrl = GetKeyState(VK_CONTROL);
    SHORT sksShft = GetKeyState(VK_SHIFT);

    PureShiftKeyUp = FALSE;

    switch (wParam & 0xff)
    {
    case VK_MENU:
        // is VK_MENU down?
        if (sksMenu & 0x8000)
        {
            // is extended key?
            if (lParam & 0x01000000)
            {
                ModifiersValue |= (TF_MOD_RALT | TF_MOD_ALT);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LALT | TF_MOD_ALT);
            }

            // is previous key state up?
            if (!(lParam & 0x40000000))
            {
                // is VK_CONTROL and VK_SHIFT up?
                if (!(sksCtrl & 0x8000) && !(sksShft & 0x8000))
                {
                    IsAltKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        break;

    case VK_CONTROL:
        // is VK_CONTROL down?
        if (sksCtrl & 0x8000)
        {
            // is extended key?
            if (lParam & 0x01000000)
            {
                ModifiersValue |= (TF_MOD_RCONTROL | TF_MOD_CONTROL);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LCONTROL | TF_MOD_CONTROL);
            }

            // is previous key state up?
            if (!(lParam & 0x40000000))
            {
                // is VK_SHIFT and VK_MENU up?
                if (!(sksShft & 0x8000) && !(sksMenu & 0x8000))
                {
                    IsControlKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        break;

    case VK_SHIFT: {
        // is VK_SHIFT down?
        if (sksShft & 0x8000)
        {
            PureShiftKeyDown = TRUE;
            // is scan code 0x36(right shift)?
            if (((lParam >> 16) & 0x00ff) == 0x36)
            {
                ModifiersValue |= (TF_MOD_RSHIFT | TF_MOD_SHIFT);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LSHIFT | TF_MOD_SHIFT);
            }

            // is previous key state up?
            if (!(lParam & 0x40000000))
            {
                // is VK_MENU and VK_CONTROL up?
                if (!(sksMenu & 0x8000) && !(sksCtrl & 0x8000))
                {
                    IsShiftKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        else
        {
            if (PureShiftKeyDown)
                PureShiftKeyUp = TRUE;
        }
        break;
    }

    default:
        IsShiftKeyDownOnly = FALSE;
        IsControlKeyDownOnly = FALSE;
        IsAltKeyDownOnly = FALSE;
        PureShiftKeyDown = FALSE;
        break;
    }

    if (!(sksMenu & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLALT;
    }
    if (!(sksCtrl & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLCONTROL;
    }
    if (!(sksShft & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLSHIFT;
    }

    return TRUE;
}

//---------------------------------------------------------------------
// override CompareElements
//---------------------------------------------------------------------
BOOL CompareElements(LCID locale, const CStringRange *pElement1, const CStringRange *pElement2)
{
    return (CStringRange::Compare(locale, (CStringRange *)pElement1, (CStringRange *)pElement2) == CSTR_EQUAL) ? TRUE
                                                                                                               : FALSE;
}
} // namespace Global
