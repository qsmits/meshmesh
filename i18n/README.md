# Translations

MeshMesh speaks 14 languages: English (`en`, the source), Ukrainian (`uk`), Spanish (`es`), Portuguese (`pt`,
Brazilian), French (`fr`), German (`de`), Italian (`it`), Polish (`pl`), Turkish (`tr`), Chinese (`zh`,
Simplified), Japanese (`ja`), Korean (`ko`), Arabic (`ar`) and Indonesian (`id`).

| What | Source | Translations |
|---|---|---|
| Device screens (firmware) | English strings in `src/` | `i18n/firmware/<lang>.json` |
| Device web page | English text of `web/index.html` | `i18n/web/<lang>.json` |

## Firmware strings

English is written in the code; the other languages come from `i18n/firmware/<lang>.json`.

- `t("English")` — a screen string (in `src/Ui.cpp` and `src/UiHeltec.cpp`, a macro around `tr()` that returns a
  `String`).
- `tr("English")` — the translated `const char*`. The key is computed by the compiler (`include/I18n.h`).
- `N_("English")` — marks a string in a table; it is translated where it is read, with `trText(text)`, which
  hashes the text at run time.
- `count(n, "one", "many")` (or `plural(n, "one", "many")`) — the number and the noun form its language needs.
  English is given as the singular and plural noun; the number is put in front of it.

A string without a translation shows English. The language number is stored in NVS, so the order of the languages
in `include/I18n.h` (and `LANGS` in `tools/i18n.py`) never changes; new languages go to the end.

Each entry of `i18n/firmware/<lang>.json` carries the English source and the translation:

```json
"0dc6b4aa": {"en": "Off", "text": "Aus"}
```

The key is an FNV-1a hash (32 bit, as 8 hex digits) of the English text; for a plural, of the two English forms
joined by the byte 0x1f. Editing the English text in the code therefore makes a new entry and drops the old
translation. `i18n/firmware/source.json` tells where each string is used and its width: `cells` is the English
text in 6-pixel columns (a CJK character takes two).

Plural entries have `forms` with the CLDR categories of the language, and `{n}` where the number goes:

```json
"…": {"en": {"one": "hop", "other": "hops"}, "forms": {"one": "{n} Hop", "other": "{n} Hops"}}
```

Categories: `one`/`other` (es, pt, fr, de, it, tr); `one`/`few`/`many` (uk, pl); `other` only (zh, ja, ko, id);
`zero`/`one`/`two`/`few`/`many`/`other` (ar). Every form needs exactly one `{n}`, except that an Arabic form may
leave the number out (`"رسالتان"`).

Rules:

- **Fit the screen.** The M9 screen is 320×240; the Heltec V4's is 128×64, about 21 columns of the body font. Keep
  a translation close to `cells`; abbreviate when needed.
  `check` reports translations that are much longer than the English.
- **Keep the edges.** Leading and trailing spaces, line breaks and placeholders (`%d`, `%s`, `{weekday}`, `{month}`,
  `{day}`) stay exactly as in English: the code glues these strings together. `check` enforces this.
- **Keep the names.** MeshMesh, MeshCore, LoRa, Wi-Fi, Bluetooth, BLE, GPS, USB, SD, SNR, RSSI, dBm, ACK, UTC, PIN,
  NTP, CSI, ESP-NOW, OSM, ThinkNode M9, and the key names written on the keyboard (OK, DEL, Enter, Sym, `@` …) are
  not translated.
- **Scripts.** Chinese is Simplified; Japanese mixes kana and kanji as usual UIs do; Arabic is Modern Standard
  Arabic without harakat or tatweel, with Western digits (the screens draw Arabic right to left inside the usual
  left-to-right layout). Only Ukrainian may contain Cyrillic.
- **Tone.** Short, neutral interface language: infinitives or imperatives for actions, no exclamation marks.

### Glyph subsets

The screen fonts hold Latin-1 and Cyrillic. Other letters come from fallback fonts in `include/I18nFonts.h`: Latin
Extended from the X11 misc-fixed fonts, Chinese, Japanese and Korean from Fusion Pixel Font (SIL OFL 1.1, licenses
in `i18n/fonts/fusion-pixel/`), Arabic from ClearlyU 12 (`i18n/fonts/clearlyu-cu12.txt`). Only the glyphs the
translations use are kept, so `generate` must run after every translation change; `check` reports characters
without a glyph. Text received over the radio in these scripts shows only the glyphs that are in the subset.

`generate` needs the BDF fonts and the u8g2 `bdfconv` tool in `upstream/fonts` (not in Git; see
`tools/i18n_fonts.py`).

### Commands

```sh
python3 tools/i18n.py sync        # after changing strings in src/: new ones appear empty, removed ones go
python3 tools/i18n.py todo de     # what German still lacks, with the code line it comes from
python3 tools/i18n.py check       # complete, same edges and placeholders, fits, glyphs exist, table up to date
.venv/bin/python tools/i18n.py generate   # include/I18nTable.h and the glyph subsets in include/I18nFonts.h
```

Then build (`pio run -e m9`) and look at the screens: `sh tools/ui_preview/build.sh m9 OUTDIR de`.

## Device web page

The page's source text is English: `L('Text')` (also `` L`Text ${x}` ``) in the code and the text of the markup.
`i18n/web/<lang>.json` maps the English text to the translation; `{0}`, `{1}` stand for the inserted values.
Plurals are `plural(n,'message','messages')`, keyed `"message|messages"`, with the language's CLDR forms and
`{n}` where the number goes. Text that must stay as it is carries `translate="no"`. Ukrainian is the only
language that may contain Cyrillic. The page picks its language from `?lang=`, then the one chosen on the page,
then the browser's; English needs no dictionary.

```sh
npm ci --prefix tools          # once
node tools/web_i18n.cjs sync   # after changing page text: new keys appear empty, removed ones go
node tools/web_i18n.cjs check
```
