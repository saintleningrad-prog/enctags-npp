// Scintilla.h — Minimal subset of Scintilla messages needed by EncTags
// Full reference: https://scintilla.org/ScintillaDoc.html
#pragma once

// Text retrieval and modification
#define SCI_GETTEXT              2182
#define SCI_SETTEXT              2181
#define SCI_GETLENGTH            2006
#define SCI_GETTEXTRANGE         2162
#define SCI_GETSELTEXT           2161
#define SCI_REPLACESEL           2170
#define SCI_ADDTEXT              2001
#define SCI_INSERTTEXT           2003
#define SCI_DELETERANGE          2645

// Selection
#define SCI_GETSELECTIONSTART    2143
#define SCI_GETSELECTIONEND      2145
#define SCI_SETSEL               2160
#define SCI_GETANCHOR            2009

// Target-based search and replace
#define SCI_SETTARGETSTART       2190
#define SCI_SETTARGETEND         2192
#define SCI_GETTARGETSTART       2191
#define SCI_GETTARGETEND         2193
#define SCI_REPLACETARGET        2194
#define SCI_SEARCHINTARGET       2197
#define SCI_SETSEARCHFLAGS       2198

// Position / Line
#define SCI_GETCURRENTPOS        2008
#define SCI_GOTOPOS              2025
#define SCI_GETLINEFROMPOSITION  2166
#define SCI_POSITIONFROMLINE     2167
#define SCI_GETLINEENDPOSITION   2136
#define SCI_GETLINECOUNT         2154
#define SCI_LINELENGTH           2350
#define SCI_GETLINE              2153

// Indicators
#define SCI_INDICSETSTYLE        2080
#define SCI_INDICSETFORE         2082
#define SCI_INDICSETALPHA        2523
#define SCI_INDICSETOUTLINEALPHA 2558
#define SCI_INDICSETUNDER        2510
#define SCI_SETINDICATORCURRENT  2500
#define SCI_SETINDICATORVALUE    2502
#define SCI_INDICATORFILLRANGE   2504
#define SCI_INDICATORCLEARRANGE  2505
#define SCI_INDICATORVALUEAT     2506
#define SCI_INDICATORSTART       2507
#define SCI_INDICATOREND         2508
#define SCI_INDICSETSTYLE        2080
#define INDIC_ROUNDBOX           7
#define INDIC_STRAIGHTBOX        8
#define INDIC_FULLBOX            16
#define INDIC_TEXTFORE           17

// Undo
#define SCI_BEGINUNDOACTION      2078
#define SCI_ENDUNDOACTION        2079

// Encoding
#define SCI_GETCODEPAGE          2137

// Annotations
#define SCI_ANNOTATIONSETTEXT    2540
#define SCI_ANNOTATIONGETTEXT    2541
#define SCI_ANNOTATIONSETSTYLE   2542
#define SCI_ANNOTATIONSETVISIBLE 2548
#define ANNOTATION_STANDARD      1
#define ANNOTATION_BOXED         2

// Notifications
#define SCN_MODIFIED             2008
#define SCN_SAVEPOINTREACHED     2002
#define SCN_SAVEPOINTLEFT        2003

// Struct for SCI_GETTEXTRANGE
struct Sci_TextRange {
    struct Sci_CharacterRange {
        long cpMin;
        long cpMax;
    } chrg;
    char* lpstrText;
};

// SCNotification (simplified)
struct SCNotification {
    NMHDR nmhdr;
    int position;
    int ch;
    int modifiers;
    int modificationType;
    const char* text;
    int length;
    int linesAdded;
    int message;
    ULONG_PTR wParam;
    LONG_PTR lParam;
    int line;
    int foldLevelNow;
    int foldLevelPrev;
    int margin;
    int listType;
    int x;
    int y;
    int token;
    int annotationLinesAdded;
    int updated;
    int listCompletionMethod;
};
