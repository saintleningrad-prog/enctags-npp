// ============================================================
//  EncTags — Notepad++ Plugin
//  Encrypts/decrypts inline ^^...^^ tags in source files
//
//  DLL entry point + Notepad++ plugin interface
// ============================================================

#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <sstream>

#include "../include/Scintilla.h"
#include "../include/PluginInterface.h"
#include "EncTagsEngine.h"
#include "TagParser.h"
#include "FragmentRegistry.h"
#include "resource.h"

// ============================================================
//  Global state
// ============================================================

static HINSTANCE g_hModule        = NULL;
static NppData   g_nppData        = {};
static bool      g_isDecrypted    = false;
static std::string g_cachedPassword;
static FragmentRegistry g_registry;

const TCHAR PLUGIN_NAME[] = _T("EncTags");
const int   NUM_MENU_ITEMS = 5;
static FuncItem g_funcItems[NUM_MENU_ITEMS];

// Forward declarations — menu commands
void cmdEncryptSelection();
void cmdDecryptAll();
void cmdLockAll();
void cmdRemoveEncryption();
void cmdAbout();

// ============================================================
//  Helper: get current Scintilla handle
// ============================================================

static HWND GetCurrentScintilla() {
    int currentEdit = 0;
    ::SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    return (currentEdit == 0) ? g_nppData._scintillaMainHandle
                              : g_nppData._scintillaSecondHandle;
}

// Send message to Scintilla
static LRESULT SCI(HWND hSci, UINT msg, WPARAM wp = 0, LPARAM lp = 0) {
    return ::SendMessage(hSci, msg, wp, lp);
}

// Get all text from the current editor
static std::string GetAllText() {
    HWND hSci = GetCurrentScintilla();
    int len = (int)SCI(hSci, SCI_GETLENGTH);
    if (len <= 0) return "";

    std::string buf(len + 1, '\0');
    SCI(hSci, SCI_GETTEXT, len + 1, (LPARAM)buf.data());
    buf.resize(len);
    return buf;
}

// Get selected text
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

// Replace selected text
static void ReplaceSelection(const std::string& text) {
    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_REPLACESEL, 0, (LPARAM)text.c_str());
}

// Replace a specific range in the document
static void ReplaceRange(int start, int end, const std::string& text) {
    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_SETTARGETSTART, start);
    SCI(hSci, SCI_SETTARGETEND, end);
    SCI(hSci, SCI_REPLACETARGET, (WPARAM)text.size(), (LPARAM)text.c_str());
}

// ============================================================
//  Password dialog
// ============================================================

static std::string g_dlgPassword;
static int g_dlgTagCount = 0;

static INT_PTR CALLBACK PasswordDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // Set tag count label
        TCHAR label[128];
        _stprintf_s(label, _T("Encrypted tags found: %d"), g_dlgTagCount);
        SetDlgItemText(hDlg, IDC_TAG_COUNT_LABEL, label);

        // Default to LOCAL mode
        CheckRadioButton(hDlg, IDC_MODE_LOCAL, IDC_MODE_OPRF, IDC_MODE_LOCAL);

        // Focus password field
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

// Show password dialog, returns true if user entered password
static bool AskPassword(int tagCount = 0) {
    if (!g_cachedPassword.empty()) return true;  // Already have password

    g_dlgTagCount = tagCount;
    g_dlgPassword.clear();

    INT_PTR result = DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_PASSWORD_DLG),
                                     g_nppData._nppHandle, PasswordDlgProc, 0);

    if (result == IDOK && !g_dlgPassword.empty()) {
        g_cachedPassword = g_dlgPassword;
        return true;
    }
    return false;
}

// ============================================================
//  Menu command: Encrypt Selection  (Ctrl+Shift+E)
// ============================================================

void cmdEncryptSelection() {
    std::string selected = GetSelectedText();
    if (selected.empty()) {
        MessageBox(g_nppData._nppHandle,
            _T("Select text to encrypt."),
            PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Check if user is trying to encrypt an already encrypted tag
    if (TagParser::IsEncryptedTag(selected)) {
        int answer = MessageBox(g_nppData._nppHandle,
            _T("Selected text is already an encrypted tag.\nEncrypt again?"),
            PLUGIN_NAME, MB_YESNO | MB_ICONQUESTION);
        if (answer != IDYES) return;
    }

    // Ask for password
    if (!AskPassword()) return;

    // Encrypt
    auto result = EncTagsEngine::Encrypt(selected, g_cachedPassword);

    if (!result.success) {
        MessageBoxA(g_nppData._nppHandle, result.error.c_str(), "EncTags Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Replace selection with encrypted tag
    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_BEGINUNDOACTION);
    ReplaceSelection(result.tag);
    SCI(hSci, SCI_ENDUNDOACTION);
}

// ============================================================
//  Menu command: Decrypt All Tags
// ============================================================

void cmdDecryptAll() {
    std::string text = GetAllText();
    if (text.empty()) return;

    // Find all tags
    auto tags = TagParser::FindTags(text);
    if (tags.empty()) {
        MessageBox(g_nppData._nppHandle,
            _T("No encrypted tags found."),
            PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Ask for password
    if (!AskPassword((int)tags.size())) return;

    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_BEGINUNDOACTION);

    // Clear previous registry
    g_registry.Clear();

    // Process tags in REVERSE order (so positions don't shift)
    for (int i = (int)tags.size() - 1; i >= 0; i--) {
        auto& tag = tags[i];

        // Try to decrypt
        auto result = EncTagsEngine::Decrypt(tag.payload, g_cachedPassword);

        if (result.success) {
            // Build the original full tag string for re-encryption
            std::string originalTag = "^^" + tag.payload + "^^";

            // Replace tag with decrypted text in buffer
            ReplaceRange(tag.startPos, tag.endPos, result.plaintext);

            // Register fragment for re-encryption on save
            g_registry.Add(tag.startPos, (int)result.plaintext.size(),
                          originalTag, result.plaintext);

            // Set indicator on the decrypted text for visual feedback
            SCI(hSci, SCI_SETINDICATORCURRENT, 9);  // Indicator slot 9
            SCI(hSci, SCI_INDICATORFILLRANGE, tag.startPos, (LPARAM)result.plaintext.size());
        }
    }

    SCI(hSci, SCI_ENDUNDOACTION);

    // Set indicator style (subtle green background)
    SCI(hSci, SCI_INDICSETSTYLE, 9, INDIC_ROUNDBOX);
    SCI(hSci, SCI_INDICSETFORE,  9, RGB(100, 200, 100));
    SCI(hSci, SCI_INDICSETALPHA, 9, 40);
    SCI(hSci, SCI_INDICSETOUTLINEALPHA, 9, 100);

    g_isDecrypted = true;

    // Status bar update
    TCHAR status[128];
    _stprintf_s(status, _T("EncTags: %d tags decrypted"), g_registry.Count());
    ::SendMessage(g_nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)status);
}

// ============================================================
//  Menu command: Lock (re-encrypt all back)
// ============================================================

void cmdLockAll() {
    if (!g_isDecrypted || g_registry.Count() == 0) {
        MessageBox(g_nppData._nppHandle,
            _T("No decrypted tags."),
            PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_BEGINUNDOACTION);

    // Get current text to check for edits
    std::string currentText = GetAllText();

    // Process fragments in REVERSE order
    auto& frags = g_registry.GetAll();
    for (int i = (int)frags.size() - 1; i >= 0; i--) {
        auto& frag = frags[i];
        if (!frag.active) continue;

        // Get current text at fragment position
        int fragEnd = frag.startPos + frag.length;
        if (frag.startPos < 0 || fragEnd > (int)currentText.size()) continue;

        std::string currentFragment = currentText.substr(frag.startPos, frag.length);

        std::string tagToInsert;

        if (currentFragment == frag.decryptedText) {
            // Text unchanged — use original tag
            tagToInsert = frag.originalTag;
        } else {
            // Text was edited — re-encrypt with new content
            auto result = EncTagsEngine::Encrypt(currentFragment, g_cachedPassword);
            if (result.success) {
                tagToInsert = result.tag;
            } else {
                tagToInsert = frag.originalTag;  // Fallback to original
            }
        }

        ReplaceRange(frag.startPos, fragEnd, tagToInsert);

        // Clear indicator
        SCI(hSci, SCI_SETINDICATORCURRENT, 9);
        SCI(hSci, SCI_INDICATORCLEARRANGE, frag.startPos, (LPARAM)tagToInsert.size());
    }

    SCI(hSci, SCI_ENDUNDOACTION);

    g_registry.Clear();
    g_isDecrypted = false;

    ::SendMessage(g_nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE,
                  (LPARAM)_T("EncTags: all tags encrypted"));
}

// ============================================================
//  Menu command: Remove Encryption (keep as plain text)
// ============================================================

void cmdRemoveEncryption() {
    if (!g_isDecrypted) {
        MessageBox(g_nppData._nppHandle,
            _T("Decrypt tags first (Decrypt All)."),
            PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    HWND hSci = GetCurrentScintilla();
    int curPos = (int)SCI(hSci, SCI_GETCURRENTPOS);

    int idx = g_registry.FindAt(curPos);
    if (idx < 0) {
        MessageBox(g_nppData._nppHandle,
            _T("Cursor is not inside an encrypted fragment."),
            PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Remove from registry — this fragment won't be re-encrypted on save
    auto& frag = g_registry.GetAll()[idx];

    // Clear indicator
    SCI(hSci, SCI_SETINDICATORCURRENT, 9);
    SCI(hSci, SCI_INDICATORCLEARRANGE, frag.startPos, (LPARAM)frag.length);

    g_registry.Remove(idx);

    MessageBox(g_nppData._nppHandle,
        _T("Encryption removed. Text will stay as plain text on save."),
        PLUGIN_NAME, MB_OK | MB_ICONINFORMATION);
}

// ============================================================
//  Menu command: About
// ============================================================

void cmdAbout() {
    MessageBox(g_nppData._nppHandle,
        _T("EncTags v0.1.0\n\n")
        _T("Encrypts fragments of text inside files\n")
        _T("using ^^...^^ tags\n\n")
        _T("AES-256-GCM + PBKDF2-SHA256\n\n")
        _T("https://github.com/enctags"),
        _T("About EncTags"), MB_OK | MB_ICONINFORMATION);
}

// ============================================================
//  Notification handler: auto re-encrypt before save
// ============================================================

static void OnBeforeSave() {
    if (!g_isDecrypted || g_registry.Count() == 0) return;

    // Re-encrypt all fragments before saving
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
            auto result = EncTagsEngine::Encrypt(currentFragment, g_cachedPassword);
            tagToInsert = result.success ? result.tag : frag.originalTag;
        }

        ReplaceRange(frag.startPos, fragEnd, tagToInsert);
    }

    SCI(hSci, SCI_ENDUNDOACTION);
}

static void OnAfterSave() {
    if (!g_isDecrypted || g_registry.Count() == 0) return;

    // After save, decrypt back for the user
    // We need to re-scan because positions changed
    g_registry.Clear();

    std::string text = GetAllText();
    auto tags = TagParser::FindTags(text);

    HWND hSci = GetCurrentScintilla();
    SCI(hSci, SCI_BEGINUNDOACTION);

    for (int i = (int)tags.size() - 1; i >= 0; i--) {
        auto& tag = tags[i];
        auto result = EncTagsEngine::Decrypt(tag.payload, g_cachedPassword);

        if (result.success) {
            std::string originalTag = "^^" + tag.payload + "^^";
            ReplaceRange(tag.startPos, tag.endPos, result.plaintext);
            g_registry.Add(tag.startPos, (int)result.plaintext.size(),
                          originalTag, result.plaintext);

            SCI(hSci, SCI_SETINDICATORCURRENT, 9);
            SCI(hSci, SCI_INDICATORFILLRANGE, tag.startPos, (LPARAM)result.plaintext.size());
        }
    }

    SCI(hSci, SCI_ENDUNDOACTION);
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

    // Shortcut: Ctrl+Shift+E for Encrypt
    static ShortcutKey skEncrypt = { true, false, true, 'E' };
    // Shortcut: Ctrl+Shift+D for Decrypt
    static ShortcutKey skDecrypt = { true, false, true, 'D' };
    // Shortcut: Ctrl+Shift+L for Lock
    static ShortcutKey skLock    = { true, false, true, 'L' };

    _tcscpy_s(g_funcItems[0]._itemName, _T("Encrypt Selection"));
    g_funcItems[0]._pFunc     = (PFUNCSETINFO)cmdEncryptSelection;
    g_funcItems[0]._cmdID     = 0;
    g_funcItems[0]._init2Check = false;
    g_funcItems[0]._pShKey    = &skEncrypt;

    _tcscpy_s(g_funcItems[1]._itemName, _T("Decrypt All"));
    g_funcItems[1]._pFunc     = (PFUNCSETINFO)cmdDecryptAll;
    g_funcItems[1]._cmdID     = 0;
    g_funcItems[1]._init2Check = false;
    g_funcItems[1]._pShKey    = &skDecrypt;

    _tcscpy_s(g_funcItems[2]._itemName, _T("Lock (Re-encrypt)"));
    g_funcItems[2]._pFunc     = (PFUNCSETINFO)cmdLockAll;
    g_funcItems[2]._cmdID     = 0;
    g_funcItems[2]._init2Check = false;
    g_funcItems[2]._pShKey    = &skLock;

    _tcscpy_s(g_funcItems[3]._itemName, _T("Remove Encryption"));
    g_funcItems[3]._pFunc     = (PFUNCSETINFO)cmdRemoveEncryption;
    g_funcItems[3]._cmdID     = 0;
    g_funcItems[3]._init2Check = false;
    g_funcItems[3]._pShKey    = NULL;

    _tcscpy_s(g_funcItems[4]._itemName, _T("About..."));
    g_funcItems[4]._pFunc     = (PFUNCSETINFO)cmdAbout;
    g_funcItems[4]._cmdID     = 0;
    g_funcItems[4]._init2Check = false;
    g_funcItems[4]._pShKey    = NULL;

    return g_funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notification) {
    switch (notification->nmhdr.code) {
    case NPPN_FILEBEFORESAVE:
        OnBeforeSave();
        break;
    case NPPN_FILESAVED:
        OnAfterSave();
        break;
    case NPPN_FILECLOSED:
    case NPPN_FILEBEFORECLOSE:
        g_registry.Clear();
        g_isDecrypted = false;
        break;
    case NPPN_BUFFERACTIVATED:
        // When switching tabs, reset state
        g_registry.Clear();
        g_isDecrypted = false;
        break;
    case NPPN_SHUTDOWN:
        g_cachedPassword.clear();
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
        g_cachedPassword.clear();
        break;
    }
    return TRUE;
}
