// capy shooter lol
#include "raylib.h"
#include "raymath.h"
#include <cctype>
#include <cstring>
#include <cmath>
#include <string>
#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

const int W = 1280, H = 720;
const float A = 36.0f, WH = 6.0f, EH = 1.65f;
const float BGM_VOL = 0.25f;

enum Screen { S_NAME, S_MENU, S_GAME };
Screen screen = S_NAME;
char pname[16] = "";
float t = 0, hitCd = 0;

struct Player { Vector3 pos; float yaw, pitch, vy, fireT, reloadT; bool grounded; int ammo; };
struct Bullet { Vector3 pos, vel; float life; int bounces; bool alive; };
struct Spark { Vector3 pos, vel; float life; Color c; };

Player me;
Bullet bullets[100];
Spark sparks[60];
Camera3D cam;
Sound sHit, sJump, sShoot, sReload, sDash;
Music bgm;
bool haveBgm = false, bgmStarted = false;
Texture2D bulletTex, charTex;
Color outfit[6];
float dashCd = 0, dashT = 0;
Vector3 dashDir = {0, 0, -1};

BoundingBox map[] = {
    {{-27,0,-6},{-7.5,3,-3}}, {{7.5,0,-6},{27,3,-3}},
    {{-27,0,6},{-7.5,3,9}},   {{7.5,0,6},{27,3,9}},
    {{-2.7,0,-18},{2.7,2,-12.6}},
    {{-21,0,21},{-15,1.2,27}}, {{15,0,-27},{21,1.2,-21}},
    {{-33,0,-33},{-28,2.4,-25}}, {{28,0,25},{33,2.4,33}},
    {{-18,0,-33},{-12,3,-27}}, {{12,0,27},{18,3,33}},
    {{-6,0,18},{6,1.2,24}},
    {{-33,0,-12},{-27,2,-6}},  {{27,0,6},{33,2,12}},
    {{-12,0,12},{-6,2.5,18}},  {{6,0,-12},{12,2.5,-6}},
    {{-21,0,-12},{-15,1.5,-8}}, {{15,0,12},{21,1.5,16}},
    {{-9,0,27},{-3,2,33}},     {{3,0,-33},{9,0,-27}},
    {{-33,0,18},{-27,1.5,24}}, {{27,0,-18},{33,1.5,-12}},
    {{-3,0,-27},{3,3,-21}},    {{-3,0,30},{3,2,34}},
    {{-24,0,0},{-18,2.2,4}},   {{18,0,-12},{24,2.2,-8}},
    {{-12,0,-21},{-7,1.2,-15}}, {{7,0,15},{12,1.2,21}}
};

float randf(float a, float b) { return a + (b - a) * GetRandomValue(0, 1000) / 1000.0f; }
bool inBoxXZ(Vector3 p, BoundingBox b, float pad) {
    return p.x > b.min.x - pad && p.x < b.max.x + pad && p.z > b.min.z - pad && p.z < b.max.z + pad;
}
void sfx(Sound s, float v) { SetSoundVolume(s, v); PlaySound(s); }

void startBgm() {
    if (!haveBgm || bgmStarted) return;
    SetMusicVolume(bgm, BGM_VOL); PlayMusicStream(bgm); bgmStarted = true;
}
#ifdef PLATFORM_WEB
extern "C" EMSCRIPTEN_KEEPALIVE void capyAudioUnlocked() {
    if (!haveBgm) return;
    PlayMusicStream(bgm); SetMusicVolume(bgm, BGM_VOL); bgmStarted = true;
}
#endif

void ctext(const char *s, int y, int fs, Color c) {
    int x = GetScreenWidth() / 2 - MeasureText(s, fs) / 2;
    DrawText(s, x + 2, y + 2, fs, BLACK); DrawText(s, x, y, fs, c);
}

const char *outfitFiles[] = {
    "assets/character/body.png", "assets/character/jacket.png", "assets/character/jeans.png",
    "assets/character/shoes.png", "assets/character/gloves.png", "assets/character/backpack.png"
};
void makeCharTex() {
    for (int i = 0; i < 6; i++) {
        Image im = LoadImage(outfitFiles[i]);
        Color sum = {0,0,0,0}; int n = 0;
        for (int y = 0; y < im.height; y += 4) for (int x = 0; x < im.width; x += 4) {
            Color c = GetImageColor(im, x, y);
            sum.r += c.r; sum.g += c.g; sum.b += c.b; n++;
        }
        outfit[i] = {(unsigned char)(sum.r/n), (unsigned char)(sum.g/n), (unsigned char)(sum.b/n), 255};
        UnloadImage(im);
    }
    Image s = GenImageColor(32, 48, {0,0,0,0});
    ImageDrawRectangle(&s, 11, 2, 10, 8, outfit[0]);
    ImageDrawRectangle(&s, 12, 9, 8, 3, outfit[0]);
    ImageDrawRectangle(&s, 10, 11, 12, 15, outfit[1]);
    ImageDrawRectangle(&s, 6, 12, 4, 13, outfit[1]);
    ImageDrawRectangle(&s, 22, 12, 4, 13, outfit[1]);
    ImageDrawRectangle(&s, 6, 25, 4, 5, outfit[4]);
    ImageDrawRectangle(&s, 22, 25, 4, 5, outfit[4]);
    ImageDrawRectangle(&s, 11, 26, 4, 14, outfit[2]);
    ImageDrawRectangle(&s, 17, 26, 4, 14, outfit[2]);
    ImageDrawRectangle(&s, 10, 40, 5, 6, outfit[3]);
    ImageDrawRectangle(&s, 17, 40, 5, 6, outfit[3]);
    ImageDrawRectangle(&s, 12, 12, 8, 10, outfit[5]);
    charTex = LoadTextureFromImage(s); UnloadImage(s);
}

void reset() {
    me = {{0,0,9}, 0, 0, 0, 0, 0, true, 12};
    for (Bullet &b : bullets) b.alive = false;
    for (Spark &s : sparks) s.life = 0;
    dashCd = dashT = 0; dashDir = {0,0,-1};
    cam = {{0,EH,9}, {0,EH,8}, {0,1,0}, 80, CAMERA_PERSPECTIVE};
}

void emit(Vector3 p, Color c, float sp, float life) {
    for (Spark &s : sparks) if (s.life <= 0) {
        s = {p, {randf(-sp,sp), randf(0,sp*1.4f), randf(-sp,sp)}, life, c}; return;
    }
}

// bullets go boing
bool bounce(Bullet &b) {
    float r = 0.12f;
    if (b.pos.x < -A+r)  { b.pos.x = -A+r;  b.vel.x = fabsf(b.vel.x);  return true; }
    if (b.pos.x >  A-r)  { b.pos.x =  A-r;  b.vel.x = -fabsf(b.vel.x); return true; }
    if (b.pos.z < -A+r)  { b.pos.z = -A+r;  b.vel.z = fabsf(b.vel.z);  return true; }
    if (b.pos.z >  A-r)  { b.pos.z =  A-r;  b.vel.z = -fabsf(b.vel.z); return true; }
    if (b.pos.y < r)     { b.pos.y = r;     b.vel.y = fabsf(b.vel.y);  return true; }
    if (b.pos.y > WH-r)  { b.pos.y = WH-r;  b.vel.y = -fabsf(b.vel.y); return true; }
    for (const BoundingBox &box : map) {
        float lim[6] = {box.min.x-r, box.max.x+r, box.min.y-r, box.max.y+r, box.min.z-r, box.max.z+r};
        if (b.pos.x < lim[0] || b.pos.x > lim[1] || b.pos.y < lim[2] ||
            b.pos.y > lim[3] || b.pos.z < lim[4] || b.pos.z > lim[5]) continue;
        float d[6] = {b.pos.x-lim[0], lim[1]-b.pos.x, b.pos.y-lim[2],
                      lim[3]-b.pos.y, b.pos.z-lim[4], lim[5]-b.pos.z};
        int k = 0;
        for (int i = 1; i < 6; i++) if (d[i] < d[k]) k = i;
        float s = k % 2 ? 1 : -1;
        if (k < 2) { b.pos.x = lim[k]; b.vel.x = s*fabsf(b.vel.x); }
        else if (k < 4) { b.pos.y = lim[k]; b.vel.y = s*fabsf(b.vel.y); }
        else { b.pos.z = lim[k]; b.vel.z = s*fabsf(b.vel.z); }
        return true;
    }
    return false;
}

void update(float dt) {
    Vector2 m = GetMouseDelta();
    if (fabsf(m.x) < 100 && fabsf(m.y) < 100) {
        me.yaw += m.x * 0.003f; me.pitch = Clamp(me.pitch - m.y * 0.003f, -1.3f, 1.3f);
    }
    Vector3 f = {sinf(me.yaw), 0, -cosf(me.yaw)}, r = {cosf(me.yaw), 0, sinf(me.yaw)};
    Vector3 move = {0,0,0};
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, f);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, f);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, r);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, r);
    if (Vector3LengthSqr(move) > 0) move = Vector3Normalize(move);

    // dash only with shift + wasd
    dashCd -= dt; dashT -= dt;
    if (IsKeyPressed(KEY_LEFT_SHIFT) && dashCd <= 0 && Vector3LengthSqr(move) > 0) {
        dashDir = move; dashT = 0.18f; dashCd = 3.0f; sfx(sDash, 0.4f);
    }
    if (dashT > 0) move = dashDir;
    if (Vector3LengthSqr(move) > 0) {
        float sp = dashT > 0 ? 32 : 9;
        me.pos.x = Clamp(me.pos.x + move.x*sp*dt, -A+0.5f, A-0.5f);
        me.pos.z = Clamp(me.pos.z + move.z*sp*dt, -A+0.5f, A-0.5f);
    }
    for (const BoundingBox &b : map) if (me.pos.y < b.max.y-0.1f && inBoxXZ(me.pos, b, 0.4f)) {
        float lim[4] = {b.min.x-0.4f, b.max.x+0.4f, b.min.z-0.4f, b.max.z+0.4f};
        float d[4] = {me.pos.x-lim[0], lim[1]-me.pos.x, me.pos.z-lim[2], lim[3]-me.pos.z};
        int k = 0;
        for (int i = 1; i < 4; i++) if (d[i] < d[k]) k = i;
        if (k < 2) me.pos.x = lim[k]; else me.pos.z = lim[k];
    }
    if (IsKeyPressed(KEY_SPACE) && me.grounded) {
        me.vy = 7; me.grounded = false; sfx(sJump, 0.25f);
    }
    float oldY = me.pos.y;
    me.vy -= 18*dt; me.pos.y += me.vy*dt;
    if (me.pos.y <= 0) { me.pos.y = 0; me.vy = 0; me.grounded = true; }
    for (const BoundingBox &b : map)
        if (me.vy <= 0 && oldY >= b.max.y && me.pos.y <= b.max.y && inBoxXZ(me.pos, b, 0.4f)) {
            me.pos.y = b.max.y; me.vy = 0; me.grounded = true;
        }

    me.fireT -= dt;
    if (me.reloadT > 0 && (me.reloadT -= dt) <= 0) me.ammo = 12;
    if ((IsKeyPressed(KEY_R) || me.ammo == 0) && me.reloadT <= 0 && me.ammo < 12) {
        me.reloadT = 0.9f; sfx(sReload, 0.7f);
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && me.fireT <= 0 && me.reloadT <= 0 && me.ammo > 0)
        for (Bullet &b : bullets) if (!b.alive) {
            Vector3 d = {sinf(me.yaw)*cosf(me.pitch), sinf(me.pitch), -cosf(me.yaw)*cosf(me.pitch)};
            b = {Vector3Add((Vector3){me.pos.x, me.pos.y+EH, me.pos.z}, Vector3Scale(d, 0.5f)),
                 Vector3Scale(d, 36), 4, 0, true};
            me.ammo--; me.fireT = 0.14f; sfx(sShoot, 0.6f); break;
        }

    hitCd -= dt;
    for (Bullet &b : bullets) {
        if (!b.alive) continue;
        if ((b.life -= dt) <= 0) { b.alive = false; continue; }
        for (int s = 0; s < 4 && b.alive; s++) {
            b.pos = Vector3Add(b.pos, Vector3Scale(b.vel, dt/4));
            if (bounce(b)) {
                if (++b.bounces > 8) { b.alive = false; continue; }
                if (hitCd <= 0) { hitCd = 0.05f; emit(b.pos, LIGHTGRAY, 2, 0.25f); sfx(sHit, 0.25f); }
            }
        }
    }
    for (Spark &s : sparks) if (s.life > 0) {
        s.life -= dt; s.vel.y -= 12*dt; s.pos = Vector3Add(s.pos, Vector3Scale(s.vel, dt));
    }
    Vector3 eye = {me.pos.x, me.pos.y+EH, me.pos.z};
    cam.position = eye;
    cam.target = Vector3Add(eye, (Vector3){sinf(me.yaw)*cosf(me.pitch), sinf(me.pitch),
                                           -cosf(me.yaw)*cosf(me.pitch)});
}

void drawWorld() {
    DrawPlane((Vector3){0,0,0}, (Vector2){A*2, A*2}, DARKGRAY);
    Color grid = Fade(LIGHTGRAY, 0.35f + 0.15f*sinf(t*4));
    for (float i = -A; i <= A; i += 2) {
        DrawLine3D((Vector3){i,0.02f,-A}, (Vector3){i,0.02f,A}, grid);
        DrawLine3D((Vector3){-A,0.02f,i}, (Vector3){A,0.02f,i}, grid);
    }
    Color wall = {230,140,215,255};
    DrawCube((Vector3){0,WH/2,-A-0.5f}, A*2+1, WH, 1, wall);
    DrawCube((Vector3){0,WH/2, A+0.5f}, A*2+1, WH, 1, wall);
    DrawCube((Vector3){-A-0.5f,WH/2,0}, 1, WH, A*2+1, wall);
    DrawCube((Vector3){ A+0.5f,WH/2,0}, 1, WH, A*2+1, wall);
    for (const BoundingBox &b : map) {
        Vector3 c = {(b.min.x+b.max.x)/2, (b.min.y+b.max.y)/2, (b.min.z+b.max.z)/2};
        Vector3 s = {b.max.x-b.min.x, b.max.y-b.min.y, b.max.z-b.min.z};
        DrawCubeV(c, s, (Color){35,85,50,255});
        DrawCubeWiresV(c, s, (Color){100,160,110,255});
    }
    // capy billbord
    Rectangle cs = {0,0,(float)charTex.width,(float)charTex.height};
    DrawBillboardPro(cam, charTex, cs, (Vector3){me.pos.x,me.pos.y+0.98f,me.pos.z},
                     (Vector3){0,1,0}, (Vector2){1.25f,1.875f}, (Vector2){0.5f,0.5f}, 0, WHITE);
    Rectangle bs = {0,0,(float)bulletTex.width,(float)bulletTex.height};
    for (const Bullet &b : bullets) if (b.alive) {
        DrawLine3D(Vector3Subtract(b.pos, Vector3Scale(Vector3Normalize(b.vel), 0.8f)), b.pos,
                   (Color){210,170,110,255});
        DrawBillboardPro(cam, bulletTex, bs, b.pos, (Vector3){0,1,0},
                         (Vector2){0.72f,0.72f}, (Vector2){0.5f,0.5f}, 0, WHITE);
    }
    for (const Spark &s : sparks) if (s.life > 0)
        DrawCube(s.pos, 0.13f, 0.13f, 0.13f, Fade(s.c, Clamp(s.life,0,1)));
}

void drawCrosshair() {
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    DrawLine(cx-14,cy,cx-5,cy,RAYWHITE); DrawLine(cx+5,cy,cx+14,cy,RAYWHITE);
    DrawLine(cx,cy-14,cx,cy-5,RAYWHITE); DrawLine(cx,cy+5,cx,cy+14,RAYWHITE);
}

void drawMinimap() {
    int mx = W-186, my = 20, ms = 160;
    auto px = [&](float v) { return (int)(mx + (v+A)/(2*A)*ms); };
    auto py = [&](float v) { return (int)(my + (v+A)/(2*A)*ms); };
    DrawRectangle(mx-6,my-6,ms+12,ms+12,Fade(BLACK,0.55f));
    DrawRectangleLines(mx,my,ms,ms,LIGHTGRAY);
    for (const BoundingBox &b : map)
        DrawRectangle(px(b.min.x),py(b.min.z),px(b.max.x)-px(b.min.x),py(b.max.z)-py(b.min.z),
                      Fade(GRAY,0.9f));
    DrawCircle(px(me.pos.x),py(me.pos.z),4,RAYWHITE);
    DrawLine(px(me.pos.x),py(me.pos.z),px(me.pos.x+sinf(me.yaw)*2),py(me.pos.z-cosf(me.yaw)*2),RAYWHITE);
    DrawText("capy", mx, my+ms+8, 14, LIGHTGRAY);
}

// menus r centered
void drawMenu() {
    ClearBackground(DARKGRAY);
    float scale = (float)GetScreenHeight()/bulletTex.height;
    float bw = bulletTex.width*scale, bh = bulletTex.height*scale;
    DrawTextureEx(bulletTex, {GetScreenWidth()/2-bw/2, GetScreenHeight()/2-bh/2}, 0, scale, WHITE);
    ctext("Hail the capybara", 100, 56, RAYWHITE);
    ctext("bounce back - bullets go brrr", 170, 20, LIGHTGRAY);
    if (screen == S_NAME) {
        ctext(TextFormat("name: %s_", pname), 270, 36, RAYWHITE);
        ctext("letters/numbers only - enter to continue", 335, 20, LIGHTGRAY);
    } else {
        ctext(TextFormat("name: %s", pname), 245, 28, RAYWHITE);
        ctext("1  play", 310, 30, RAYWHITE);
        ctext("enter = go    backspace = change name", 370, 20, LIGHTGRAY);
        ctext("WASD move   mouse aim   left click shoot", 440, 18, LIGHTGRAY);
        ctext("space jump   SHIFT+WASD dash   R reload", 470, 18, LIGHTGRAY);
        ctext("no peeps online, just you and the bullets", 560, 18, LIGHTGRAY);
    }
}

int main() {
    InitWindow(W, H, "Hail the capybara");
    DisableCursor(); InitAudioDevice();
    sHit = LoadSound("assets/hit.mp3"); sJump = LoadSound("assets/jump.mp3");
    sShoot = LoadSound("assets/shoot.mp3"); sReload = LoadSound("assets/reload.mp3");
    sDash = LoadSound("assets/dash.mp3");
    // bgm stays low
    if (FileExists("assets/bgm.mp3")) {
        bgm = LoadMusicStream("assets/bgm.mp3");
        if (bgm.ctxData != NULL) {
            bgm.looping = true; SetMusicVolume(bgm, BGM_VOL); haveBgm = true; startBgm();
        }
    }
    bulletTex = LoadTexture("assets/capybara_bullet.png");
    makeCharTex();
    SetTargetFPS(120); SetExitKey(KEY_NULL); reset();

    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.05f);
        t += dt;
        if (haveBgm) UpdateMusicStream(bgm);
        if (screen == S_NAME) {
            for (int c = GetCharPressed(); c; c = GetCharPressed())
                if ((isalnum(c) || c == '_') && strlen(pname) < 15) {
                    size_t n = strlen(pname); pname[n] = (char)c; pname[n+1] = 0;
                }
            if (IsKeyPressed(KEY_BACKSPACE) && strlen(pname)) pname[strlen(pname)-1] = 0;
            if (IsKeyPressed(KEY_ENTER) && strlen(pname)) screen = S_MENU;
        } else if (screen == S_MENU) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ONE)) { reset(); screen = S_GAME; }
            if (IsKeyPressed(KEY_BACKSPACE)) screen = S_NAME;
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) screen = S_MENU; else update(dt);
        }
        BeginDrawing();
        ClearBackground(DARKGRAY);
        if (screen == S_GAME) {
            BeginMode3D(cam); drawWorld(); EndMode3D();
            drawCrosshair();
            DrawText(TextFormat("%s  |  ammo %d", pname, me.ammo), 20, 20, 20, RAYWHITE);
            if (me.reloadT > 0) DrawText("reloading...", 20, 50, 18, LIGHTGRAY);
            DrawText("dash", 20, 80, 16, LIGHTGRAY);
            DrawRectangle(66,82,120,12,Fade(BLACK,0.6f));
            DrawRectangle(66,82,(int)(120*(1-dashCd/3.0f)),12,dashCd <= 0 ? RAYWHITE : LIGHTGRAY);
            DrawRectangleLines(66,82,120,12,LIGHTGRAY);
            drawMinimap();
        } else drawMenu();
        EndDrawing();
    }
    // clean up stuff
    if (haveBgm) UnloadMusicStream(bgm);
    UnloadSound(sHit); UnloadSound(sJump); UnloadSound(sShoot);
    UnloadSound(sReload); UnloadSound(sDash);
    UnloadTexture(bulletTex); UnloadTexture(charTex);
    CloseAudioDevice(); CloseWindow();
    return 0;
}
