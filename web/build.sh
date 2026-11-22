#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="${WEB_BUILD_CACHE:-$HOME/.local/share/opencode/hail-cap-web}"
RAYLIB_TAG="${RAYLIB_TAG:-6.2}"
RAYLIB_SRC="${RAYLIB_SRC:-$CACHE/raylib-$RAYLIB_TAG}"
RAYLIB_BUILD="${RAYLIB_BUILD:-$CACHE/raylib-$RAYLIB_TAG-build}"
STAGE="$ROOT/web/stage"
DIST="$ROOT/web/dist"
OUT_ZIP="$ROOT/hail-the-capybara-web.zip"

if ! command -v emcc >/dev/null 2>&1; then
  EMSDK="${EMSDK:-$HOME/.local/share/opencode/emsdk}"
  if [[ -f "$EMSDK/emsdk_env.sh" ]]; then
    source "$EMSDK/emsdk_env.sh" >/dev/null
  fi
fi

if ! command -v emcc >/dev/null 2>&1 || ! command -v emcmake >/dev/null 2>&1; then
  echo "emcc/emcmake not found. install emsdk then set EMSDK to its folder:"
  echo "  git clone https://github.com/emscripten-core/emsdk.git"
  echo "  ./emsdk/emsdk install latest && ./emsdk/emsdk activate latest"
  echo "  EMSDK=/path/to/emsdk bash web/build.sh"
  exit 1
fi

mkdir -p "$CACHE"
if [[ ! -f "$RAYLIB_SRC/CMakeLists.txt" ]]; then
  git clone --depth 1 --branch "$RAYLIB_TAG" https://github.com/raysan5/raylib.git "$RAYLIB_SRC"
fi

if [[ ! -f "$RAYLIB_BUILD/.built" ]]; then
  rm -rf "$RAYLIB_BUILD"
  emcmake cmake -S "$RAYLIB_SRC" -B "$RAYLIB_BUILD" \
    -DPLATFORM=Web -DBUILD_EXAMPLES=OFF -DUSE_AUDIO=ON \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build "$RAYLIB_BUILD" --parallel 4
  touch "$RAYLIB_BUILD/.built"
fi

RAYLIB_LIB="$(find "$RAYLIB_BUILD" -name 'libraylib.a' -print -quit)"
if [[ -z "$RAYLIB_LIB" ]]; then
  echo "could not find libraylib.a in $RAYLIB_BUILD"
  exit 1
fi

rm -rf "$STAGE" "$DIST"
mkdir -p "$STAGE/assets/snake/textures" "$STAGE/assets/character" "$DIST"

FILES=(
  "assets/CREDITS.txt"
  "assets/capybara_bullet.png"
  "assets/points.png"
  "assets/character/body.png"
  "assets/character/jacket.png"
  "assets/character/jeans.png"
  "assets/character/shoes.png"
  "assets/character/gloves.png"
  "assets/character/backpack.png"
  "assets/hit.mp3"
  "assets/kill.mp3"
  "assets/jump.mp3"
  "assets/shoot.mp3"
  "assets/reload.mp3"
  "assets/dash.mp3"
  "assets/bgm.mp3"
  "assets/snake/Snake.obj"
  "assets/snake/Snake.mtl"
  "assets/snake/textures/S-Skin_03.png"
  "assets/snake/textures/S-Skin_NMap.png"
)
for f in "${FILES[@]}"; do
  cp "$ROOT/$f" "$STAGE/$f"
done

em++ -O2 -std=c++17 -DPLATFORM_WEB \
  -I"$RAYLIB_SRC/src" \
  "$ROOT/main.cpp" "$RAYLIB_LIB" -lwebsocket.js \
  -s USE_GLFW=3 \
  -s ASYNCIFY=1 \
  -s ASYNCIFY_STACK_SIZE=65536 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=134217728 \
  -s STACK_SIZE=1048576 \
  -s ENVIRONMENT=web \
  --preload-file "$STAGE/assets@assets/" \
  --shell-file "$ROOT/web/shell.html" \
  -o "$DIST/index.html"

python3 - "$DIST" "$OUT_ZIP" <<'PY'
import os
import sys
import zipfile

src, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for name in sorted(os.listdir(src)):
        path = os.path.join(src, name)
        if os.path.isfile(path):
            z.write(path, name)
PY

echo "web build: $DIST/index.html"
echo "itch zip:  $OUT_ZIP"
