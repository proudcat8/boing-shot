#include "raylib.h"
#include "raymath.h"
#include <cctype>
#include <cstring>

const int W = 1280, H = 720;
const float A = 12.0f, WH = 6.0f, EH = 1.65f;

enum Screen { S_NAME, S_MENU, S_GAME };
Screen screen = S_NAME;
char pname[16] = "";
int mode = 0;
bool nameTaken = false;
float t = 0, shake = 0;

struct Player { Vector3 pos; float yaw, pitch, vy, fireT, reloadT, invuln; bool grounded; int ammo, hp; };
struct Bullet { Vector3 pos, vel; float life; int bounces; bool alive, mine; };
struct Spark  { Vector3 pos, vel; float life; Color c; };

Player me;
Bullet bullets[100];
Spark sparks[60];
Camera3D cam;
Sound sHit, sKill, sJump, sShoot, sReload;
float hitCd = 0;

BoundingBox map[] = {
    {{-9,0,-2},{-2.5,3,-1}},  {{2.5,0,-2},{9,3,-1}},
    {{-9,0,2},{-2.5,3,3}},    {{2.5,0,2},{9,3,3}},
    {{-0.9,0,-6},{0.9,2,-4.2}},
    {{-7,0,7},{-5,1.2,9}},    {{5,0,-9},{7,1.2,-7}}
};
const int NMAP = sizeof(map) / sizeof(map[0]);

float randf(float a, float b) { return a + (b - a) * GetRandomValue(0, 1000) / 1000.0f; }

// void spawnBot(Vector3 at) {
//     bots[numBots].pos = at;
//     bots[numBots].hp = 40;
//     numBots++;
// }

bool same(const char *a, const char *b) {
    while (*a && *b) if (tolower(*a++) != tolower(*b++)) return false;
    return !*a && !*b;
}

bool nameUnavailable(const char *n) {
    const char *busy[] = {"ghost", "admin", "nobody"};
    for (const char *b : busy) if (same(n, b)) return true;
    return false;
}

void reset() {
    me = {{0, 0, 9}, 0, 0, 0, 0, 0, 0, true, 12, 100};
    for (Bullet &b : bullets) b.alive = false;
    for (Spark &s : sparks) s.life = 0;
    cam = {{0, EH, 9}, {0, EH, 8}, {0, 1, 0}, 80, CAMERA_PERSPECTIVE};
}

void emit(Vector3 p, Color c, float sp, float life) {
    for (Spark &s : sparks) if (s.life <= 0) {
        s = {p, {randf(-sp, sp), randf(0, sp * 1.4f), randf(-sp, sp)}, life, c};
        return;
    }
}

// particle for a kill. works when i call it, no dying
void burst(Vector3 p) {
    for (int i = 0; i < 24; i++)
        emit(p, i % 3 == 0 ? RAYWHITE : (i % 3 == 1 ? LIGHTGRAY : GRAY), 5, randf(0.4f, 1.0f));
    shake = 0.35f;
}

bool bounce(Bullet &b) {
    float r = 0.12f;
    if (b.pos.x < -A + r)  { b.pos.x = -A + r; b.vel.x = fabsf(b.vel.x); return true; }
    if (b.pos.x >  A - r)  { b.pos.x =  A - r; b.vel.x = -fabsf(b.vel.x); return true; }
    if (b.pos.z < -A + r)  { b.pos.z = -A + r; b.vel.z = fabsf(b.vel.z); return true; }
    if (b.pos.z >  A - r)  { b.pos.z =  A - r; b.vel.z = -fabsf(b.vel.z); return true; }
    if (b.pos.y < r)       { b.pos.y = r;      b.vel.y = fabsf(b.vel.y); return true; }
    if (b.pos.y > WH - r)  { b.pos.y = WH - r; b.vel.y = -fabsf(b.vel.y); return true; }
    for (const BoundingBox &box : map) {
        if (b.pos.x < box.min.x - r || b.pos.x > box.max.x + r ||
            b.pos.y < box.min.y - r || b.pos.y > box.max.y + r ||
            b.pos.z < box.min.z - r || b.pos.z > box.max.z + r) continue;
        float d[6] = {b.pos.x - (box.min.x - r), (box.max.x + r) - b.pos.x,
                      b.pos.y - (box.min.y - r), (box.max.y + r) - b.pos.y,
                      b.pos.z - (box.min.z - r), (box.max.z + r) - b.pos.z};
        int k = 0;
        for (int i = 1; i < 6; i++) if (d[i] < d[k]) k = i;
        if (k == 0) { b.pos.x = box.min.x - r; b.vel.x = -fabsf(b.vel.x); }
        if (k == 1) { b.pos.x = box.max.x + r; b.vel.x =  fabsf(b.vel.x); }
        if (k == 2) { b.pos.y = box.min.y - r; b.vel.y = -fabsf(b.vel.y); }
        if (k == 3) { b.pos.y = box.max.y + r; b.vel.y =  fabsf(b.vel.y); }
        if (k == 4) { b.pos.z = box.min.z - r; b.vel.z = -fabsf(b.vel.z); }
        if (k == 5) { b.pos.z = box.max.z + r; b.vel.z =  fabsf(b.vel.z); }
        return true;
    }
    return false;
}

void update(float dt) {
    Vector2 m = GetMouseDelta();
    me.yaw += m.x * 0.003f;
    me.pitch = Clamp(me.pitch - m.y * 0.003f, -1.3f, 1.3f);
    Vector3 f = {sinf(me.yaw), 0, -cosf(me.yaw)}, r = {cosf(me.yaw), 0, sinf(me.yaw)};
    Vector3 move = {0,0,0};
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, f);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, f);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, r);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, r);
    if (Vector3LengthSqr(move) > 0) {
        move = Vector3Normalize(move);
        float sp = IsKeyDown(KEY_LEFT_SHIFT) ? 14 : 9;
        me.pos.x = Clamp(me.pos.x + move.x * sp * dt, -A + 0.5f, A - 0.5f);
        me.pos.z = Clamp(me.pos.z + move.z * sp * dt, -A + 0.5f, A - 0.5f);
    }
    for (const BoundingBox &b : map) if (me.pos.y < b.max.y - 0.1f &&
        me.pos.x > b.min.x - 0.4f && me.pos.x < b.max.x + 0.4f &&
        me.pos.z > b.min.z - 0.4f && me.pos.z < b.max.z + 0.4f) {
        float d[4] = {me.pos.x - (b.min.x - 0.4f), (b.max.x + 0.4f) - me.pos.x,
                      me.pos.z - (b.min.z - 0.4f), (b.max.z + 0.4f) - me.pos.z};
        int k = 0;
        for (int i = 1; i < 4; i++) if (d[i] < d[k]) k = i;
        if (k == 0) me.pos.x = b.min.x - 0.4f;
        if (k == 1) me.pos.x = b.max.x + 0.4f;
        if (k == 2) me.pos.z = b.min.z - 0.4f;
        if (k == 3) me.pos.z = b.max.z + 0.4f;
    }
    if (IsKeyPressed(KEY_SPACE) && me.grounded) {
        me.vy = 7; me.grounded = false;
        SetSoundVolume(sJump, 0.25f); PlaySound(sJump);
    }
    float oldY = me.pos.y;
    me.vy -= 18 * dt;
    me.pos.y += me.vy * dt;
    if (me.pos.y <= 0) { me.pos.y = 0; me.vy = 0; me.grounded = true; }
    for (const BoundingBox &b : map)
        if (me.vy <= 0 && oldY >= b.max.y && me.pos.y <= b.max.y &&
            me.pos.x > b.min.x - 0.4f && me.pos.x < b.max.x + 0.4f &&
            me.pos.z > b.min.z - 0.4f && me.pos.z < b.max.z + 0.4f) {
            me.pos.y = b.max.y; me.vy = 0; me.grounded = true;
        }

    // ammo/reload
    me.fireT -= dt;
    me.invuln -= dt;
    if (me.reloadT > 0 && (me.reloadT -= dt) <= 0) me.ammo = 12;
    if (IsKeyPressed(KEY_R) || me.ammo == 0) if (me.reloadT <= 0 && me.ammo < 12) {
        me.reloadT = 0.9f;
        SetSoundVolume(sReload, 0.7f); PlaySound(sReload);
    }
    if (IsKeyPressed(KEY_K)) {          // temp key
        burst(me.pos);
        SetSoundVolume(sKill, 1.0f); PlaySound(sKill);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && me.fireT <= 0 && me.reloadT <= 0 && me.ammo > 0) {
        for (Bullet &b : bullets) if (!b.alive) {
            Vector3 d = {sinf(me.yaw)*cosf(me.pitch), sinf(me.pitch), -cosf(me.yaw)*cosf(me.pitch)};
            b = {Vector3Add((Vector3){me.pos.x, me.pos.y + EH, me.pos.z}, Vector3Scale(d, 0.5f)),
                 Vector3Scale(d, 55), 5, 0, true, true};
            me.ammo--; me.fireT = 0.14f;
            shake = 0.08f;
            SetSoundVolume(sShoot, 0.6f); PlaySound(sShoot);
            break;
        }
    }
    hitCd -= dt;
    // no enemy bullet
    for (Bullet &b : bullets) {
        if (!b.alive) continue;
        if ((b.life -= dt) <= 0) { b.alive = false; continue; }
        for (int s = 0; s < 4 && b.alive; s++) {
            b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt / 4));
            if (bounce(b)) {
                if (++b.bounces > 8) b.alive = false;
                if (hitCd <= 0) {
                    hitCd = 0.05f;
                    emit(b.pos, LIGHTGRAY, 2, 0.25f);
                    shake = fmaxf(shake, 0.03f);
                    SetSoundVolume(sHit, 0.8f); PlaySound(sHit);
                }
            }
            //own shots damage after they bounce once
            Vector3 body = {me.pos.x, me.pos.y + 1, me.pos.z};
            if (me.invuln <= 0 && Vector3DistanceSqr(body, b.pos) < 0.8f * 0.8f &&
                (!b.mine || b.bounces > 0)) {
                b.alive = false;
                me.hp -= 10; me.invuln = 0.4f;
                shake = 0.25f;
            }
        }
    }
    if (me.hp <= 0) reset();
    for (Spark &s : sparks) {
        if (s.life <= 0) continue;
        s.life -= dt;
        s.vel.y -= 12 * dt;
        s.pos = Vector3Add(s.pos, Vector3Scale(s.vel, dt));
    }
    shake = fmaxf(0, shake - dt);
    Vector3 eye = {me.pos.x, me.pos.y + EH, me.pos.z};
    cam.position = eye;
    cam.target = Vector3Add(eye, (Vector3){sinf(me.yaw)*cosf(me.pitch), sinf(me.pitch), -cosf(me.yaw)*cosf(me.pitch)});
    if (shake > 0) cam.position = Vector3Add(cam.position, (Vector3){randf(-shake,shake), randf(-shake,shake), 0});
}

void drawWorld() {
    DrawPlane((Vector3){0,0,0}, (Vector2){A*2, A*2}, DARKGRAY);
    Color grid = Fade(LIGHTGRAY, 0.35f + 0.15f * sinf(t * 4));
    for (float i = -A; i <= A; i += 2) {
        DrawLine3D((Vector3){i, 0.02f, -A}, (Vector3){i, 0.02f, A}, grid);
        DrawLine3D((Vector3){-A, 0.02f, i}, (Vector3){A, 0.02f, i}, grid);
    }
    Color wall = DARKGRAY;
    DrawCube((Vector3){0, WH/2, -A-0.5f}, A*2+1, WH, 1, wall);
    DrawCube((Vector3){0, WH/2,  A+0.5f}, A*2+1, WH, 1, wall);
    DrawCube((Vector3){-A-0.5f, WH/2, 0}, 1, WH, A*2+1, wall);
    DrawCube((Vector3){ A+0.5f, WH/2, 0}, 1, WH, A*2+1, wall);
    for (int i = 0; i < NMAP; i++) {
        BoundingBox b = map[i];
        Vector3 c = {(b.min.x+b.max.x)/2, (b.min.y+b.max.y)/2, (b.min.z+b.max.z)/2};
        Vector3 s = {b.max.x-b.min.x, b.max.y-b.min.y, b.max.z-b.min.z};
        DrawCubeV(c, s, GRAY);
        DrawCubeWiresV(c, s, LIGHTGRAY);
    }
    for (const Bullet &b : bullets) {
        if (!b.alive) continue;
        Color c = b.mine ? RAYWHITE : RED;
        DrawSphere(b.pos, 0.28f, Fade(c, 0.18f));
        DrawSphere(b.pos, 0.11f, c);
        DrawLine3D(Vector3Subtract(b.pos, Vector3Scale(Vector3Normalize(b.vel), 0.8f)), b.pos, c);
    }
    for (const Spark &s : sparks)
        if (s.life > 0) DrawCube(s.pos, 0.13f, 0.13f, 0.13f, Fade(s.c, Clamp(s.life, 0, 1)));
}

// crosshair
void drawCrosshair() {
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    DrawLine(cx - 14, cy, cx - 5, cy, RAYWHITE);
    DrawLine(cx + 5, cy, cx + 14, cy, RAYWHITE);
    DrawLine(cx, cy - 14, cx, cy - 5, RAYWHITE);
    DrawLine(cx, cy + 5, cx, cy + 14, RAYWHITE);
}

// minimap fak plyr tst
void drawMinimap() {
    int mx = W - 186, my = 20, ms = 160;
    DrawRectangle(mx - 6, my - 6, ms + 12, ms + 12, Fade(BLACK, 0.55f));
    DrawRectangleLines(mx, my, ms, ms, LIGHTGRAY);
    for (const BoundingBox &b : map)
        DrawRectangle(mx + (b.min.x + A) / (2*A) * ms, my + (b.min.z + A) / (2*A) * ms,
                      (b.max.x - b.min.x) / (2*A) * ms, (b.max.z - b.min.z) / (2*A) * ms,
                      Fade(GRAY, 0.9f));
    auto dot = [&](float x, float z, Color c, int r) {
        DrawCircle(mx + (x + A) / (2*A) * ms, my + (z + A) / (2*A) * ms, r, c);
    };
    for (int i = 0; i < 3; i++)
        dot(sinf(t * 0.6f + i * 2.1f) * 7, cosf(t * 0.5f + i * 2.1f) * 7, mode ? LIGHTGRAY : GRAY, 3);
    dot(me.pos.x, me.pos.z, RAYWHITE, 4);
    DrawLine(mx + (me.pos.x + A) / (2*A) * ms, my + (me.pos.z + A) / (2*A) * ms,
             mx + (me.pos.x + sinf(me.yaw) * 2 + A) / (2*A) * ms,
             my + (me.pos.z - cosf(me.yaw) * 2 + A) / (2*A) * ms, RAYWHITE);
    DrawText(mode ? "players" : "bots", mx, my + ms + 8, 14, LIGHTGRAY);
}

void drawMenu() {
    ClearBackground(DARKGRAY);
    DrawText("BOING SHOOT", 80, 90, 64, RAYWHITE);
    DrawText("bounce back - bullets go brrr (wip)", 84, 165, 20, LIGHTGRAY);
    if (screen == S_NAME) {
        DrawText(TextFormat("name: %s_", pname), 84, 250, 32, RAYWHITE);
        DrawText("letters/numbers only - enter to continue", 84, 300, 20, LIGHTGRAY);
    } else {
        DrawText(TextFormat("name: %s", pname), 84, 250, 28, RAYWHITE);
        DrawText("1  bot mode   (any name)", 84, 320, 26, RAYWHITE);
        DrawText("2  online mode (name is reserved)", 84, 360, 26, RAYWHITE);
        if (nameTaken) DrawText("NAME UNAVAILABLE - pick another name", 84, 415, 22, RED);
        DrawText("backspace - change name", 84, 465, 20, LIGHTGRAY);
        DrawText("WASD=move\n Left click= shoot\n R = reload", 84, 505, 18, LIGHTGRAY);
    }
}

int main() {
    InitWindow(W, H, "BOING SHOOT");
    //bot adn multiplauer not done
    DisableCursor();
    InitAudioDevice();
    sHit = LoadSound("assets/hit.mp3");
    sKill = LoadSound("assets/kill.mp3");
    sJump = LoadSound("assets/jump.mp3");
    sShoot = LoadSound("assets/shoot.mp3");
    sReload = LoadSound("assets/reload.mp3");
    SetTargetFPS(120);
    SetExitKey(KEY_NULL);
    reset();

    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.05f);
        t += dt;

        if (screen == S_NAME) {
            for (int c = GetCharPressed(); c; c = GetCharPressed())
                if ((isalnum(c) || c == '_') && strlen(pname) < 15) {
                    size_t n = strlen(pname);
                    pname[n] = (char)c;
                    pname[n+1] = 0;
                }
            if (IsKeyPressed(KEY_BACKSPACE) && strlen(pname)) pname[strlen(pname)-1] = 0;
            if (IsKeyPressed(KEY_ENTER) && strlen(pname)) {
                nameTaken = nameUnavailable(pname);
                screen = S_MENU;
            }
        } else if (screen == S_MENU) {
            if (IsKeyPressed(KEY_ONE)) { mode = 0; reset(); screen = S_GAME; }
            if (IsKeyPressed(KEY_TWO) && !nameTaken) { mode = 1; reset(); screen = S_GAME; }
            if (IsKeyPressed(KEY_BACKSPACE)) screen = S_NAME;
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) screen = S_MENU;
            update(dt);
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);
        if (screen == S_GAME) {
            BeginMode3D(cam);
            drawWorld();
            EndMode3D();
            drawCrosshair();
            DrawText(TextFormat("%s  |  %s  |  ammo %d  |  K = kill fx preview", pname,
                                mode ? "ONLINE" : "BOTS", me.ammo), 20, 20, 22, RAYWHITE);
            // bots + netcode need
            DrawText(TextFormat("HP %d", me.hp), 20, 50, 20, LIGHTGRAY);
            drawMinimap();
        } else drawMenu();
        EndDrawing();
    }
    UnloadSound(sHit); UnloadSound(sKill); UnloadSound(sJump);
    UnloadSound(sShoot); UnloadSound(sReload);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
