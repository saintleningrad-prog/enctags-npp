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

static INT_PTR CALLBACK PasswordDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        TCHAR label[128];
        _stprintf_s(label, _T("Encrypted tags found: %d"), g_dlgTagCount);
        SetDlgItemText(hDlg, IDC_TAG_COUNT_LABEL, label);
        CheckRadioButton(hDlg, IDC_MODE_LOCAL, IDC_MODE_OPRF, IDC_MODE_LOCAL);
        SetFocus(GetDlgItem(hDlg, IDC_PASSWORD_EDIT));
        return FALSE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            char buf[512] = {};
            GetDlgItemTextA(hDlg, IDC_PASSWORD_EDIT, buf, sizeof(buf));
            g_dlgPassword = buf;
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
// Returns empty string if user cancelled.
static std::string AskPasswordDialog(int tagCount) {
    g_dlgTagCount = tagCount;
    g_dlgPassword.clear();

    INT_PTR result = DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_PASSWORD_DLG),
                                     g_nppData._nppHandle, PasswordDlgProc, 0);

    if (result == IDOK) return g_dlgPassword;
    return "";
}

// ============================================================
//  Close-decision dialog (3 options)
// ============================================================

enum class CloseChoice { Encrypt, Return, Ignore, Cancelled };
static CloseChoice g_closeChoice = CloseChoice::Cancelled;
static int g_closeBlockCount = 0;

static INT_PTR CALLBACK CloseDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        TCHAR label[128];
        _stprintf_s(label, _T("Decrypted blocks found: %d"), g_closeBlockCount);
        SetDlgItemText(hDlg, IDC_CLOSE_LABEL, label);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_ENCRYPT:
            g_closeChoice = CloseChoice::Encrypt;
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDC_BTN_RETURN:
            g_closeChoice = CloseChoice::Return;
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDC_BTN_IGNORE:
            g_closeChoice = CloseChoice::Ignore;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static CloseChoice AskCloseDecision(int blockCount) {
    g_closeBlockCount = blockCount;
    g_closeChoice = CloseChoice::Cancelled;

    DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_CLOSE_DLG),
                    g_nppData._nppHandle, CloseDlgProc, 0);

    return g_closeChoice;
}

// ============================================================
//  On file opened — find encrypted tags, ask password once,
//  decrypt everything that matches
// ============================================================

static void OnFileOpened() {
    std::string text = GetAllText();
    auto encTags = TagParser::FindEncryptedTags(text);

    if (encTags.empty()) return;

    std::string password = AskPasswordDialog((int)encTags.size());
    if (password.empty()) return;  // user cancelled

    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_BEGINUNDOACTION);

    int decryptedCount = 0;

    // Process in reverse so earlier positions stay valid
    for (int i = (int)encTags.size() - 1; i >= 0; i--) {
        auto& tag = encTags[i];
        auto result = EncTagsEngine::Decrypt(tag.payload, password);

        if (result.success) {
            std::string originalTag = "^^" + tag.payload + "^^";
            ReplaceRange(tag.startPos, tag.endPos, result.plaintext);
            g_registry.Add(tag.startPos, (int)result.plaintext.size(),
                          originalTag, result.plaintext, password);
            SetIndicator(tag.startPos, (int)result.plaintext.size());
            decryptedCount++;
        }
        // Non-matching tags are left as-is — user handles via hotkey
    }

    SCI(hSci, SCI_ENDUNDOACTION);

    TCHAR status[160];
    _stprintf_s(status, _T("EncTags: decrypted %d of %d tags"),
                decryptedCount, (int)encTags.size());
    ::SendMessage(g_nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)status);
}

// ============================================================
//  Ctrl+Shift+E — toggle the tag under the cursor
// ============================================================

void cmdToggleAtCursor() {
    HWND hSci = GetCurrentScintilla();
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
        std::string password = AskPasswordDialog(0);
        if (password.empty()) return;

        auto result = EncTagsEngine::Decrypt(tag.payload, password);
        if (!result.success) {
            MessageBox(g_nppData._nppHandle,
                _T("Wrong password or corrupted data."),
                PLUGIN_NAME, MB_OK | MB_ICONERROR);
            return;
        }

        std::string originalTag = "^^" + tag.payload + "^^";

        SCI(hSci, SCI_BEGINUNDOACTION);
        ReplaceRange(tag.startPos, tag.endPos, result.plaintext);
        SCI(hSci, SCI_ENDUNDOACTION);

        g_registry.Add(tag.startPos, (int)result.plaintext.size(),
                      originalTag, result.plaintext, password);
        SetIndicator(tag.startPos, (int)result.plaintext.size());

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
//  Close / Save handling
// ============================================================

// Returns false if the close/save should be cancelled (user chose "Return")
static bool HandleCloseOrSave() {
    int count = g_registry.Count();
    if (count == 0) return true;  // nothing decrypted, proceed normally

    CloseChoice choice = AskCloseDecision(count);

    switch (choice) {
    case CloseChoice::Return:
        return false;  // cancel close/save

    case CloseChoice::Ignore:
        // Leave everything as plaintext, proceed with close/save
        g_registry.Clear();
        return true;

    case CloseChoice::Encrypt: {
        HWND hSci = GetCurrentScintilla();
        std::string currentText = GetAllText();

        SCI(hSci, SCI_BEGINUNDOACTION);

        auto& frags = g_registry.GetAll();
        for (int i = (int)frags.size() - 1; i >= 0; i--) {
            auto& frag = frags[i];
            if (!frag.active) continue;

            int fragEnd = frag.startPos + frag.length;
            if (frag.startPos < 0 || fragEnd > (int)currentText.size()) continue;

            std::string currentFragment = currentText.substr(frag.startPos, frag.length);
            std::string tagToInsert;

            if (currentFragment == frag.decryptedText) {
                tagToInsert = frag.originalTag;
            } else {
                auto result = EncTagsEngine::Encrypt(currentFragment, frag.password);
                tagToInsert = result.success ? result.tag : frag.originalTag;
            }

            ReplaceRange(frag.startPos, fragEnd, tagToInsert);
        }

        SCI(hSci, SCI_ENDUNDOACTION);
        g_registry.Clear();
        return true;
    }

    case CloseChoice::Cancelled:
    default:
        return false;  // dialog dismissed without a choice — treat as cancel
    }
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
    case NPPN_FILEOPENED:
        OnFileOpened();
        break;

    case NPPN_FILEBEFORESAVE:
        // NOTE: Notepad++'s plugin API does not let FILEBEFORESAVE cancel
        // the save. We resolve decrypted fragments here; if the user
        // chooses "Return", we simply leave text decrypted (it will be
        // saved as plaintext) and rely on FILEBEFORECLOSE for true
        // cancellable behavior when closing the tab.
        HandleCloseOrSave();
        break;

    case NPPN_FILEBEFORECLOSE:
        // Notepad++ does not support cancelling close via this
        // notification either; treat as best-effort prompt.
        HandleCloseOrSave();
        break;

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
