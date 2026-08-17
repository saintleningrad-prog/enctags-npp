// PluginInterface.h — Notepad++ Plugin Interface
// Based on the official Notepad++ plugin template
#pragma once

#include <windows.h>

const int nbChar = 64;

typedef const TCHAR* (*PFUNCGETNAME)();

struct NppData {
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

typedef void (__cdecl *PFUNCSETINFO)(NppData);
typedef void (__cdecl *BENOTIFIED)(SCNotification*);
typedef LRESULT (__cdecl *PMESSAGEPROC)(UINT Message, WPARAM wParam, LPARAM lParam);

struct ShortcutKey {
    bool _isCtrl;
    bool _isAlt;
    bool _isShift;
    UCHAR _key;
};

struct FuncItem {
    TCHAR _itemName[nbChar];
    PFUNCSETINFO _pFunc;
    int _cmdID;
    bool _init2Check;
    ShortcutKey* _pShKey;
};

// Notepad++ messages
#define NPPM_GETCURRENTSCINTILLA  (WM_USER + 1000 + 4)
#define NPPM_MENUCOMMAND          (WM_USER + 1000 + 48)
#define NPPM_GETPLUGINSCONFIGDIR  (WM_USER + 1000 + 46)
#define NPPM_GETCURRENTLANGTYPE   (WM_USER + 1000 + 5)
#define NPPM_SETSTATUSBAR         (WM_USER + 1000 + 24)
#define STATUSBAR_DOC_TYPE        0

// Notepad++ notification codes
#define NPPN_FIRST               1000
#define NPPN_READY               (NPPN_FIRST + 1)
#define NPPN_TBMODIFICATION      (NPPN_FIRST + 2)
#define NPPN_FILEBEFOREOPEN      (NPPN_FIRST + 3)
#define NPPN_FILEOPENED          (NPPN_FIRST + 4)
#define NPPN_FILECLOSED          (NPPN_FIRST + 5)
#define NPPN_FILEBEFORECLOSE     (NPPN_FIRST + 6)
#define NPPN_FILEBEFORESAVE      (NPPN_FIRST + 7)
#define NPPN_FILESAVED           (NPPN_FIRST + 8)
#define NPPN_SHUTDOWN            (NPPN_FIRST + 9)
#define NPPN_BUFFERACTIVATED     (NPPN_FIRST + 10)
#define NPPN_LANGCHANGED         (NPPN_FIRST + 11)
#define NPPN_WORDSTYLESUPDATED   (NPPN_FIRST + 12)
#define NPPN_SHORTCUTREMAPPED    (NPPN_FIRST + 13)
#define NPPN_FILEBEFORELOAD      (NPPN_FIRST + 14)
#define NPPN_FILELOADFAILED      (NPPN_FIRST + 15)
#define NPPN_READONLYCHANGED     (NPPN_FIRST + 16)
