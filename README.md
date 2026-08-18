## EncTags v0.2.0

Encrypt fragments of text inline, right inside any file: code, config, plain text, using `^^...^^` tags. AES-256-GCM, no external dependencies.

### How it works

- Select text (or place your cursor inside a `^^...^^` tag) and press Ctrl+Shift+E
- Raw `^^text^^` becomes encrypted into `^^L1:base64...^^`
- Encrypted tag gets decrypted back to plain text (password prompt)
- Nothing happens automatically on open, save, or close: every action is explicit

### What's in this release

- One hotkey (Ctrl+Shift+E) handles encrypt, decrypt, and re-encrypt: no separate menu commands to remember
- Works on a text selection or on the tag under the cursor
- Optional "keep `^^ ^^` tags after decrypting" checkbox: leaves the wrapper visible instead of showing bare plaintext
- AES-256-GCM via Windows CNG (`bcrypt.dll`): no bundled crypto library
- PBKDF2-HMAC-SHA256, 100,000 iterations for key derivation
- Zero automatic behavior on file open/save/close: the plugin never touches your file unless you press the hotkey

### Install

Download `EncTags.dll` below, copy it to `<Notepad++>\plugins\EncTags\EncTags.dll`, restart Notepad++.

### Known limitations

- Windows only (relies on Notepad++ / Scintilla / BCrypt)
- One password per encrypted fragment, no batch "decrypt everything at once" yet
- Format is open and documented in the README: a Python or browser-based decryptor works the same way, no plugin required to read a tag elsewhere
- Corruption is unrecoverable, not silent-but-wrong. AES-GCM's authentication tag means any accidental edit inside a `^^L1:...^^` tag (line wrapping, partial copy-paste, a merge conflict) makes decryption fail outright rather than producing garbled text, but there is no way to partially recover or repair a damaged tag. Keep your file's git history if the encrypted content matters, it's the only recovery path.
