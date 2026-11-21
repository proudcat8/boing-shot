# Hail the capybara - a game built to induce pure fun

Capybaras are perfect. Snakes are not. Your ex is a snake.

This is a first-person bounce-shooter where you, a loyal capybara servant, defend capybara supremacy against the natural enemy of capybara: **Snake**. The snakes are tagged **"your ex"** because they know what they did.

The game is purely hardcoded in cpp without a game editor. No Unity, no Unreal, no Godot, no scene files, no drag-and-drop. Pure cpp with raylib, one `main.cpp`, and a pile of assets sitting in `assets/`.

# What happens in the game

You spawn in a 72x72 grey grid arena with magenta walls and dark green cover boxes. Snakes slither around, shoot red capybara bullets at you, and generally act like your ex. You shoot back with capybara bullets that bounce off walls and boxes.

- Your bullets are natural-colored capybara billboards with sandy trails.
- Enemy bullets are the same capybara texture, tinted red, with red trails.
- Bullets bounce off the magenta walls, the green boxes, the floor, and the ceiling.
- A bullet dies after 8 bounces.
- Your own bullet can hurt you **only after it bounces once**. Straight shots are safe. Ricochets are capybara karma.
- Enemy bullets hurt you on contact.
- Getting hit costs 10 HP and earns a red insult like `capybara bad`.
- Killing a snake gives white praise like `capybaras are perfect`, plus the kill burst and sound. There is no score. The capybara does not need numbers.

# Controls

- `WASD` - move
- `Left click` - shoot
- `Space` - jump
- `R` - reload, or respawn after death
- `Shift + WASD` - dash
- `Mouse` - look and aim
- `Esc` - back to menu from the game
- `Enter` - continue from name entry, respawn after death

The crosshair is locked to the center of the screen: `GetScreenWidth() / 2`, `GetScreenHeight() / 2`.

Dash only works when you are holding a movement key. `Shift` by itself does nothing. The dash goes in the direction you were moving when you pressed it, lasts a short moment, and has a 3 second cooldown shown by the `dash` bar in the HUD.

# Menu and modes

1. Type a name using letters, numbers, and `_`. Max 15 characters.
2. Press `Enter`.
3. Pick a mode:
   - `1` - bot mode, works with any name
   - `2` - online mode, only when your name is not reserved
4. In bot mode, pick difficulty:
   - `1` - chill: slow haters, soft bullets
   - `2` - normal: the usual
   - `3` - mean: fast angry haters
5. Backspace goes back a menu step.

Reserved names are `ghost`, `admin`, and `nobody`. Do not try to be special.

Bot mode is the real hunt. Online mode is a quiet waiting room until real netcode exists; no bots or enemies are spawned there to fake it.

# Rounds and healing

Bot mode runs in rounds. Clear every snake in the round and the next group loads after a short pause.

- Round 1: 1 snake
- Round 2: 2 snakes
- Round 3: 4 snakes
- Round 4 and after: 6 snakes, forever

You start with 100 HP and a 12 round magazine. Reloading takes about a second and also happens automatically when you run dry.

Pickups show up randomly around the map, up to 3 at a time. They are the `points.png` billboards on the minimap and in the arena. Walking into one heals **+30 HP**, capped at 100, and shows `+30`.

# HUD and minimap

Top left shows your name, mode, difficulty, ammo, HP, round, and dash cooldown. Top right shows the minimap:

- white dot: you
- red dots: your ex
- light gray dots: health pickups
- gray blocks: cover
- white line: where you are looking

# Build and run

This is a native raylib build from the repository root.

```sh
g++ -O2 main.cpp -o boing-shoot -lraylib -lm -lpthread -ldl
./boing-shoot
```

The tracked `boing-shoot` binary can also be run directly if it already exists. Run from this folder so the relative `assets/` paths load correctly.

# Assets and credits

- `assets/capybara_bullet.png` was downloaded from pngimg.com, license CC BY-NC 4.0. Source and direct link are in `assets/CREDITS.txt`.
- The snake model and textures came from the provided `assets/snake fxb.zip` and `assets/texture snake.rar`. The FBX was converted to `assets/snake/Snake.obj` because this raylib build does not load FBX. The progressive JPG textures were converted to PNG copies for the same reason.
- No generated art or music. Only downloaded or provided assets, with format conversions where needed.

#HAVE FUN#
