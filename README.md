# EncTags for Notepad++

Encrypt sensitive fragments of text inline, right inside any file — code, config, markdown, plain text — without touching the rest of the file.

```
TODO: prod DB password is hunter2
```
becomes
```
^^L1:c2FsdA==f8x9K3mN2pQ7rL4wYz1sT6vU9bC5dE8gH...^^
```

No plugin, no password → just a harmless-looking string. With the plugin and the right password → the original text, right where it was.

---

## Why

People leave sensitive notes in all kinds of text files — source comments, `.env`-style configs, personal notes, README drafts — internal API keys, business details, credentials for test environments. These end up in git history, get exposed during audits, and are visible to every contractor or new hire with repo access.

EncTags lets you keep the note in place, encrypted, so the file stays functional (or just readable) and the fragment stays hidden from anyone without the password.

## How it works

The plugin looks for `^^...^^` tags in your file.

| You see | It means |
|---|---|
| `^^some text^^` | Not yet encrypted — select it and press the hotkey |
| `^^L1:base64...^^` | Encrypted — put your cursor inside and press the hotkey |

One hotkey, **Ctrl+Shift+E**, toggles whichever tag your cursor is on (or your current text selection):

- Selected plain text → encrypted into a `^^L1:...^^` tag
- Cursor inside a raw `^^text^^` tag → encrypted in place
- Cursor inside an encrypted `^^L1:...^^` tag → decrypted, password prompt appears
- Cursor inside already-decrypted text (this session) → re-encrypted with the same password

The plugin does nothing automatically on file open, save, or close — every encryption/decryption is a deliberate action you trigger yourself.

## Install

1. Go to [Releases](../../releases) and download `EncTags.dll`
2. Copy it to `<Notepad++ install folder>\plugins\EncTags\EncTags.dll`
3. Restart Notepad++
4. `Plugins → EncTags`

## Cryptography

- **Key derivation:** PBKDF2-HMAC-SHA256, 100,000 iterations
- **Encryption:** AES-256-GCM (authenticated — tampering is detected, not just ignored)
- **Implementation:** Windows CNG (`bcrypt.dll`) — no third-party crypto library, no external dependencies
- Each encryption generates a fresh random salt and nonce — encrypting the same text twice with the same password produces different output

## Format

```
^^L1:<base64>^^
```

The base64 blob decodes to: `salt (16 bytes) + nonce (12 bytes) + ciphertext + auth tag (16 bytes)`.

The format is intentionally simple and openly specified, so it isn't locked to this plugin. Anything that implements PBKDF2-SHA256 + AES-256-GCM with the same parameters can decrypt an EncTags tag: a Python script, a browser extension, a CLI tool.

## Building from source

Requires Visual Studio 2022 with the "Desktop development with C++" workload.

```
git clone https://github.com/saintleningrad-prog/enctags-npp.git
cd enctags-npp
msbuild EncTags.sln /p:Configuration=Release /p:Platform=x64
```

Output: `x64\Release\EncTags.dll`

Or push to `main` — GitHub Actions builds it automatically (see [`.github/workflows/build.yml`](.github/workflows/build.yml)).

## Project structure

```
enctags-npp/
├── src/
│   ├── PluginMain.cpp          Plugin entry point, menu, hotkey logic
│   ├── EncTagsEngine.h/.cpp    PBKDF2 + AES-256-GCM (Windows BCrypt)
│   ├── TagParser.h/.cpp        Finds and classifies ^^...^^ tags
│   ├── FragmentRegistry.h/.cpp Tracks decrypted fragments in the buffer
│   ├── EncTags.rc              Password dialog resource
│   └── EncTags.def             DLL export table
├── include/
│   ├── PluginInterface.h       Notepad++ plugin API
│   └── Scintilla.h             Scintilla editor messages
├── EncTags.sln / .vcxproj
└── .github/workflows/build.yml
```

## Roadmap

- [ ] VSCode extension (same tag format, same crypto)
- [ ] Browser extension for reading tags in HTML source
- [ ] CLI tool for scripting / CI use
- [ ] Optional passphrase-strengthening service (OPRF-based, zero-knowledge)

## License

MIT — see [LICENSE](LICENSE)
