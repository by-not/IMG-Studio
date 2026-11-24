# Localization Guide — IMG Studio

IMG Studio loads translations dynamically at startup from UTF-8 JSON files under `assets/locales/`. Adding a new community language requires **zero C++ code changes**—just drop a validated `<lang>.json` file next to `en.json` and `es.json`.

---

## 1. Existing Locales & Template

- `assets/locales/en.json` — Master reference language (**English**, `429` keys).
- `assets/locales/es.json` — Complete reference translation (**Español**).
- `assets/locales/template.json` — Empty contributor template with all `429` keys ready to fill out (ignored automatically by IMG Studio at runtime).

---

## 2. Creating a New Translation

1. Copy `assets/locales/template.json` to `assets/locales/<code>.json` (use a lowercase 2-letter ISO 639-1 code such as `pt.json`, `fr.json`, `de.json`, `ru.json`, or `pl.json`).
2. Fill in `_meta`:

```json
{
  "_meta": {
    "code": "pt",
    "native_name": "Português",
    "english_name": "Portuguese",
    "version": "2.1.0",
    "authors": [
      "Your Name or GitHub Handle"
    ]
  },
  "strings": {
    ...
  }
}
```

- `native_name`: Shown in **Settings -> Language** (e.g., `Español`, `Português`, `Deutsch`).
- `english_name`: Used in documentation and diagnostics.
- `authors`: Optional list of translator credits.

---

## 3. Rules & Conventions

1. **Encoding**: Save as **UTF-8 without BOM** (`\xEF\xBB\xBF` is rejected by `scripts/validate_locales.py`). Use Unix LF (`\n`) line endings.
2. **Do Not Rename Keys**: Every JSON key inside `"strings"` must match `en.json` verbatim.
3. **Preserve Placeholders Exactly**:
   - Placeholders look like `{count}`, `{name}`, `{path}`, `{added}`, `{replaced}`, `{renamed}`, `{visible}`, `{total}`.
   - Keep the **exact English placeholder identifier** inside `{...}`. You may freely change their word order in your translation:
     - English: `"Showing {visible} / {total} entries"`
     - Example translation reordering parameters is supported: `"{total} entries total ({visible} shown)"`
   - Never drop or misspell a placeholder—the validator script and Catch2 tests enforce placeholder parity.
4. **Pluralization & Concise UI Fit**:
   - Where English uses compact labels in toolbars, table headers, and modal buttons, prefer concise phrasing so text fits cleanly inside menus and dialogs.
   - Until specialized plural forms are needed, follow the existing convention for numeric templates (e.g. `"{count} entry(s)"` / `"{count} entrada(s)"`).
5. **Keep Technical Identifiers Untranslated**:
   - File extensions and binary identifiers stay unchanged: `IMG`, `VER2`, `.img`, `.dff`, `.txd`, `.col`, `.ifp`, `.ipl`, `.ide`, `SHA-256`, `OpenGL 3.3`, `RenderWare`, `DXT1`, `DXT3`, `DXT5`, `RGBA`, `RGB`, `PNG`, `TGA`, `BMP`.
   - Default keyboard shortcut hints (such as `Ctrl+O`, `Ctrl+S`, `F5`) are passed separately by ImGui or appear in fixed hint lines; keep the actual key combinations unchanged.

---

## 4. Validating Your Translation

Always run the locale validation script from the repository root:

```bash
python3 scripts/validate_locales.py
```

The validator checks:
- Valid JSON syntax and **no UTF-8 BOM**
- **Zero duplicate keys**
- Required `_meta` metadata (`code`, `native_name`, `english_name`)
- **Zero missing or extra keys** compared to `en.json`
- **Zero empty translations**
- **100% `{placeholder}` match** against `en.json`
- Full consistency with all `app::tr` / `app::tr_c` / `app::trf` calls in `src/`

---

## 5. Testing Live in IMG Studio

1. Place your `<lang>.json` inside `<IMGStudio folder>/assets/locales/`.
2. Launch `IMGStudio.exe`:
   - Either select your language live in **File -> Settings -> Language** (the UI updates immediately on the next frame), or
   - Launch from CLI with `IMGStudio.exe --lang <code>`.
3. Inspect menus, modals (Rename, Delete, Rebuild & Save, Settings, About), Explorer headers, Inspector tabs, DFF Studio, TXD Studio, IMG Compare, and Batch Processing to confirm typography and button spacing look clean.
4. Submit a pull request or attach your file via the **Translation Contribution** issue template.
