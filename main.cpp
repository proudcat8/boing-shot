// Hail the capybara - your ex shoots red bullets, you shoot it down lol
#include "raylib.h"
#include "raymath.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono>
#endif

const int W = 1280, H = 720;
const float A = 36.0f, WH = 6.0f, EH = 1.65f;

enum Screen { S_NAME, S_MENU, S_DIFF, S_ROOM, S_GAME };
Screen screen = S_NAME;
char pname[16] = "", roomCode[8] = "";
bool roomStarted = false;
int mode = 0, difficulty = 1;
bool nameTaken = false;
float t = 0, shake = 0;

struct Player { Vector3 pos; float yaw, pitch, vy, fireT, reloadT, invuln; bool grounded; int ammo, hp; };
struct Bullet { Vector3 pos, vel; float life; int bounces, owner; bool alive, mine; };
struct Spark  { Vector3 pos, vel; float life; Color c; };
struct Hater  { Vector3 pos; float fireT, hurtT, moveT, dir; int hp; bool alive; };
struct Pickup { Vector3 pos; float spin; bool alive; };
struct NetPlayer { int id, hp; char name[16]; Vector3 pos; float yaw; bool alive; };

Player me;
Bullet bullets[100];
Spark sparks[60];
Hater haters[6];
Pickup pickups[3];
NetPlayer netPlayers[64];
Camera3D cam;
Sound sHit, sKill, sJump, sShoot, sReload, sDash;
Music bgm;
bool haveBgm = false;
Texture2D pointsTex, bulletTex, charTex;
Model snakeModel;
Color outfit[6];
std::string netBuf;
int myId = 0;
float netT = 0;
bool netReady = false;
#ifdef PLATFORM_WEB
EMSCRIPTEN_WEBSOCKET_T ws = 0;
#else
int netSock = -1;
std::string netIn;
#endif
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
    charTex = LoadTextureFromImage(s);
    UnloadImage(s);
}

#ifdef PLATFORM_WEB
EM_BOOL wsOpen(int, const EmscriptenWebSocketOpenEvent *, void *) { netReady = true; return true; }
EM_BOOL wsMsg(int, const EmscriptenWebSocketMessageEvent *e, void *) {
    if (e->isText) for (int i = 0; i < e->numBytes; i++) {
        char c = ((const char *)e->data)[i];
        if (c) netBuf += c;
    }
    return true;
}
EM_BOOL wsClose(int, const EmscriptenWebSocketCloseEvent *, void *) { netReady = false; return true; }
void netSend(const char *) {}
void netClose() {
    if (ws > 0) emscripten_websocket_close(ws, 1000, "bye");
    ws = 0;
    netReady = false;
}
#else
const int LAN_PORT = 8765;
const char *LAN_ALPHA = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

struct RelayClient {
    int fd = -1, id = 0;
    bool ready = false;
    std::string in;
    char name[16] = "capy";
    float x = 0, y = 0, z = 0, yaw = 0;
    int hp = 100;
};
std::vector<RelayClient> relayPeers;
std::vector<int> relayFree;
std::recursive_mutex relayMx;
std::string relayCode;
std::atomic<bool> relayOn{false};
int relayTcpFd = -1, relayUdpFd = -1, relayNext = 1;
std::thread relayThread;
long long relayHit[64][64] = {};

std::string makeRelayCode() {
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(nullptr)); seeded = true; }
    std::string s;
    for (int i = 0; i < 6; i++) s += LAN_ALPHA[rand() % 32];
    return s;
}

bool sendAll(int fd, const char *b, size_t n) {
    while (n) {
        int k = send(fd, b, (int)n, MSG_NOSIGNAL);
        if (k <= 0) return false;
        b += k; n -= (size_t)k;
    }
    return true;
}

void relayFrame(int fd, const std::string &s) {
    std::string f;
    size_t n = s.size();
    f += (char)0x81;
    if (n < 126) f += (char)n;
    else if (n < 65536) { f += (char)126; f += (char)(n >> 8); f += (char)n; }
    else return;
    f += s;
    sendAll(fd, f.data(), f.size());
}

void relayBroadcast(const std::string &s, int skip = 0) {
    std::lock_guard<std::recursive_mutex> lk(relayMx);
    for (const RelayClient &c : relayPeers)
        if (c.ready && c.id != skip) relayFrame(c.fd, s);
}

std::string relayState(const RelayClient &c) {
    char b[160];
    snprintf(b, sizeof(b), "P %d %s %.2f %.2f %.2f %.2f %d",
             c.id, c.name, c.x, c.y, c.z, c.yaw, c.hp);
    return b;
}

void relayWelcome(RelayClient &c) {
    std::lock_guard<std::recursive_mutex> lk(relayMx);
    relayFrame(c.fd, "W " + std::to_string(c.id) + " " + relayCode);
    for (const RelayClient &p : relayPeers)
        if (p.ready && p.fd != c.fd) relayFrame(c.fd, relayState(p));
    relayBroadcast(relayState(c), c.id);
}

void relayHandle(int id, const std::string &line) {
    std::lock_guard<std::recursive_mutex> lk(relayMx);
    std::istringstream in(line);
    char tag = 0;
    in >> tag;
    RelayClient *self = nullptr;
    for (RelayClient &c : relayPeers) if (c.id == id) self = &c;
    if (!self) return;

    if (tag == 'S') {
        int sid, hp;
        float x, y, z, yaw;
        char nm[16];
        if (!(in >> sid >> nm >> x >> y >> z >> yaw >> hp) || sid != id) return;
        snprintf(self->name, sizeof(self->name), "%s", nm);
        self->x = x; self->y = y; self->z = z; self->yaw = yaw; self->hp = hp;
        relayBroadcast(relayState(*self), id);
    } else if (tag == 'F') {
        int sid;
        if ((in >> sid) && sid == id) relayBroadcast(line, id);
    } else if (tag == 'H') {
        int src, dst;
        if (!(in >> src >> dst) || src != id || src == dst) return;
        if (src < 0 || src >= 64 || dst < 0 || dst >= 64) return;
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - relayHit[src][dst] < 120) return;
        relayHit[src][dst] = now;
        RelayClient *target = nullptr;
        for (RelayClient &c : relayPeers) if (c.id == dst) target = &c;
        if (!target) return;
        target->hp -= 10;
        relayBroadcast(line, src);
        relayBroadcast("D " + std::to_string(dst) + " " + std::to_string(src), src);
        relayBroadcast(relayState(*target));
    }
}

bool relayFrames(RelayClient &c) {
    while (c.in.size() >= 2) {
        unsigned char h0 = (unsigned char)c.in[0], h1 = (unsigned char)c.in[1];
        size_t len = h1 & 127, off = 2;
        if (len == 126) {
            if (c.in.size() < 4) break;
            len = ((unsigned char)c.in[2] << 8) | (unsigned char)c.in[3];
            off = 4;
        } else if (len == 127) {
            if (c.in.size() < 10) break;
            len = 0;
            for (int i = 0; i < 8; i++) len = (len << 8) | (unsigned char)c.in[2 + i];
            off = 10;
        }
        bool masked = h1 & 128;
        size_t start = off + (masked ? 4 : 0);
        if (c.in.size() < start + len) break;
        unsigned char mask[4] = {0,0,0,0};
        if (masked) for (int i = 0; i < 4; i++) mask[i] = (unsigned char)c.in[off + i];
        std::string payload = c.in.substr(start, len);
        c.in.erase(0, start + len);
        if (masked) for (size_t i = 0; i < payload.size(); i++)
            payload[i] = (char)(payload[i] ^ mask[i % 4]);
        int opcode = h0 & 15;
        if (opcode == 8) return false;
        if (opcode == 1) {
            std::string line;
            for (char ch : payload) if (ch) line += ch;
            size_t p = 0;
            while ((p = line.find('\n')) != std::string::npos) {
                std::string one = line.substr(0, p);
                line.erase(0, p + 1);
                if (!one.empty()) relayHandle(c.id, one);
            }
            if (!line.empty()) relayHandle(c.id, line);
        }
    }
    return true;
}

void relayDrop(int fd) {
    int id = 0;
    {
        std::lock_guard<std::recursive_mutex> lk(relayMx);
        auto it = relayPeers.begin();
        for (; it != relayPeers.end(); ++it) if (it->fd == fd) break;
        if (it == relayPeers.end()) return;
        id = it->id;
        close(fd);
        relayFree.push_back(id);
        relayPeers.erase(it);
    }
    relayBroadcast("X " + std::to_string(id));
}

void relayLoop() {
    while (relayOn) {
        fd_set rs;
        FD_ZERO(&rs);
        int maxfd = relayTcpFd > relayUdpFd ? relayTcpFd : relayUdpFd;
        FD_SET(relayTcpFd, &rs);
        FD_SET(relayUdpFd, &rs);
        std::vector<int> fds;
        {
            std::lock_guard<std::recursive_mutex> lk(relayMx);
            for (const RelayClient &c : relayPeers) {
                fds.push_back(c.fd);
                FD_SET(c.fd, &rs);
                if (c.fd > maxfd) maxfd = c.fd;
            }
        }
        timeval tv = {0, 100000};
        if (select(maxfd + 1, &rs, nullptr, nullptr, &tv) <= 0) continue;

        if (FD_ISSET(relayUdpFd, &rs)) {
            char b[64];
            sockaddr_in from = {};
            socklen_t fl = sizeof(from);
            int n = recvfrom(relayUdpFd, b, sizeof(b), 0, (sockaddr *)&from, &fl);
            if (n > 0 && std::string(b, n).rfind("CAPY?", 0) == 0) {
                std::string msg = "CAPY " + relayCode;
                sendto(relayUdpFd, msg.data(), msg.size(), 0, (sockaddr *)&from, fl);
            }
        }

        if (FD_ISSET(relayTcpFd, &rs)) {
            int fd = accept(relayTcpFd, nullptr, nullptr);
            if (fd >= 0) {
                std::lock_guard<std::recursive_mutex> lk(relayMx);
                if ((int)relayPeers.size() < 64) {
                    RelayClient c;
                    c.fd = fd;
                    c.id = relayFree.empty() ? relayNext++ : *relayFree.begin();
                    if (!relayFree.empty()) relayFree.erase(relayFree.begin());
                    relayPeers.push_back(c);
                } else close(fd);
            }
        }

        for (int fd : fds) {
            if (!FD_ISSET(fd, &rs)) continue;
            char b[4096];
            int n = recv(fd, b, sizeof(b), 0);
            if (n <= 0) { relayDrop(fd); continue; }
            bool drop = false, welcome = false;
            {
                std::lock_guard<std::recursive_mutex> lk(relayMx);
                for (RelayClient &c : relayPeers) {
                    if (c.fd != fd) continue;
                    c.in.append(b, n);
                    if (!c.ready) {
                        size_t end = c.in.find("\r\n\r\n");
                        if (end == std::string::npos) break;
                        std::string req = c.in.substr(0, end);
                        c.in.erase(0, end + 4);
                        std::istringstream hs(req);
                        std::string method, path, ver;
                        hs >> method >> path >> ver;
                        if (path != "/create" && path != "/" + relayCode) { drop = true; break; }
                        const char *ok = "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
                        sendAll(fd, ok, strlen(ok));
                        c.ready = true;
                        welcome = true;
                    } else if (!relayFrames(c)) drop = true;
                    break;
                }
            }
            if (welcome) {
                std::lock_guard<std::recursive_mutex> lk(relayMx);
                for (RelayClient &c : relayPeers) if (c.fd == fd) relayWelcome(c);
            }
            if (drop) relayDrop(fd);
        }
    }
}

void lanStop() {
    relayOn = false;
    if (relayThread.joinable()) relayThread.join();
    std::lock_guard<std::recursive_mutex> lk(relayMx);
    for (RelayClient &c : relayPeers) if (c.fd >= 0) close(c.fd);
    relayPeers.clear();
    relayFree.clear();
    relayNext = 1;
    if (relayTcpFd >= 0) { close(relayTcpFd); relayTcpFd = -1; }
    if (relayUdpFd >= 0) { close(relayUdpFd); relayUdpFd = -1; }
}

void lanStart() {
    lanStop();
    relayCode = makeRelayCode();
    memset(relayHit, 0, sizeof(relayHit));
    relayTcpFd = socket(AF_INET, SOCK_STREAM, 0);
    relayUdpFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (relayTcpFd < 0 || relayUdpFd < 0) { lanStop(); return; }
    int yes = 1;
    setsockopt(relayTcpFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(LAN_PORT);
    if (bind(relayTcpFd, (sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(relayTcpFd, 16) < 0 ||
        bind(relayUdpFd, (sockaddr *)&addr, sizeof(addr)) < 0) {
        lanStop();
        return;
    }
    relayOn = true;
    relayThread = std::thread(relayLoop);
}

std::string lanDiscover() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    sockaddr_in dst = {};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(LAN_PORT);
    dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sendto(fd, "CAPY?", 5, 0, (sockaddr *)&dst, sizeof(dst));
    ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) == 0) {
        for (ifaddrs *p = ifap; p; p = p->ifa_next) {
            if (!p->ifa_broadaddr || p->ifa_broadaddr->sa_family != AF_INET) continue;
            in_addr a = ((sockaddr_in *)p->ifa_broadaddr)->sin_addr;
            if (a.s_addr == INADDR_ANY) continue;
            dst.sin_addr = a;
            sendto(fd, "CAPY?", 5, 0, (sockaddr *)&dst, sizeof(dst));
        }
        freeifaddrs(ifap);
    }
    timeval tv = {1, 200000};
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(fd, &rs);
    if (select(fd + 1, &rs, nullptr, nullptr, &tv) > 0) {
        char b[64];
        sockaddr_in from = {};
        socklen_t fl = sizeof(from);
        int n = recvfrom(fd, b, sizeof(b), 0, (sockaddr *)&from, &fl);
        if (n > 0) {
            std::string msg(b, n);
            if (msg.rfind("CAPY ", 0) == 0) {
                std::string code = msg.substr(5, 6);
                if (code.size() == 6) {
                    close(fd);
                    return std::string(inet_ntoa(from.sin_addr)) + " " + code;
                }
            }
        }
    }
    close(fd);
    return "";
}

static int netRead(char *b, int n) { return netSock >= 0 ? recv(netSock, b, n, 0) : -1; }
static bool netFill(size_t n) {
    while (netIn.size() < n) {
        char b[4096];
        int got = netRead(b, sizeof(b));
        if (got > 0) { netIn.append(b, got); continue; }
        return false;
    }
    return true;
}

void netClose();

void netConnect(const char *host, const char *code) {
    netClose();
    netBuf.clear(); netReady = false; myId = 0; netT = 0;
    roomCode[0] = 0;
    for (NetPlayer &p : netPlayers) p.alive = false;

    std::string h = host && *host ? host : "127.0.0.1";
    netSock = socket(AF_INET, SOCK_STREAM, 0);
    if (netSock < 0) return;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LAN_PORT);
    if (!inet_pton(AF_INET, h.c_str(), &addr.sin_addr)) {
        hostent *he = gethostbyname(h.c_str());
        if (!he) { netClose(); return; }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    if (connect(netSock, (sockaddr *)&addr, sizeof(addr)) != 0) { netClose(); return; }

    std::string req = std::string("GET /") + code + " HTTP/1.1\r\nHost: " + h +
        "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    if (!sendAll(netSock, req.data(), req.size())) { netClose(); return; }
    size_t end;
    while ((end = netIn.find("\r\n\r\n")) == std::string::npos)
        if (!netFill(netIn.size() + 1)) { netClose(); return; }
    if (netIn.substr(0, end).find("101") == std::string::npos) { netClose(); return; }
    netIn.erase(0, end + 4);

    int flags = fcntl(netSock, F_GETFL, 0);
    fcntl(netSock, F_SETFL, flags | O_NONBLOCK);
    netReady = true;
}

void netSend(const char *s) {
    if (!netReady) return;
    std::string msg(s), frame;
    size_t n = msg.size();
    frame += (char)0x81;
    if (n < 126) frame += (char)(0x80 | n);
    else if (n < 65536) {
        frame += (char)(0x80 | 126);
        frame += (char)(n >> 8);
        frame += (char)(n & 255);
    } else return;
    unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
    for (int i = 0; i < 4; i++) frame += (char)mask[i];
    for (size_t i = 0; i < n; i++) frame += (char)(msg[i] ^ mask[i % 4]);
    sendAll(netSock, frame.data(), frame.size());
}

static void netPump() {
    while (netReady) {
        if (!netFill(2)) break;
        unsigned char h0 = (unsigned char)netIn[0], h1 = (unsigned char)netIn[1];
        size_t len = h1 & 127, off = 2;
        if (len == 126) {
            if (!netFill(4)) break;
            len = ((unsigned char)netIn[2] << 8) | (unsigned char)netIn[3];
            off = 4;
        } else if (len == 127) {
            if (!netFill(10)) break;
            len = 0;
            for (int i = 0; i < 8; i++) len = (len << 8) | (unsigned char)netIn[2 + i];
            off = 10;
        }
        bool masked = h1 & 128;
        size_t start = off + (masked ? 4 : 0);
        if (!netFill(start + len)) break;
        unsigned char key[4] = {0,0,0,0};
        if (masked) for (int i = 0; i < 4; i++) key[i] = (unsigned char)netIn[off + i];
        std::string payload = netIn.substr(start, len);
        netIn.erase(0, start + len);
        if (masked) for (size_t i = 0; i < payload.size(); i++)
            payload[i] = (char)(payload[i] ^ key[i % 4]);
        int opcode = h0 & 15;
        if (opcode == 1) {
            for (char c : payload) if (c) netBuf += c;
        } else if (opcode == 8) netReady = false;
    }
}

void netClose() {
#ifdef PLATFORM_WEB
    if (ws > 0) emscripten_websocket_close(ws, 1000, "bye");
    ws = 0;
#else
    if (netSock >= 0) { close(netSock); netSock = -1; }
    netIn.clear();
#endif
    netReady = false;
}
#endif

void remoteShot(int id, Vector3 p, Vector3 v) {
    for (Bullet &b : bullets) if (!b.alive) {
        b = {p, v, 7, 0, id, true, false};
        return;
    }
}

void netLine(std::string s) {
    std::istringstream in(s);
    char tag = 0;
    in >> tag;
    if (tag == 'W') {
        in >> myId >> roomCode;
        float a = (myId - 1) * 2.39996f;
        me.pos = {cosf(a) * 5, 0, 9 + sinf(a) * 5};
    } else if (tag == 'E') {
        hurtMsg = "room gone - make another";
        hurtMsgT = 2.0f;
        netClose();
        screen = S_MENU;
    } else if (tag == 'P') {
        int id, hp; char nm[16]; float x, y, z, yaw;
        in >> id >> nm >> x >> y >> z >> yaw >> hp;
        if (id < 0 || id >= 64) return;
        NetPlayer &p = netPlayers[id];
        p.id = id; p.hp = hp; p.pos = {x,y,z}; p.yaw = yaw; p.alive = true;
        snprintf(p.name, sizeof(p.name), "%s", nm);
        if (id == myId) me.hp = hp;
    } else if (tag == 'X') {
        int id; in >> id;
        if (id >= 0 && id < 64) netPlayers[id].alive = false;
    } else if (tag == 'F') {
        int id; float x, y, z, dx, dy, dz;
        in >> id >> x >> y >> z >> dx >> dy >> dz;
        remoteShot(id, {x,y,z}, Vector3Scale(Vector3Normalize({dx,dy,dz}), 36));
    } else if (tag == 'D') {
        int id; in >> id;
        if (id == myId) { hurtMsg = "capybara got hit"; hurtMsgT = 1.0f; shake = 0.2f; }
    }
}

void netUpdate(float dt) {
#ifndef PLATFORM_WEB
    netPump();
#endif
    size_t nl;
    while ((nl = netBuf.find('\n')) != std::string::npos) {
        std::string line = netBuf.substr(0, nl);
        netBuf.erase(0, nl + 1);
        if (!line.empty()) netLine(line);
    }
    netT -= dt;
    if (netReady && myId && netT <= 0) {
        netT = 0.08f;
        char b[160];
        snprintf(b, sizeof(b), "S %d %s %.2f %.2f %.2f %.2f %d\n", myId, pname,
                 me.pos.x, me.pos.y, me.pos.z, me.yaw, me.hp);
        netSend(b);
    }
}

// 1, 2, 4, 6 then keep 6
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
    if (mode == 1) return;      // online has real players
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
                 Vector3Scale(d, 36), 4, 0, myId, true, true};
            me.ammo--; me.fireT = 0.14f;
            shake = 0.08f;
            sfx(sShoot, 0.6f);
            if (mode == 1) {
                char msg[128];
                snprintf(msg, sizeof(msg), "F %d %.2f %.2f %.2f %.2f %.2f %.2f\n", myId,
                         b.pos.x, b.pos.y, b.pos.z, d.x, d.y, d.z);
                netSend(msg);
            }
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
                 7, 0, -2, true, false};
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
            if (mode == 1 && b.owner == myId) for (NetPlayer &p : netPlayers)
                if (b.alive && p.alive && p.id != myId && p.hp > 0 &&
                    Vector3DistanceSqr(b.pos, (Vector3){p.pos.x, p.pos.y + 1, p.pos.z}) < 0.8f * 0.8f) {
                    b.alive = false;
                    char msg[32];
                    snprintf(msg, sizeof(msg), "H %d %d\n", myId, p.id);
                    netSend(msg);
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
                    if (mode == 1 && b.owner >= 0) {
                        char msg[32];
                        snprintf(msg, sizeof(msg), "H %d %d\n", myId, b.owner);
                        netSend(msg);
                    }
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
    Rectangle cs = {0, 0, (float)charTex.width, (float)charTex.height};
    for (const NetPlayer &p : netPlayers) if (p.alive && p.id != myId && p.hp > 0) {
        Vector3 at = {p.pos.x, p.pos.y + 0.98f, p.pos.z};
        DrawBillboardPro(cam, charTex, cs, at, (Vector3){0,1,0},
                         (Vector2){1.25f, 1.875f}, (Vector2){0.5f,0.5f}, 0, WHITE);
        Vector3 gd = {sinf(p.yaw), 0, -cosf(p.yaw)};
        Vector3 gp = {p.pos.x + gd.x * 0.72f, p.pos.y + 1.18f, p.pos.z + gd.z * 0.72f};
        DrawBillboardPro(cam, bulletTex, bs, gp, (Vector3){0,1,0},
                         (Vector2){0.62f, 0.62f}, (Vector2){0.5f,0.5f}, 0, WHITE);
    }
    for (const Bullet &b : bullets) {
        if (!b.alive) continue;
        bool mean = mode == 0 && !b.mine;
        Color trail = mean ? RED : (Color){210, 170, 110, 255};
        Color tint = mean ? RED : WHITE;
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
    for (const NetPlayer &p : netPlayers) if (p.alive && p.id != myId && p.hp > 0)
        tag((Vector3){p.pos.x, p.pos.y + 2.25f, p.pos.z},
            TextFormat("%s %d", p.name, p.hp), 16, RAYWHITE);
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
    for (const NetPlayer &p : netPlayers) if (p.alive && p.id != myId && p.hp > 0)
        dot(p.pos.x, p.pos.z, (Color){230, 140, 215, 255}, 4);
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
    } else if (screen == S_ROOM) {
#ifdef PLATFORM_WEB
        DrawText("lan play needs the native build", 84, 250, 32, RAYWHITE);
        DrawText("the web build is solo only", 84, 310, 22, LIGHTGRAY);
        DrawText("enter = back", 84, 370, 20, LIGHTGRAY);
#else
        DrawText("lan capybara ffa", 84, 250, 36, RAYWHITE);
        DrawText("enter = find a game or start one", 84, 320, 26, RAYWHITE);
        DrawText("first player hosts. others join same wifi.", 84, 370, 20, LIGHTGRAY);
        DrawText("no setup, no ip, no extra files", 84, 420, 18, LIGHTGRAY);
        DrawText("esc = back", 84, 470, 20, LIGHTGRAY);
#endif
    } else {
        DrawText(TextFormat("name: %s", pname), 84, 250, 28, RAYWHITE);
        DrawText("1  bot mode   (any name)", 84, 320, 26, RAYWHITE);
        DrawText("2  lan mode (same wifi, first player hosts)", 84, 360, 24, RAYWHITE);
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
    if (FileExists("assets/bgm.mp3")) {
        bgm = LoadMusicStream("assets/bgm.mp3");
        bgm.looping = true;
        SetMusicVolume(bgm, 0.12f);
        PlayMusicStream(bgm);
        haveBgm = true;
    }
    pointsTex = LoadTexture("assets/points.png");
    bulletTex = LoadTexture("assets/capybara_bullet.png");
    makeCharTex();
    snakeModel = LoadModel("assets/snake/Snake.obj");
    SetTargetFPS(120);
    SetExitKey(KEY_NULL);
    reset();

    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.05f);
        t += dt;
        if (haveBgm) UpdateMusicStream(bgm);

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
            if (IsKeyPressed(KEY_TWO) && !nameTaken) {
                mode = 1; roomStarted = false; screen = S_ROOM;
            }
            if (IsKeyPressed(KEY_BACKSPACE)) screen = S_NAME;
        } else if (screen == S_DIFF) {
            int keys[] = {KEY_ONE, KEY_TWO, KEY_THREE};
            for (int i = 0; i < 3; i++)
                if (IsKeyPressed(keys[i])) { difficulty = i; reset(); screen = S_GAME; }
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) screen = S_MENU;
        } else if (screen == S_ROOM) {
#ifdef PLATFORM_WEB
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) screen = S_MENU;
#else
            if (!roomStarted && IsKeyPressed(KEY_ENTER)) {
                roomStarted = true;
                std::string found = lanDiscover();
                reset();
                if (!found.empty()) {
                    std::istringstream fi(found);
                    std::string ip, code;
                    fi >> ip >> code;
                    netConnect(ip.c_str(), code.c_str());
                } else {
                    lanStart();
                    netConnect("127.0.0.1", relayCode.c_str());
                }
                screen = S_GAME;
            }
            if (IsKeyPressed(KEY_ESCAPE)) screen = S_MENU;
#endif
        } else {
            if (mode == 1) netUpdate(dt);
            if (IsKeyPressed(KEY_ESCAPE)) {
                if (mode == 1) netClose();
#ifndef PLATFORM_WEB
                if (mode == 1) lanStop();
#endif
                screen = S_MENU; dead = false;
            }
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
            DrawText(TextFormat("%s  |  %s  |  ammo %d", pname,
                                mode ? "LAN" : diffName[difficulty], me.ammo),
                     20, 20, 20, RAYWHITE);
            DrawText(TextFormat("HP %d", me.hp), 20, 50, 20, LIGHTGRAY);
            if (mode == 0)
                DrawText(TextFormat("round %d", wave), 20, 75, 20, LIGHTGRAY);
            else {
                int peers = 0;
                for (const NetPlayer &p : netPlayers) if (p.alive && p.id != myId) peers++;
                const char *st = !netReady ? "connecting..." :
                    (!myId ? "joining room..." :
                     roomCode[0] ? TextFormat("room %s | id %d | %d other capybara(s)",
                                              roomCode, myId, peers) : "making room...");
                DrawText(st, 20, 75, 16, LIGHTGRAY);
            }
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
    if (haveBgm) UnloadMusicStream(bgm);
    UnloadSound(sHit); UnloadSound(sKill); UnloadSound(sJump);
    UnloadSound(sShoot); UnloadSound(sReload); UnloadSound(sDash);
    UnloadModel(snakeModel);
    UnloadTexture(pointsTex); UnloadTexture(bulletTex); UnloadTexture(charTex);
    netClose();
#ifndef PLATFORM_WEB
    lanStop();
#endif
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
