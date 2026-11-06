# CLAUDE.md

A static recompilation of the N64 *Super Mario 64* (USA 1.0) with N64Recomp, N64ModernRuntime, RecompFrontend and
RT64, from the n64decomp/sm64 symbols. Almost all of it is written by AI agents, so this file is what the next one
needs: the rules, where things are, and the facts about the game the code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, generated (never edited by hand) by `tools/build_decomp.sh` then
  `tools/gen_syms.sh` (`tools/gen_syms.py` says how).
- `us.toml`: N64Recomp's configuration; `tools/recompile.sh` writes `RecompiledFuncs/` with it.
- `patches/` and `patches.toml`: MIPS C against `lib/sm64`'s headers, recompiled into PatchesLib. Its `ui_funcs.h`,
  `patch_helpers.h` and `recompui_event_structs.h` are RecompFrontend's, which includes the first from the game's
  `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the natives the patches call
  (`src/game/recomp_api.cpp`) and the settings.
- `lib/`: `sm64` (the decomp, for the symbols and the patches' headers), upstream `rt64`, `N64ModernRuntime` and
  `RecompFrontend` at the commits N64Recomp-browser's web forks are built on, and `N64Recomp-browser` at a release tag.
  The web support every recomp's page shares is developed and documented there; `tools/build_n64recomp.sh` and
  `CMakeLists.txt` apply its `patches/` to the other three, and `CMakeLists.txt` then this game's own changes to them
  (`runtime-patches/`, its README lists them). `xbrz` is xBRZ, vendored, for Hi-res Font.
- `docs/web.md`: the web page: building it (`tools/build_web.sh`), the survey a script drives it by
  (`patches/shader_survey.c`, `tools/shader_survey.js`), the play plans (`tools/play_plan*.json`), the prebuilt shader
  list (`src/rt64_shaders.txt`) and what the page measured. The page's part of N64Recomp-browser (its Games row, its
  part of `docs/game-forks.md`, `tools/publish_pages.sh`'s `sm64`) is there, and every play of the page is a line in
  N64Recomp-browser's `plays.jsonl` (`play_page.mjs --record plays.jsonl --game sm64`), never here.
- `docs/architecture.md`: how the game and its recompilation fit together, the facts the code can't say on its
  own (How it fits together, below).
- Comments beside the code, for how and why: each file's header. Read the header of the file you change and update
  it in the same commit.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks). Nothing from
  the decomp's `assets/` or its build is committed but `syms/`. The icon (`icons/icon.svg`, the launcher's thumbnail and
  the program's icon by `tools/icon.py`) is drawn for the project from plain shapes: no logo, character or picture from
  the game.
- The decomp is built only where the ROM is, never in CI.
- The page is built where the ROM is and published to this repo's Pages (`gh-pages`) by N64Recomp-browser's
  `tools/publish_pages.sh` (its `docs/publishing.md`), its `.data` holding nothing from the ROM. The desktop releases are built where the ROM is too, by
  `tools/release.sh` (its header says how), and published to GitHub Releases with their notes in `releases/`.
- The scope is the game as it plays, plus what a web page needs (RT64 interpolation to the display's rate, resolution
  Auto), plus the README's Modern enhancements: each an option with an Original value that plays as the console does,
  none changing how the game plays. No camera changes, cheats or 60 fps logic. USA 1.0 only, with one thing
  from the Japanese Shindou edition: its rumble, as an enhancement.
- Write for a stranger, in the present tense: nothing about a contributor's machine, servers, paths or accounts, and
  nothing about where anyone keeps ROMs.
- GPL-3.0, as the recompilations it follows.

## How it fits together

`docs/architecture.md` has the facts about the game the code can't say on its own: the code, no TLB, busy waits,
narrow arguments, stubs, the symbols, the entry point, microcodes, jump tables, saves, input, testing without the
speakers, driving it from a test, the game's own pacing, interpolation to the display's rate, Widescreen, Model detail,
Draw Distance, Input lag, Hi-res Font and Rumble. Read its part before changing the recompilation, a patch or a hook,
and add a new finding there, not here.
