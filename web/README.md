# browser / itch.io build

Built by Haven Hack Club, Delhi — https://hack.club/hv

This folder makes a real WebAssembly build of the game. No game editor, still pure cpp with raylib, just compiled to wasm with Emscripten.

## Build it

From the repo root:

```sh
bash web/build.sh
```

It writes:

- `web/dist/index.html` plus the `.js`, `.wasm` and `.data` files
- `hail-the-capybara-web.zip` with `index.html` at the zip root, ready for itch.io

The zip only carries what the game uses: the capybara art, character pngs, sounds, music, and credits.

## Test it locally

Do not open `index.html` by double-clicking it. Browsers block wasm/data loading over `file://`. Serve the folder:

```sh
python3 -m http.server 8000 --directory web/dist
```

Then open http://localhost:8000.

Click the game or press a key once so the browser gives it mouse lock and audio. The BGM is quiet under the SFX and starts on the name screen as soon as the browser allows sound. Press `esc` to give the mouse back.

## Game modes

Solo only. No enemies spawn and there is no LAN mode. You just shoot bullets and watch them bounce.

## Upload to itch.io

1. Make a project on https://itch.io/game/new.
2. Title: `Hail the capybara`.
3. Kind of project: **HTML**.
4. Upload `hail-the-capybara-web.zip` from the repo root.
5. In the upload options, enable **This file will be played in the browser**.
6. Set viewport size to **1280 x 720**. Turn on fullscreen button if you want it.
7. Publish, then copy the embed URL from the project's **Embed** widget if you want to put it on the Hack Club page.

If you are linking it from `hack.club/hv`, point that link at the itch game page, or paste the itch embed snippet into the site.
