# Hail the capybara - a game built to induce pure fun

capybaras are perfect. snakes are not. your ex turned into a snake, kept the grudge, and started yeeting capybaras at you, so yeah, that is why the model is tagged `your ex`.

The game is purely hardcoded in cpp without a game editor. i used pure cpp with raylib. one `main.cpp`, no drag and drop, no scene files, no unity/godot stuff. just code and a folder of assets.

# what you do

you run around a 72x72 grey grid arena with magenta walls and dark green cover. snakes slide around and shoot red capybara bullets at you. you shoot capybara bullets back and they bounce off walls and boxes. simple.

- your shots are the normal capybara png, enemy shots are the same png but red
- bullets bounce off walls, boxes, floor and ceiling, then die after 8 bounces
- your own bullet only hurts you after it bounced once. dont stand in the way of your own ricochet
- enemy bullets hurt right away
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

type a name. letters, numbers and `_`, 15 max. press enter.

- `1` bot mode, any name works
- `2` online mode, but not with `ghost`, `admin` or `nobody`, those names are taken
- then `1` chill / `2` normal / `3` mean
- backspace backs out

online is empty on purpose. no fake bots in there until real netcode exists.

# rounds and health

bot mode waves go 1, 2, 4, then 6 snakes forever. clear the wave and the next bunch loads after a bit.

you start with 100 hp and 12 rounds. reload takes about a second and also happens auto when you run dry. health pickups roll random every half sec to a sec with a 5% chance, max 3 at once. they are the `points.png` billboards. walking into one gives +30 hp, capped at 100, and shows `+30`.

# hud and minimap

top left: name, mode, difficulty, ammo, hp, round, dash bar.

top right minimap:

- white dot = you
- red dots = snakes
- light gray dots = health pickups
- gray blocks = cover
- white line = where you are looking

# build and run

from this folder:

```sh
g++ -O2 main.cpp -o boing-shoot -lraylib -lm -lpthread -ldl
./boing-shoot
```

the tracked `boing-shoot` binary can also just be run. stay in this folder so `assets/` loads right.

# browser / itch.io

there is also a real wasm web build:

```sh
bash web/build.sh
```

that makes `web/dist/index.html` and `boing-shoot-web.zip`. upload the zip to itch.io as an HTML project, 1280x720, with "played in the browser" on. full itch steps are in `web/README.md`.

made for haven hack club, delhi — https://hack.club/hv

# where the stuff came from

capybara bullet png came from pngimg, CC BY-NC 4.0, links are in `assets/CREDITS.txt`.

snake model and textures came from the `assets/snake fxb.zip` and `assets/texture snake.rar` you gave me. fbx got converted to obj because this raylib build wont load fbx. the jpg textures got converted to png because progressive jpgs broke. no ai art or music.

#HAVE FUN#
