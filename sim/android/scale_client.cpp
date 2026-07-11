// HTTP client for the PandaScale. A background thread polls /weight and /info
// from the scale and runs queued commands (tare/cal/setwifi/setip). All network
// I/O is here, off the LVGL/main thread; the Scale UI just reads cached values.
#include "scale_client.h"

#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

// --- cached scale state (mutex-protected) ---
static SDL_mutex* g_mtx = nullptr;
static float g_grams = 0;
static bool  g_stable = false;
static uint32_t g_last_ok = 0;          // SDL_GetTicks of last good reply
static char  g_info[256] = "";
static char  g_msg[128] = "";
static bool  g_polling = false;

// --- command queue (paths to GET) ---
#define SCQ 8
static char g_q[SCQ][128];
static int  g_qh = 0, g_qt = 0;

// Blocking HTTP GET with a short timeout. Copies the response body into out.
// Returns true on a 2xx-ish reply. Runs only on the background thread.
static bool http_get(const char* host, int port, const char* path, char* out, int outn) {
    if (out && outn) out[0] = 0;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) {   // numeric IP expected
        close(fd);
        return false;
    }
    if (connect(fd, (sockaddr*)&a, sizeof(a)) != 0) { close(fd); return false; }

    char req[256];
    int rn = snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    if (send(fd, req, rn, 0) != rn) { close(fd); return false; }

    char resp[1024];
    int total = 0, r;
    while ((r = (int)recv(fd, resp + total, sizeof(resp) - 1 - total, 0)) > 0) {
        total += r;
        if (total >= (int)sizeof(resp) - 1) break;
    }
    close(fd);
    if (total <= 0) return false;
    resp[total] = 0;

    bool ok = (strncmp(resp, "HTTP/1.", 7) == 0) && (strstr(resp, " 200 ") != nullptr);
    char* body = strstr(resp, "\r\n\r\n");
    body = body ? body + 4 : resp;
    if (out && outn) { strncpy(out, body, outn - 1); out[outn - 1] = 0; }
    return ok;
}

static void poll_once() {
    char host[40];
    SDL_LockMutex(g_mtx);
    strncpy(host, g_scale_ip, sizeof(host) - 1); host[sizeof(host) - 1] = 0;
    SDL_UnlockMutex(g_mtx);
    if (!host[0]) return;

    char body[256];
    if (http_get(host, 80, "/weight", body, sizeof(body))) {
        float g = 0; bool st = false;
        const char* p = strstr(body, "\"g\":");
        if (p) g = atof(p + 4);
        st = (strstr(body, "\"stable\":true") != nullptr);
        SDL_LockMutex(g_mtx);
        g_grams = g; g_stable = st; g_last_ok = SDL_GetTicks();
        SDL_UnlockMutex(g_mtx);
    }
    if (http_get(host, 80, "/info", body, sizeof(body))) {
        SDL_LockMutex(g_mtx);
        strncpy(g_info, body, sizeof(g_info) - 1); g_info[sizeof(g_info) - 1] = 0;
        SDL_UnlockMutex(g_mtx);
    }
}

static bool pop_cmd(char* out, int n) {
    bool has = false;
    SDL_LockMutex(g_mtx);
    if (g_qh != g_qt) { strncpy(out, g_q[g_qh], n - 1); out[n - 1] = 0; g_qh = (g_qh + 1) % SCQ; has = true; }
    SDL_UnlockMutex(g_mtx);
    return has;
}

static int worker(void*) {
    uint32_t last_info = 0;
    for (;;) {
        // Run any queued commands first (tare/cal/setwifi/setip).
        char path[128];
        while (pop_cmd(path, sizeof(path))) {
            char host[40], body[160];
            SDL_LockMutex(g_mtx);
            strncpy(host, g_scale_ip, sizeof(host) - 1); host[sizeof(host) - 1] = 0;
            SDL_UnlockMutex(g_mtx);
            bool ok = http_get(host, 80, path, body, sizeof(body));
            SDL_LockMutex(g_mtx);
            if (ok && body[0]) { strncpy(g_msg, body, sizeof(g_msg) - 1); g_msg[sizeof(g_msg) - 1] = 0; }
            else strncpy(g_msg, "geen verbinding met schaal", sizeof(g_msg) - 1);
            SDL_UnlockMutex(g_mtx);
        }

        bool poll;
        SDL_LockMutex(g_mtx);
        poll = g_polling;
        SDL_UnlockMutex(g_mtx);
        if (poll) poll_once();

        SDL_Delay(poll ? 700 : 250);
    }
    return 0;
}

// --- public API ---
void scale_client_start() {
    if (g_mtx) return;
    g_mtx = SDL_CreateMutex();
    SDL_Thread* th = SDL_CreateThread(worker, "scale", nullptr);
    if (th) SDL_DetachThread(th);
}

void scale_set_polling(bool on) {
    if (!g_mtx) return;
    SDL_LockMutex(g_mtx);
    g_polling = on;
    SDL_UnlockMutex(g_mtx);
}

float scale_grams() { SDL_LockMutex(g_mtx); float g = g_grams; SDL_UnlockMutex(g_mtx); return g; }
bool  scale_stable() { SDL_LockMutex(g_mtx); bool s = g_stable; SDL_UnlockMutex(g_mtx); return s; }

bool scale_online() {
    if (!g_mtx) return false;
    SDL_LockMutex(g_mtx);
    bool on = g_last_ok && (SDL_GetTicks() - g_last_ok < 4000);
    SDL_UnlockMutex(g_mtx);
    return on;
}

void scale_info(char* out, int n) {
    if (!g_mtx) { if (out && n) out[0] = 0; return; }
    SDL_LockMutex(g_mtx);
    strncpy(out, g_info, n - 1); out[n - 1] = 0;
    SDL_UnlockMutex(g_mtx);
}

void scale_last_msg(char* out, int n) {
    if (!g_mtx) { if (out && n) out[0] = 0; return; }
    SDL_LockMutex(g_mtx);
    strncpy(out, g_msg, n - 1); out[n - 1] = 0;
    SDL_UnlockMutex(g_mtx);
}

void scale_cmd(const char* path) {
    if (!g_mtx || !path) return;
    SDL_LockMutex(g_mtx);
    int nt = (g_qt + 1) % SCQ;
    if (nt != g_qh) { strncpy(g_q[g_qt], path, sizeof(g_q[0]) - 1); g_q[g_qt][sizeof(g_q[0]) - 1] = 0; g_qt = nt; }
    SDL_UnlockMutex(g_mtx);
}
