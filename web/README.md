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

The build only preloads files the game actually uses at runtime. The FBX source, JPG originals, and zip/rar archives stay out of the web download.

## Test it locally

Do not open `index.html` by double-clicking it. Browsers block wasm/data loading over `file://`. Serve the folder:

```sh
python3 -m http.server 8000 --directory web/dist
```

Then open http://localhost:8000.

Click the game once so the browser gives it mouse lock and audio. Press `esc` to give the mouse back.

## Online rooms

Start the relay on one machine:

```sh
python3 server.py
```

It listens on `8765` and speaks both raw tcp (native builds) and websocket text frames (this web build). In game pick `2` online mode, type the host, press enter. `localhost:8765` is the default and works when the page is served from local http on the same pc.

Plain hosts like `example.com:8765` get `ws://` added on http pages and `wss://` on https pages. You can also type the full `ws://` or `wss://` url. itch.io pages are https, so internet play needs a public `wss://` relay; a plain `ws://` relay will be blocked by the browser.

## Upload to itch.io

1. Make a project on https://itch.io/game/new.
2. Title: `Hail the capybara`.
3. Kind of project: **HTML**.
4. Upload `hail-the-capybara-web.zip` from the repo root.
5. In the upload options, enable **This file will be played in the browser**.
6. Set viewport size to **1280 x 720**. Turn on fullscreen button if you want it.
7. Publish, then copy the embed URL from the project's **Embed** widget if you want to put it on the Hack Club page.

If you are linking it from `hack.club/hv`, point that link at the itch game page, or paste the itch embed snippet into the site.
