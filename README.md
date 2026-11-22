# Hail the capybara - a game built to induce pure fun

capybaras are perfect. snakes are not. your ex turned into a snake, kept the grudge, and started yeeting capybaras at you, so yeah, that is why the model is tagged `your ex`.

The game is purely hardcoded in cpp without a game editor. i used pure cpp with raylib. one `main.cpp`, no drag and drop, no scene files, no unity/godot stuff. just code and a folder of assets.

# what you do

you run around a 72x72 grey grid arena with magenta walls and dark green cover. snakes slide around and shoot red capybara bullets at you. you shoot capybara bullets back and they bounce off walls and boxes. simple.

- your shots are the normal capybara png. bot snakes use the same png tinted red
- online everyones bullets are the same plain capybara png, no red team bullets
- bullets bounce off walls, boxes, floor and ceiling, then die after 8 bounces
- your own bullet only hurts you after it bounced once. dont stand in the way of your own ricochet
- enemy and online bullets hurt right away
- getting hit is -10 hp and a red insult. killing a snake is white capybara praise plus the kill fx and sound. no score. capybaras dont need a scoreboard

# controls

- `WASD` = move
- mouse = look
- left click = shoot
- `space` = jump
- `R` = reload, also respawn if dead
- `shift + WASD` = dash
- `esc` = back to menu
- `enter` = continue from name screen, respawn if dead

dash needs a direction, shift alone does nothing. it goes where you were holding wasd, lasts a blink, and has a 3 sec cooldown shown by the dash bar in the hud. crosshair is dead center because that is where the shot goes.

# starting up

type a name. letters, numbers and `_`, 15 max. press enter. the bgm starts on this screen too; in a browser, click or press a key once first so the browser allows sound.

- `1` bot mode, any name works
- `2` lan mode, but not with `ghost`, `admin` or `nobody`, those names are taken
- bot mode then `1` chill / `2` normal / `3` mean
- lan mode then just press enter
- first player becomes host. friends on the same wifi press enter and join automatically
- room code shows in the hud. nobody types an ip or a code
- backspace backs out

lan is local only, native build only. no extra process, no cloud, no setup. the first game starts its own relay thread; everyone else finds it on the lan. when the host leaves, the room goes away.

everyone in the room sees the same capybara bullets and can bounce them into each other. hits are -10 hp, just like bot mode. kill praise and snakes only exist in bot mode.

# rounds and health

bot mode waves go 1, 2, 4, then 6 snakes forever. clear the wave and the next bunch loads after a bit.

you start with 100 hp and 12 rounds. reload takes about a second and also happens auto when you run dry. health pickups roll random every half sec to a sec with a 5% chance, max 3 at once. they are the `points.png` billboards. walking into one gives +30 hp, capped at 100, and shows `+30`.

# hud and minimap

top left: name, mode, difficulty, ammo, hp, round, dash bar.

top right minimap:

- white dot = you
- red dots = snakes
- pink dots = other capybaras on lan
- light gray dots = health pickups
- gray blocks = cover
- white line = where you are looking

# build and run

from this folder:

```sh
g++ -O2 -std=c++17 main.cpp -o hail-the-capybara -lraylib -lm -lpthread -ldl
./hail-the-capybara
```

the tracked `hail-the-capybara` binary can also just be run. stay in this folder so `assets/` loads right.

lan needs no extra process. the relay lives inside the native game and only listens while you are in a lan room.

the github url still says `boing-shot` from the old name. that is just legacy plumbing, the game is **Hail the capybara**.

# browser / itch.io

there is also a real wasm web build:

```sh
bash web/build.sh
```

that makes `web/dist/index.html` and `hail-the-capybara-web.zip`. upload the zip to itch.io as an HTML project, 1280x720, with "played in the browser" on. full itch steps are in `web/README.md`.

the web build is solo only. lan play needs the native build because browsers cannot host a relay.

made for haven hack club, delhi — https://hack.club/hv

# where the stuff came from

capybara bullet png came from pngimg, CC BY-NC 4.0, links are in `assets/CREDITS.txt`. the bgm is the `assets/bgm.mp3` you added, kept quiet so shots and hits stay on top.

snake model and textures came from the original fbx/rar files you gave me. fbx got converted to obj because this raylib build wont load fbx. the jpg textures got converted to png because progressive jpgs broke.

the other capybara sprite comes from base-color textures you gave me. they got averaged down into the tiny pngs in `assets/character/`, so the repo only carries those. no ai art.

#HAVE FUN#
