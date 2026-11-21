// Hail the capybara - your ex shoots red bullets, you shoot it down lol
#include "raylib.h"
#include "raymath.h"
#include <cctype>
#include <cstring>

const int W = 1280, H = 720;
const float A = 36.0f, WH = 6.0f, EH = 1.65f;

enum Screen { S_NAME, S_MENU, S_DIFF, S_GAME };
Screen screen = S_NAME;
char pname[16] = "";
int mode = 0, difficulty = 1;
bool nameTaken = false;
float t = 0, shake = 0;

struct Player { Vector3 pos; float yaw, pitch, vy, fireT, reloadT, invuln; bool grounded; int ammo, hp; };
struct Bullet { Vector3 pos, vel; float life; int bounces; bool alive, mine; };
struct Spark  { Vector3 pos, vel; float life; Color c; };
struct Hater  { Vector3 pos; float fireT, hurtT, moveT, dir; int hp; bool alive; };
struct Pickup { Vector3 pos; float spin; bool alive; };

Player me;
Bullet bullets[100];
Spark sparks[60];
Hater haters[6];
Pickup pickups[3];
Camera3D cam;
Sound sHit, sKill, sJump, sShoot, sReload, sDash;
Texture2D pointsTex, bulletTex;
Model snakeModel;
float hitCd = 0, praiseMsgT = 0, winT = 0, hurtMsgT = 0, bonusMsgT = 0, pointT = 0;
float dashCd = 0, dashT = 0;
Vector3 dashDir;
const char *diffName[] = {"chill", "normal", "mean"};
const char *praiseMsg = "capybaras are perfect";
const char *hurtMsg = "capybara bad";
const char *bonusMsg = "+30";
int kills = 0, wave = 1;
bool dead = false;

BoundingBox map[] = {
    {{-27,0,-6},{-7.5,3,-3}}, {{7.5,0,-6},{27,3,-3}},
    {{-27,0,6},{-7.5,3,9}},   {{7.5,0,6},{27,3,9}},
    {{-2.7,0,-18},{2.7,2,-12.6}},
    {{-21,0,21},{-15,1.2,27}}, {{15,0,-27},{21,1.2,-21}},

    {{-33,0,-33},{-28,2.4,-25}}, {{28,0,25},{33,2.4,33}},
    {{-18,0,-33},{-12,3,-27}},   {{12,0,27},{18,3,33}},
    {{-6,0,18},{6,1.2,24}},
    {{-33,0,-12},{-27,2,-6}},    {{27,0,6},{33,2,12}},
    {{-12,0,12},{-6,2.5,18}},    {{6,0,-12},{12,2.5,-6}},
    {{-21,0,-12},{-15,1.5,-8}},  {{15,0,12},{21,1.5,16}},
    {{-9,0,27},{-3,2,33}},       {{3,0,-33},{9,2,-27}},
    {{-33,0,18},{-27,1.5,24}},   {{27,0,-18},{33,1.5,-12}},
    {{-3,0,-27},{3,3,-21}},      {{-3,0,30},{3,2,34}},
    {{-24,0,0},{-18,2.2,4}},     {{18,0,-12},{24,2.2,-8}},
    {{-12,0,-21},{-7,1.2,-15}},  {{7,0,15},{12,1.2,21}}
};
// white msg when a hater goes down. no words on the bullets anymore
const char *praise[] = {
    "capybaras are perfect", "capybara supremacy", "capybaras deserve a treat",
    "capybara is a good boy", "capybaras are chill like that", "respect the capybara"
};
const int NPRAISE = sizeof(praise) / sizeof(praise[0]);

// red insults when u get hit, they are NOT stuck on the balls anymore
const char *insults[] = {
    "capybara bad", "capybara overrated", "capybaras r mid",
    "capybara stole my lunch", "no capybaras allowed",
    "capybara = fancy rat", "capybara bad at math"
};
const int NINSULT = sizeof(insults) / sizeof(insults[0]);

float randf(float a, float b) { return a + (b - a) * GetRandomValue(0, 1000) / 1000.0f; }
bool inBoxXZ(Vector3 p, BoundingBox b, float pad) {
    return p.x > b.min.x - pad && p.x < b.max.x + pad &&
           p.z > b.min.z - pad && p.z < b.max.z + pad;
}
void sfx(Sound s, float v) { SetSoundVolume(s, v); PlaySound(s); }
void ctext(const char *s, int y, int fs, Color c) {
    DrawText(s, GetScreenWidth()/2 - MeasureText(s, fs)/2, y, fs, c);
}

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

// 1, 2, 4, 5 then keep 5, 
int waveSize(int r) {
    if (r == 1) return 1;
    if (r == 2) return 2;
    if (r == 3) return 4;
    if (r == 4) return 6;
    return 6;
}

// difficulty changes the haters, not just the menu text
float haterMoveSp() { return difficulty == 0 ? 1.5f : difficulty == 2 ? 3.1f : 2.2f; }
float haterShotSp() { return difficulty == 0 ? 8 : difficulty == 2 ? 14 : 11; }
float haterFireLo() { return difficulty == 0 ? 2.4f : difficulty == 2 ? 0.8f : 1.5f; }
float haterFireHi() { return difficulty == 0 ? 3.8f : difficulty == 2 ? 1.6f : 2.7f; }
int haterHpMax()   { return difficulty == 0 ? 1 : difficulty == 2 ? 3 : 2; }

void spawnRound() {
    for (Hater &h : haters) h.alive = false;
    if (mode == 1) return;      // online stays empty till netcode exists
    Vector3 spots[] = {{-24,0,-30}, {-30,0,-2}, {24,0,30}, {-24,0,30}, {30,0,-30}, {-30,0,30}};
    int n = waveSize(wave);
    for (int i = 0; i < n; i++)
        haters[i] = {spots[i], randf(1.0f, 2.6f), 0,
                     randf(0.5f, 2.0f), randf(0, 6.28318f), haterHpMax(), true};
}

void reset() {
    me = {{0, 0, 9}, 0, 0, 0, 0, 0, 0, true, 12, 100};
    for (Bullet &b : bullets) b.alive = false;
    for (Spark &s : sparks) s.life = 0;
    for (Pickup &p : pickups) p.alive = false;
    wave = 1; kills = 0; dead = false;
    pointT = randf(0.5f, 1.5f);
    praiseMsgT = hurtMsgT = bonusMsgT = winT = dashCd = dashT = 0;
    dashDir = {0,0,-1};
    spawnRound();
    cam = {{0, EH, 9}, {0, EH, 8}, {0, 1, 0}, 80, CAMERA_PERSPECTIVE};
}

void emit(Vector3 p, Color c, float sp, float life) {
    for (Spark &s : sparks) if (s.life <= 0) {
        s = {p, {randf(-sp, sp), randf(0, sp * 1.4f), randf(-sp, sp)}, life, c};
        return;
    }
}

// particle for a kill. now wired to hater deaths, K still previews it
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
        float lim[6] = {box.min.x-r, box.max.x+r, box.min.y-r, box.max.y+r,
                        box.min.z-r, box.max.z+r};
        if (b.pos.x < lim[0] || b.pos.x > lim[1] ||
            b.pos.y < lim[2] || b.pos.y > lim[3] ||
            b.pos.z < lim[4] || b.pos.z > lim[5]) continue;
        float d[6] = {b.pos.x-lim[0], lim[1]-b.pos.x, b.pos.y-lim[2],
                      lim[3]-b.pos.y, b.pos.z-lim[4], lim[5]-b.pos.z};
        int k = 0;
        for (int i = 1; i < 6; i++) if (d[i] < d[k]) k = i;
        float s = k % 2 ? 1 : -1;
        if (k < 2) { b.pos.x = lim[k]; b.vel.x = s * fabsf(b.vel.x); }
        else if (k < 4) { b.pos.y = lim[k]; b.vel.y = s * fabsf(b.vel.y); }
        else { b.pos.z = lim[k]; b.vel.z = s * fabsf(b.vel.z); }
        return true;
    }
    return false;
}

void update(float dt) {
    if (dead) return;
    Vector2 m = GetMouseDelta();
    if (fabsf(m.x) < 100 && fabsf(m.y) < 100) {
        me.yaw += m.x * 0.003f;
        me.pitch = Clamp(me.pitch - m.y * 0.003f, -1.3f, 1.3f);
    }
    Vector3 f = {sinf(me.yaw), 0, -cosf(me.yaw)}, r = {cosf(me.yaw), 0, sinf(me.yaw)};
    if (dashT > 0) dashT -= dt;
    if (dashCd > 0) dashCd -= dt;
    Vector3 move = {0,0,0};
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, f);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, f);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, r);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, r);
    if (Vector3LengthSqr(move) > 0) move = Vector3Normalize(move);
    if (IsKeyPressed(KEY_LEFT_SHIFT) && dashCd <= 0 && Vector3LengthSqr(move) > 0) {
        dashDir = move;
        dashT = 0.18f;
        dashCd = 3.0f;
        sfx(sDash, 0.4f);
    }
    if (dashT > 0) move = dashDir;
    if (Vector3LengthSqr(move) > 0) {
        float sp = dashT > 0 ? 32 : 9;
        me.pos.x = Clamp(me.pos.x + move.x * sp * dt, -A + 0.5f, A - 0.5f);
        me.pos.z = Clamp(me.pos.z + move.z * sp * dt, -A + 0.5f, A - 0.5f);
    }
    for (const BoundingBox &b : map) if (me.pos.y < b.max.y - 0.1f && inBoxXZ(me.pos, b, 0.4f)) {
        float lim[4] = {b.min.x-0.4f, b.max.x+0.4f, b.min.z-0.4f, b.max.z+0.4f};
        float d[4] = {me.pos.x-lim[0], lim[1]-me.pos.x, me.pos.z-lim[2], lim[3]-me.pos.z};
        int k = 0;
        for (int i = 1; i < 4; i++) if (d[i] < d[k]) k = i;
        if (k < 2) me.pos.x = lim[k]; else me.pos.z = lim[k];
    }
    if (IsKeyPressed(KEY_SPACE) && me.grounded) {
        me.vy = 7; me.grounded = false;
        sfx(sJump, 0.25f);
    }
    float oldY = me.pos.y;
    me.vy -= 18 * dt;
    me.pos.y += me.vy * dt;
    if (me.pos.y <= 0) { me.pos.y = 0; me.vy = 0; me.grounded = true; }
    for (const BoundingBox &b : map)
        if (me.vy <= 0 && oldY >= b.max.y && me.pos.y <= b.max.y && inBoxXZ(me.pos, b, 0.4f)) {
            me.pos.y = b.max.y; me.vy = 0; me.grounded = true;
        }

    // ammo/reload
    me.fireT -= dt; me.invuln -= dt;
    if (me.reloadT > 0 && (me.reloadT -= dt) <= 0) me.ammo = 12;
    if (IsKeyPressed(KEY_R) || me.ammo == 0) if (me.reloadT <= 0 && me.ammo < 12) {
        me.reloadT = 0.9f;
        sfx(sReload, 0.7f);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && me.fireT <= 0 && me.reloadT <= 0 && me.ammo > 0) {
        for (Bullet &b : bullets) if (!b.alive) {
            Vector3 d = {sinf(me.yaw)*cosf(me.pitch), sinf(me.pitch), -cosf(me.yaw)*cosf(me.pitch)};
            b = {Vector3Add((Vector3){me.pos.x, me.pos.y + EH, me.pos.z}, Vector3Scale(d, 0.5f)),
                 Vector3Scale(d, 36), 4, 0, true, true};
            me.ammo--; me.fireT = 0.14f;
            shake = 0.08f;
            sfx(sShoot, 0.6f);
            break;
        }
    }
    hitCd -= dt; praiseMsgT -= dt; hurtMsgT -= dt; bonusMsgT -= dt;
    for (Hater &h : haters) {
        if (!h.alive) continue;
        h.fireT -= dt; h.hurtT -= dt; h.moveT -= dt;
        if (h.moveT <= 0) { h.moveT = randf(1.0f, 2.5f); h.dir = randf(0, 6.28318f); }
        float hsp = haterMoveSp();
        Vector3 step = {sinf(h.dir) * hsp * dt, 0, cosf(h.dir) * hsp * dt};
        Vector3 np = Vector3Add(h.pos, step);
        np.x = Clamp(np.x, -A + 1, A - 1);
        np.z = Clamp(np.z, -A + 1, A - 1);
        bool stuck = false;
        for (const BoundingBox &b : map) if (inBoxXZ(np, b, 0.7f)) stuck = true;
        if (stuck) h.moveT = 0; else h.pos = np;
        if (h.fireT > 0) continue;
        h.fireT = randf(haterFireLo(), haterFireHi());
        for (Bullet &b : bullets) if (!b.alive) {
            Vector3 from = {h.pos.x, 2.1f, h.pos.z};
            Vector3 dir = Vector3Normalize(Vector3Subtract(
                (Vector3){me.pos.x, me.pos.y + 1, me.pos.z}, from));
            dir.x += randf(-0.07f, 0.07f);
            dir = Vector3Normalize(dir);
            b = {Vector3Add(from, Vector3Scale(dir, 0.6f)), Vector3Scale(dir, haterShotSp()),
                 7, 0, true, false};
            break;
        }
    }
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
                    sfx(sHit, 0.25f);
                }
            }
            // my bullets pop haters
            if (b.mine) for (Hater &h : haters) if (h.alive &&
                Vector3DistanceSqr(b.pos, (Vector3){h.pos.x, 1.2f, h.pos.z}) < 0.9f * 0.9f) {
                b.alive = false; h.hp--; h.hurtT = 0.12f;
                sfx(sHit, 0.8f);
                if (h.hp <= 0) {
                    h.alive = false; kills++;
                    Vector3 at = {h.pos.x, 1.2f, h.pos.z};
                    burst(at);
                    for (int q = 0; q < 6; q++) emit(at, RED, 4, 0.6f);
                    praiseMsg = praise[GetRandomValue(0, NPRAISE - 1)];
                    praiseMsgT = 1.8f;
                    sfx(sKill, 1.0f);
                }
                break;
            }
            //own shots damage after they bounce once
            Vector3 body = {me.pos.x, me.pos.y + 1, me.pos.z};
            if (me.invuln <= 0 && Vector3DistanceSqr(body, b.pos) < 0.8f * 0.8f &&
                (!b.mine || b.bounces > 0)) {
                b.alive = false;
                me.hp -= 10; me.invuln = 0.4f;
                if (!b.mine) {
                    hurtMsg = insults[GetRandomValue(0, NINSULT - 1)];
                    hurtMsgT = 1.1f;
                }
                shake = 0.25f;
            }
        }
    }
    pointT -= dt;
    if (pointT <= 0) {
        pointT = randf(0.5f, 1.5f);
        if (GetRandomValue(0, 99) < 5)
            for (Pickup &p : pickups) if (!p.alive) {
                for (int tries = 0; tries < 12; tries++) {
                    p.pos = {randf(-A+1, A-1), 0.8f, randf(-A+1, A-1)};
                    bool blocked = false;
                    for (const BoundingBox &b : map) if (inBoxXZ(p.pos, b, 1.0f)) blocked = true;
                    if (!blocked) break;
                }
                p.spin = 0; p.alive = true;
                break;
            }
    }
    for (Pickup &p : pickups) if (p.alive) {
        p.spin += dt * 1.5f;
        if (Vector3DistanceSqr(p.pos, (Vector3){me.pos.x, me.pos.y + 1, me.pos.z}) < 1.1f * 1.1f) {
            p.alive = false;
            me.hp += 30;
            if (me.hp > 100) me.hp = 100;
            bonusMsgT = 1.2f;
            sfx(sJump, 0.5f);
        }
    }
    if (me.hp <= 0 && !dead) {
        dead = true;
        hurtMsgT = 0;
        sfx(sKill, 0.9f);
    }
    // clear the round, then spawn the next bunch
    if (kills >= waveSize(wave) && winT <= 0) winT = 2.5f;
    if (winT > 0 && (winT -= dt) <= 0) {
        wave++; kills = 0; spawnRound();
    }
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
    Color wall = {230, 140, 215, 255};   // light magenta walls
    DrawCube((Vector3){0, WH/2, -A-0.5f}, A*2+1, WH, 1, wall);
    DrawCube((Vector3){0, WH/2,  A+0.5f}, A*2+1, WH, 1, wall);
    DrawCube((Vector3){-A-0.5f, WH/2, 0}, 1, WH, A*2+1, wall);
    DrawCube((Vector3){ A+0.5f, WH/2, 0}, 1, WH, A*2+1, wall);
    for (const BoundingBox &b : map) {
        Vector3 c = {(b.min.x+b.max.x)/2, (b.min.y+b.max.y)/2, (b.min.z+b.max.z)/2};
        Vector3 s = {b.max.x-b.min.x, b.max.y-b.min.y, b.max.z-b.min.z};
        DrawCubeV(c, s, (Color){35, 85, 50, 255});      // dark green boxes
        DrawCubeWiresV(c, s, (Color){100, 160, 110, 255});
    }
    // snakes are the natural enemy of capybaras. show them side on
    for (const Hater &h : haters) if (h.alive) {
        float bob = sinf(t * 4 + h.pos.x) * 0.05f;
        float face = atan2f(me.pos.x - h.pos.x, me.pos.z - h.pos.z) * RAD2DEG;
        Color skin = h.hurtT > 0 ? RED : WHITE;
        DrawModelEx(snakeModel, (Vector3){h.pos.x, 0.05f + bob, h.pos.z},
                    (Vector3){0, 1, 0}, face + 90, (Vector3){0.2f, 0.2f, 0.2f}, skin);
    }
    for (const Pickup &p : pickups) if (p.alive) {
        Vector3 at = {p.pos.x, p.pos.y + sinf(t * 3 + p.pos.x) * 0.15f, p.pos.z};
        Rectangle src = {0, 0, (float)pointsTex.width, (float)pointsTex.height};
        DrawBillboardPro(cam, pointsTex, src, at, (Vector3){0, 1, 0},
                         (Vector2){1.0f, (float)pointsTex.height / pointsTex.width},
                         (Vector2){0.5f, 0.5f}, p.spin, WHITE);
    }
    Rectangle bs = {0, 0, (float)bulletTex.width, (float)bulletTex.height};
    for (const Bullet &b : bullets) {
        if (!b.alive) continue;
        Color trail = b.mine ? (Color){210, 170, 110, 255} : RED;
        Color tint = b.mine ? WHITE : RED;
        DrawLine3D(Vector3Subtract(b.pos, Vector3Scale(Vector3Normalize(b.vel), 0.8f)), b.pos, trail);
        DrawBillboardPro(cam, bulletTex, bs, b.pos, (Vector3){0,1,0},
                         (Vector2){0.72f, 0.72f}, (Vector2){0.5f,0.5f}, 0, tint);
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

// tags, praise, insults, bonus, win msg.
void drawHateText() {
    Vector3 fw = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    auto tag = [&](Vector3 p, const char *say, int fs, Color c) {
        Vector3 to = Vector3Subtract(p, cam.position);
        if (Vector3DotProduct(to, fw) <= 0) return;
        Vector2 s = GetWorldToScreen(p, cam);
        DrawText(say, (int)s.x - MeasureText(say, fs)/2, (int)s.y - fs/2, fs, c);
    };
    for (const Hater &h : haters) if (h.alive)
        tag((Vector3){h.pos.x, 2.2f, h.pos.z}, "your ex", 18, LIGHTGRAY);
    if (praiseMsgT > 0)
        ctext(praiseMsg, GetScreenHeight()/2 - 20, 36, Fade(WHITE, Clamp(praiseMsgT, 0, 1)));
    if (hurtMsgT > 0)
        ctext(hurtMsg, GetScreenHeight()/2 + 70, 38, Fade(RED, Clamp(hurtMsgT, 0, 1)));
    if (bonusMsgT > 0)
        DrawText(bonusMsg, GetScreenWidth()/2 + 24, GetScreenHeight()/2 + 26, 22,
                 Fade(WHITE, Clamp(bonusMsgT, 0, 1)));
    if (winT > 0)
        ctext(TextFormat("ROUND %d CLEARED - more haters loading...", wave),
              GetScreenHeight()/2 - 140, 30, RAYWHITE);
}

void drawMinimap() {
    int mx = W - 186, my = 20, ms = 160;
    auto px = [&](float v) { return (int)(mx + (v + A) / (2*A) * ms); };
    auto py = [&](float v) { return (int)(my + (v + A) / (2*A) * ms); };
    DrawRectangle(mx - 6, my - 6, ms + 12, ms + 12, Fade(BLACK, 0.55f));
    DrawRectangleLines(mx, my, ms, ms, LIGHTGRAY);
    for (const BoundingBox &b : map)
        DrawRectangle(px(b.min.x), py(b.min.z), px(b.max.x)-px(b.min.x),
                      py(b.max.z)-py(b.min.z), Fade(GRAY, 0.9f));
    auto dot = [&](float x, float z, Color c, int r) { DrawCircle(px(x), py(z), r, c); };
    for (const Hater &h : haters) if (h.alive) dot(h.pos.x, h.pos.z, RED, 4);
    for (const Pickup &p : pickups) if (p.alive) dot(p.pos.x, p.pos.z, LIGHTGRAY, 2);
    dot(me.pos.x, me.pos.z, RAYWHITE, 4);
    DrawLine(px(me.pos.x), py(me.pos.z),
             px(me.pos.x + sinf(me.yaw)*2), py(me.pos.z - cosf(me.yaw)*2), RAYWHITE);
    DrawText(mode ? "players" : "haters", mx, my + ms + 8, 14, LIGHTGRAY);
}

void drawMenu() {
    ClearBackground(DARKGRAY);
    DrawText("Hail the capybara", 80, 90, 56, RAYWHITE);
    DrawText("bounce back - bullets go brrr", 84, 165, 20, LIGHTGRAY);
    if (screen == S_NAME) {
        DrawText(TextFormat("name: %s_", pname), 84, 250, 32, RAYWHITE);
        DrawText("letters/numbers only - enter to continue", 84, 300, 20, LIGHTGRAY);
    } else if (screen == S_DIFF) {
        DrawText("pick difficulty", 84, 250, 36, RAYWHITE);
        DrawText("1  chill   - slow haters, soft bullets", 84, 320, 24, RAYWHITE);
        DrawText("2  normal  - the usual", 84, 360, 24, RAYWHITE);
        DrawText("3  mean    - fast angry haters", 84, 400, 24, RAYWHITE);
        DrawText("backspace - back", 84, 470, 20, LIGHTGRAY);
    } else {
        DrawText(TextFormat("name: %s", pname), 84, 250, 28, RAYWHITE);
        DrawText("1  bot mode   (any name)", 84, 320, 26, RAYWHITE);
        DrawText("2  online mode (name is reserved)", 84, 360, 26, RAYWHITE);
        if (nameTaken) DrawText("NAME UNAVAILABLE - pick another name", 84, 415, 22, RED);
        DrawText("backspace - change name", 84, 465, 20, LIGHTGRAY);
        DrawText("WASD=move\n Left click= shoot\n R = reload\n SHIFT+WASD = dash", 84, 505, 18, LIGHTGRAY);
        DrawText("bot mode: kill the natural enemy of capybara. Snake", 84, 580, 18, LIGHTGRAY);
    }
}

int main() {
    InitWindow(W, H, "Hail the capybara");
    //bot adn multiplauer
    DisableCursor();
    InitAudioDevice();
    sHit = LoadSound("assets/hit.mp3");
    sKill = LoadSound("assets/kill.mp3");
    sJump = LoadSound("assets/jump.mp3");
    sShoot = LoadSound("assets/shoot.mp3");
    sReload = LoadSound("assets/reload.mp3");
    sDash = LoadSound("assets/dash.mp3");
    pointsTex = LoadTexture("assets/points.png");
    bulletTex = LoadTexture("assets/capybara_bullet.png");
    snakeModel = LoadModel("assets/snake/Snake.obj");
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
            if (IsKeyPressed(KEY_ONE)) { mode = 0; screen = S_DIFF; }
            if (IsKeyPressed(KEY_TWO) && !nameTaken) { mode = 1; reset(); screen = S_GAME; }
            if (IsKeyPressed(KEY_BACKSPACE)) screen = S_NAME;
        } else if (screen == S_DIFF) {
            int keys[] = {KEY_ONE, KEY_TWO, KEY_THREE};
            for (int i = 0; i < 3; i++)
                if (IsKeyPressed(keys[i])) { difficulty = i; reset(); screen = S_GAME; }
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) screen = S_MENU;
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) { screen = S_MENU; dead = false; }
            if (dead) {
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R)) reset();
            } else update(dt);
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);
        if (screen == S_GAME) {
            BeginMode3D(cam);
            drawWorld();
            EndMode3D();
            drawCrosshair();
            drawHateText();
            DrawText(TextFormat("%s  |  %s  |  %s  |  ammo %d", pname,
                                mode ? "ONLINE" : "BOTS", diffName[difficulty], me.ammo),
                     20, 20, 20, RAYWHITE);
            DrawText(TextFormat("HP %d", me.hp), 20, 50, 20, LIGHTGRAY);
            if (mode == 0)
                DrawText(TextFormat("round %d", wave), 20, 75, 20, LIGHTGRAY);
            DrawText("dash", 20, 100, 16, LIGHTGRAY);
            DrawRectangle(66, 102, 120, 12, Fade(BLACK, 0.6f));
            DrawRectangle(66, 102, (int)(120 * (1 - dashCd / 3.0f)), 12,
                          dashCd <= 0 ? RAYWHITE : LIGHTGRAY);
            DrawRectangleLines(66, 102, 120, 12, LIGHTGRAY);
            drawMinimap();
            if (dead) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.72f));
                ctext(TextFormat("round %d", wave), 24, 24, RAYWHITE);
                ctext("U DIED", GetScreenHeight()/2 - 100, 72, RED);
                DrawText("ENTER = respawn   ESC = menu", GetScreenWidth()/2 - 190,
                         GetScreenHeight()/2 + 20, 20, LIGHTGRAY);
            }
        } else drawMenu();
        EndDrawing();
    }
    UnloadSound(sHit); UnloadSound(sKill); UnloadSound(sJump);
    UnloadSound(sShoot); UnloadSound(sReload); UnloadSound(sDash);
    UnloadModel(snakeModel);
    UnloadTexture(pointsTex); UnloadTexture(bulletTex);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
