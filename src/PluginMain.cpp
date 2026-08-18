// ============================================================
//  EncTags — Notepad++ Plugin
//  Encrypts/decrypts inline ^^...^^ tags in source files
//
//  Behavior:
//   - On file open: if encrypted tags found, ask for password,
//     decrypt everything that matches. Non-matching tags stay
//     encrypted (user handles them manually).
//   - Ctrl+Shift+E: toggle the tag under the cursor
//     (encrypted -> decrypt, raw -> encrypt, decrypted -> re-encrypt).
//   - On close/save: if decrypted fragments remain, ask:
//     1) Encrypt  2) Return to document  3) No (close as is)
// ============================================================

#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>

#include "../include/Scintilla.h"
#include "../include/PluginInterface.h"
#include "EncTagsEngine.h"
#include "TagParser.h"
#include "FragmentRegistry.h"
#include "resource.h"

// ============================================================
//  Global state
// ============================================================

static HINSTANCE g_hModule  = NULL;
static NppData   g_nppData  = {};
static FragmentRegistry g_registry;

const TCHAR PLUGIN_NAME[] = _T("EncTags");
const int   NUM_MENU_ITEMS = 2;
static FuncItem g_funcItems[NUM_MENU_ITEMS];

void cmdToggleAtCursor();
void cmdAbout();

// ============================================================
//  Scintilla helpers
// ============================================================

static HWND GetCurrentScintilla() {
    int currentEdit = 0;
    ::SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    return (currentEdit == 0) ? g_nppData._scintillaMainHandle
                              : g_nppData._scintillaSecondHandle;
}

static LRESULT SCI(HWND hSci, UINT msg, WPARAM wp = 0, LPARAM lp = 0) {
    return ::SendMessage(hSci, msg, wp, lp);
}

static std::string GetAllText() {
    HWND hSci = GetCurrentScintilla();
    int len = (int)SCI(hSci, SCI_GETLENGTH);
    if (len <= 0) return "";
    std::string buf(len + 1, '\0');
    SCI(hSci, SCI_GETTEXT, len + 1, (LPARAM)buf.data());
    buf.resize(len);
    return buf;
}

static void ReplaceRange(int start, int end, const std::string& text) {
    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_SETTARGETSTART, start);
    SCI(hSci, SCI_SETTARGETEND, end);
    SCI(hSci, SCI_REPLACETARGET, (WPARAM)text.size(), (LPARAM)text.c_str());
}

static void SetIndicator(int start, int length) {
    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_INDICSETSTYLE, 9, INDIC_ROUNDBOX);
    SCI(hSci, SCI_INDICSETFORE,  9, RGB(100, 200, 100));
    SCI(hSci, SCI_INDICSETALPHA, 9, 40);
    SCI(hSci, SCI_INDICSETOUTLINEALPHA, 9, 100);
    SCI(hSci, SCI_SETINDICATORCURRENT, 9);
    SCI(hSci, SCI_INDICATORFILLRANGE, start, (LPARAM)length);
}

static void ClearIndicator(int start, int length) {
    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_SETINDICATORCURRENT, 9);
    SCI(hSci, SCI_INDICATORCLEARRANGE, start, (LPARAM)length);
}

// ============================================================
//  Password dialog (single password prompt)
// ============================================================

static std::string g_dlgPassword;
static int g_dlgTagCount = 0;
static bool g_dlgKeepTags = false;

static INT_PTR CALLBACK PasswordDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        TCHAR label[128];
        _stprintf_s(label, _T("Encrypted tags found: %d"), g_dlgTagCount);
        SetDlgItemText(hDlg, IDC_TAG_COUNT_LABEL, label);
        CheckRadioButton(hDlg, IDC_MODE_LOCAL, IDC_MODE_OPRF, IDC_MODE_LOCAL);
        CheckDlgButton(hDlg, IDC_KEEP_TAGS, BST_UNCHECKED);  // default: off
        SetFocus(GetDlgItem(hDlg, IDC_PASSWORD_EDIT));
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            char buf[512] = {};
            GetDlgItemTextA(hDlg, IDC_PASSWORD_EDIT, buf, sizeof(buf));
            g_dlgPassword = buf;
            g_dlgKeepTags = (IsDlgButtonChecked(hDlg, IDC_KEEP_TAGS) == BST_CHECKED);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// Ask for a password. tagCount is shown in the label (0 = generic prompt).
// Returns empty string if user cancelled. keepTagsOut receives the checkbox state.
static std::string AskPasswordDialog(int tagCount, bool* keepTagsOut = nullptr) {
    g_dlgTagCount = tagCount;
    g_dlgPassword.clear();
    g_dlgKeepTags = false;

    INT_PTR result = DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_PASSWORD_DLG),
                                     g_nppData._nppHandle, PasswordDlgProc, 0);

    if (result == IDOK) {
        if (keepTagsOut) *keepTagsOut = g_dlgKeepTags;
        return g_dlgPassword;
    }
    return "";
}

// ============================================================
//  Ctrl+Shift+E — toggle the tag under the cursor
// ============================================================

// Get currently selected text (empty string if nothing selected)
static std::string GetSelectedText() {
    HWND hSci = GetCurrentScintilla();
    int selStart = (int)SCI(hSci, SCI_GETSELECTIONSTART);
    int selEnd   = (int)SCI(hSci, SCI_GETSELECTIONEND);
    if (selEnd <= selStart) return "";

    int len = selEnd - selStart;
    std::string buf(len + 1, '\0');
    SCI(hSci, SCI_GETSELTEXT, 0, (LPARAM)buf.data());
    buf.resize(len);
    return buf;
}

void cmdToggleAtCursor() {
    HWND hSci = GetCurrentScintilla();

    // Case 0: user has text selected — encrypt the selection directly,
    // no need to type ^^...^^ manually first
    std::string selected = GetSelectedText();
    if (!selected.empty()) {
        // If the selection already IS an encrypted tag, offer to
        // re-encrypt (rare, but handled for safety)
        if (TagParser::IsEncryptedTag(selected)) {
            int answer = MessageBox(g_nppData._nppHandle,
                _T("Selection is already an encrypted tag.\nEncrypt again?"),
                PLUGIN_NAME, MB_YESNO | MB_ICONQUESTION);
            if (answer != IDYES) return;
        }

        std::string password = AskPasswordDialog(0);
        if (password.empty()) return;

        auto result = EncTagsEngine::Encrypt(selected, password);
        if (!result.success) {
            MessageBoxA(g_nppData._nppHandle, result.error.c_str(),
                "EncTags Error", MB_OK | MB_ICONERROR);
            return;
        }

        SCI(hSci, SCI_BEGINUNDOACTION);
        SCI(hSci, SCI_REPLACESEL, 0, (LPARAM)result.tag.c_str());
        SCI(hSci, SCI_ENDUNDOACTION);
        return;
    }

    int curPos = (int)SCI(hSci, SCI_GETCURRENTPOS);

    // Case 1: cursor is inside an already-decrypted (managed) fragment
    //         -> re-encrypt just this one, using its stored password
    int fragIdx = g_registry.FindAt(curPos);
    if (fragIdx >= 0) {
        auto& frag = g_registry.GetAll()[fragIdx];
        std::string currentText = GetAllText();
        int fragEnd = frag.startPos + frag.length;

        if (fragEnd <= (int)currentText.size()) {
            std::string currentFragment = currentText.substr(frag.startPos, frag.length);
            std::string tagToInsert;

            if (currentFragment == frag.decryptedText) {
                tagToInsert = frag.originalTag;  // unchanged — restore original tag
            } else {
                auto result = EncTagsEngine::Encrypt(currentFragment, frag.password);
                tagToInsert = result.success ? result.tag : frag.originalTag;
            }

            SCI(hSci, SCI_BEGINUNDOACTION);
            ClearIndicator(frag.startPos, frag.length);
            ReplaceRange(frag.startPos, fragEnd, tagToInsert);
            SCI(hSci, SCI_ENDUNDOACTION);

            g_registry.Remove(fragIdx);
        }
        return;
    }

    // Case 2 & 3: cursor is inside a raw ^^text^^ or encrypted ^^L1:...^^ tag
    //             not yet tracked in the registry
    std::string text = GetAllText();
    auto allTags = TagParser::FindAllTags(text);
    int tagIdx = TagParser::FindTagAtPosition(allTags, curPos);

    if (tagIdx < 0) {
        MessageBox(g_nppData._nppHandle,
            _T("Cursor is not inside a ^^...^^ tag."),
            PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    auto& tag = allTags[tagIdx];

    if (tag.isEncrypted) {
        // Decrypt this single tag
        bool keepTags = false;
        std::string password = AskPasswordDialog(0, &keepTags);
        if (password.empty()) return;

        auto result = EncTagsEngine::Decrypt(tag.payload, password);
        if (!result.success) {
            MessageBox(g_nppData._nppHandle,
                _T("Wrong password or corrupted data."),
                PLUGIN_NAME, MB_OK | MB_ICONERROR);
            return;
        }

        std::string originalTag = "^^" + tag.payload + "^^";
        std::string displayText = keepTags ? ("^^" + result.plaintext + "^^")
                                            : result.plaintext;

        SCI(hSci, SCI_BEGINUNDOACTION);
        ReplaceRange(tag.startPos, tag.endPos, displayText);
        SCI(hSci, SCI_ENDUNDOACTION);

        if (!keepTags) {
            g_registry.Add(tag.startPos, (int)displayText.size(),
                          originalTag, displayText, password);
            SetIndicator(tag.startPos, (int)displayText.size());
        }

    } else {
        // Encrypt this raw tag's content
        std::string password = AskPasswordDialog(0);
        if (password.empty()) return;

        auto result = EncTagsEngine::Encrypt(tag.payload, password);
        if (!result.success) {
            MessageBoxA(g_nppData._nppHandle, result.error.c_str(),
                "EncTags Error", MB_OK | MB_ICONERROR);
            return;
        }

        SCI(hSci, SCI_BEGINUNDOACTION);
        ReplaceRange(tag.startPos, tag.endPos, result.tag);
        SCI(hSci, SCI_ENDUNDOACTION);
    }
}

// ============================================================
//  About
// ============================================================

void cmdAbout() {
    MessageBox(g_nppData._nppHandle,
        _T("EncTags v0.2.0\n\n")
        _T("Encrypts fragments of text inside files\n")
        _T("using ^^...^^ tags.\n\n")
        _T("Ctrl+Shift+E — toggle encryption on the tag\n")
        _T("under the cursor.\n\n")
        _T("AES-256-GCM + PBKDF2-SHA256\n\n")
        _T("https://github.com/enctags"),
        _T("About EncTags"), MB_OK | MB_ICONINFORMATION);
}

// ============================================================
//  DLL exports required by Notepad++
// ============================================================

extern "C" __declspec(dllexport) void setInfo(NppData nppData) {
    g_nppData = nppData;
}

extern "C" __declspec(dllexport) const TCHAR* getName() {
    return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {
    *nbF = NUM_MENU_ITEMS;

    static ShortcutKey skToggle = { true, false, true, 'E' };

    _tcscpy_s(g_funcItems[0]._itemName, _T("Toggle Encryption (cursor)"));
    g_funcItems[0]._pFunc      = (PFUNCSETINFO)cmdToggleAtCursor;
    g_funcItems[0]._cmdID      = 0;
    g_funcItems[0]._init2Check = false;
    g_funcItems[0]._pShKey     = &skToggle;

    _tcscpy_s(g_funcItems[1]._itemName, _T("About..."));
    g_funcItems[1]._pFunc      = (PFUNCSETINFO)cmdAbout;
    g_funcItems[1]._cmdID      = 0;
    g_funcItems[1]._init2Check = false;
    g_funcItems[1]._pShKey     = NULL;

    return g_funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notification) {
    switch (notification->nmhdr.code) {

    case NPPN_FILECLOSED:
        g_registry.Clear();
        break;

    case NPPN_BUFFERACTIVATED:
        g_registry.Clear();
        break;

    case NPPN_SHUTDOWN:
        g_registry.Clear();
        break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT msg, WPARAM wParam, LPARAM lParam) {
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}

// ============================================================
//  DllMain
// ============================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reasonForCall, LPVOID lpReserved) {
    switch (reasonForCall) {
    case DLL_PROCESS_ATTACH:
        g_hModule = (HINSTANCE)hModule;
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
