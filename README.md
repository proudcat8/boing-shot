# Hail the capybara - a game built to induce pure fun

capybaras are perfect. this one runs around a grey arena and shoots its own face at walls. the bullets bounce. that is the whole game now.

pure cpp with raylib. one `main.cpp`, no game editor, no scene files, no unity/godot stuff. just code and a folder of assets.

# what you do

you run around a 72x72 grey grid arena with magenta walls and dark green cover. left click throws capybara bullets. they bounce off walls, boxes, floor and ceiling, then die after 8 bounces.

- every bullet is the same plain capybara png
- no enemies, no snakes, no multiplayer, no health, no score
- reload takes about a second and also happens auto when you run dry
- the capybara billboard is averaged from the little `assets/character/` pngs

# controls

- `WASD` = move
- mouse = look
- left click = shoot
- `space` = jump
- `R` = reload
- `shift + WASD` = dash
- `esc` = back to menu

dash needs a direction, shift alone does nothing. it has a 3 sec cooldown shown by the dash bar. crosshair is dead center because that is where the shot goes.

# starting up

type a name. letters, numbers and `_`, 15 max. press enter, then `1` or enter to play. backspace backs out.

the bgm starts on the name screen too. in a browser, click or press a key once first so the browser allows sound.

# hud and minimap

top left: name, ammo, dash bar.

top right minimap:

- white dot = you
- gray blocks = cover
- white line = where you are looking

# build and run

from this folder:

```sh
g++ -O2 -std=c++17 main.cpp -o hail-the-capybara -lraylib -lm -lpthread -ldl
./hail-the-capybara
```

the tracked `hail-the-capybara` binary can also just be run. stay in this folder so `assets/` loads right.

the github url still says `boing-shot` from the old name. that is just legacy plumbing, the game is **Hail the capybara**.

# browser / itch.io

there is also a real wasm web build:

```sh
bash web/build.sh
```

that makes `web/dist/index.html` and `hail-the-capybara-web.zip`. upload the zip to itch.io as an HTML project, 1280x720, with "played in the browser" on. full itch steps are in `web/README.md`.

both builds are solo only now. no lan, no relay, no extra process.

made for haven hack club, delhi — https://hack.club/hv

# where the stuff came from

capybara bullet png came from pngimg, CC BY-NC 4.0, links are in `assets/CREDITS.txt`. the bgm is the `assets/bgm.mp3` you added, kept quiet so shots stay on top.

the capybara sprite comes from base-color textures you gave me. they got averaged down into the tiny pngs in `assets/character/`, so the repo only carries those. no ai art.

#HAVE FUN#
