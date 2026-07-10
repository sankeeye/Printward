#include "bambu_ftp.h"
#include "storage.h"
#ifdef __ANDROID__
#include <stdlib.h>   // arc4random_buf (bionic, API 21+)
#else
#include <esp_random.h>
#include <esp_heap_caps.h>
#endif
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/platform.h>

// Bambu printers expose their SD card over implicit FTPS on port 990 (see
// ftp.md in github.com/Doridian/OpenBambuAPI). The tricky part isn't the FTP
// protocol - it's that the printer's server requires the *data* connection to
// RESUME the TLS session negotiated on the *control* connection (a common
// anti-"data-connection-stealing" measure). Arduino's WiFiClientSecure exposes
// no session-reuse API, and the convenient esp_tls session helpers are compiled
// out of the precompiled arduino-esp32 libs, so this talks raw mbedTLS: we keep
// the control session with mbedtls_ssl_get_session() and inject it into the
// data connection with mbedtls_ssl_set_session() before its handshake.

// -----------------------------------------------------------------------------
//  Raw mbedTLS connection helper (used for both control and data channels)
// -----------------------------------------------------------------------------

// RNG for the TLS stack: ESP32's hardware RNG is a valid entropy source while
// WiFi/BT is active, so we can skip the entropy/CTR-DRBG boilerplate.
static int ftp_rng(void* ctx, unsigned char* buf, size_t len) {
    (void)ctx;
#ifdef __ANDROID__
    arc4random_buf(buf, len);   // bionic CSPRNG - no seeding needed
#else
    esp_fill_random(buf, len);
#endif
    return 0;
}

// This build compiles mbedTLS with CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC, so TLS
// buffers come from internal RAM only - and two 16KB-content contexts at once
// (control + data, both open during a transfer) don't fit next to WiFi and the
// RGB bounce buffers. Point mbedTLS at PSRAM instead (8MB, and TLS buffers need
// no DMA/internal RAM). Installed once, globally: harmless for the other TLS
// users (MQTT/Cloud), which just get their buffers from PSRAM too.
#ifdef __ANDROID__
// The tablet has plenty of heap; keep mbedTLS on the default allocator.
static void ftp_use_psram_tls() {}
#else
static void* ftp_tls_calloc(size_t n, size_t size) {
    return heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM);
}
static void ftp_tls_free(void* p) {
    heap_caps_free(p);
}
static void ftp_use_psram_tls() {
    static bool done = false;
    if (!done) {
        mbedtls_platform_set_calloc_free(ftp_tls_calloc, ftp_tls_free);
        done = true;
    }
}
#endif

struct FtpConn {
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    bool up = false;
};

// Opens a TCP connection to host:port and completes a TLS handshake. When
// `resume` is non-null its session is installed first, so the server resumes
// it instead of doing a full handshake (the data-channel requirement). Cert
// verification is off - same trust trade-off as the LAN MQTT/Cloud links.
static bool ftpc_connect(FtpConn& c, const char* host, int port,
                         const mbedtls_ssl_session* resume, String* err) {
    mbedtls_net_init(&c.net);
    mbedtls_ssl_init(&c.ssl);
    mbedtls_ssl_config_init(&c.conf);

    char port_s[8];
    snprintf(port_s, sizeof(port_s), "%d", port);
    int ret = mbedtls_net_connect(&c.net, host, port_s, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) { if (err) *err = "tcp " + String(ret); return false; }

    if (mbedtls_ssl_config_defaults(&c.conf, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        if (err) *err = "cfg"; return false;
    }
    mbedtls_ssl_conf_authmode(&c.conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&c.conf, ftp_rng, nullptr);
    mbedtls_ssl_conf_session_tickets(&c.conf, MBEDTLS_SSL_SESSION_TICKETS_ENABLED);
    // Pin TLS 1.2 so resumption uses the classic ticket/session-id flow that
    // this printer's (old mbedTLS-based) server and FileZilla negotiate.
    mbedtls_ssl_conf_min_tls_version(&c.conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&c.conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_read_timeout(&c.conf, 8000);

    ret = mbedtls_ssl_setup(&c.ssl, &c.conf);
    if (ret != 0) { if (err) *err = "setup " + String(ret); return false; }
    if (resume) {
        ret = mbedtls_ssl_set_session(&c.ssl, resume);
        if (ret != 0) { if (err) *err = "set_session " + String(ret); return false; }
    }
    mbedtls_ssl_set_bio(&c.ssl, &c.net, mbedtls_net_send, nullptr, mbedtls_net_recv_timeout);

    unsigned long start = millis();
    while ((ret = mbedtls_ssl_handshake(&c.ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            if (err) *err = "hs " + String(ret);
            return false;
        }
        if (millis() - start > 10000) { if (err) *err = "hs timeout"; return false; }
        delay(5);
    }
    c.up = true;
    return true;
}

static int ftpc_write(FtpConn& c, const uint8_t* buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int ret = mbedtls_ssl_write(&c.ssl, buf + sent, len - sent);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret <= 0) return ret;
        sent += ret;
    }
    return (int)sent;
}

// Returns >0 bytes read, 0 on "nothing yet"/timeout, <0 on close or error.
static int ftpc_read(FtpConn& c, uint8_t* buf, size_t len) {
    int ret = mbedtls_ssl_read(&c.ssl, buf, len);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
        ret == MBEDTLS_ERR_SSL_TIMEOUT) return 0;
    if (ret <= 0) return -1; // PEER_CLOSE_NOTIFY, reset, or hard error
    return ret;
}

static void ftpc_close(FtpConn& c) {
    if (c.up) mbedtls_ssl_close_notify(&c.ssl);
    mbedtls_ssl_free(&c.ssl);
    mbedtls_ssl_config_free(&c.conf);
    mbedtls_net_free(&c.net);
    c.up = false;
}

// -----------------------------------------------------------------------------
//  FTP reply reading / command sending over an FtpConn
// -----------------------------------------------------------------------------

// Reads one FTP reply, handling multi-line replies (RFC 959: continuation
// lines look like "150-...", the reply ends with "150 " - the same code then a
// space). Returns the numeric code, or -1 on timeout/disconnect.
static int ftp_read_reply(FtpConn& c, String* full_out, unsigned long timeout_ms = 8000) {
    String all, line;
    unsigned long start = millis();
    while (millis() - start < timeout_ms) {
        uint8_t ch;
        int n = ftpc_read(c, &ch, 1);
        if (n == 1) {
            line += (char)ch;
            start = millis();
            if (ch == '\n') {
                all += line;
                bool final_line = line.length() >= 4 &&
                    isDigit(line[0]) && isDigit(line[1]) && isDigit(line[2]) && line[3] == ' ';
                if (final_line) { if (full_out) *full_out = all; return line.substring(0, 3).toInt(); }
                line = "";
            }
        } else if (n < 0) {
            break;
        } else {
            delay(2);
        }
    }
    if (full_out) *full_out = all;
    return -1;
}

static int ftp_send_cmd(FtpConn& c, const String& cmd, String* full_out = nullptr) {
    String line = cmd + "\r\n"; // one write -> one TLS record (this printer's
    ftpc_write(c, (const uint8_t*)line.c_str(), line.length()); // server mis-parses split records)
    return ftp_read_reply(c, full_out);
}

// -----------------------------------------------------------------------------
//  Reply/listing parsing (transport-independent)
// -----------------------------------------------------------------------------

// Parses EPSV's reply (RFC 2428): "229 ... (|||6446|)" - delimiter is the char
// after '(', the port sits between the last two delimiters. Returns -1 if the
// reply doesn't look like a valid EPSV response.
static int parse_epsv_port(const String& resp) {
    int paren_start = resp.indexOf('(');
    int paren_end = resp.indexOf(')', paren_start);
    if (paren_start < 0 || paren_end < 0 || paren_end <= paren_start + 1) return -1;
    String inner = resp.substring(paren_start + 1, paren_end);
    if (inner.length() < 2) return -1;
    char delim = inner[0];
    int last = inner.lastIndexOf(delim);
    if (last <= 0) return -1;
    int prev = inner.lastIndexOf(delim, last - 1);
    if (prev < 0) return -1;
    String port_str = inner.substring(prev + 1, last);
    if (port_str.length() == 0) return -1;
    return port_str.toInt();
}

// Parses Unix `ls -l` lines: 8 whitespace-separated fields then the name (which
// may contain spaces, so it's "everything after field 8"). Field 5 is the size,
// a leading 'd' marks a directory.
static int parse_listing(const String& listing, FtpEntry* out, int max_entries) {
    int count = 0, line_start = 0, len = listing.length();
    while (line_start < len && count < max_entries) {
        int nl = listing.indexOf('\n', line_start);
        if (nl < 0) nl = len;
        String line = listing.substring(line_start, nl);
        line.trim();
        line_start = nl + 1;
        if (line.length() == 0) continue;

        bool is_dir = (line[0] == 'd');
        int i = 0, n = line.length(), fields_seen = 0;
        uint32_t size = 0;
        while (i < n && fields_seen < 8) {
            while (i < n && line[i] == ' ') i++;
            int field_start = i;
            while (i < n && line[i] != ' ') i++;
            if (i > field_start) {
                fields_seen++;
                if (fields_seen == 5) size = (uint32_t)line.substring(field_start, i).toInt();
            }
        }
        while (i < n && line[i] == ' ') i++;
        String name = line.substring(i);
        name.trim();
        if (name.length() == 0 || name == "." || name == "..") continue;

        out[count].name = name;
        out[count].is_dir = is_dir;
        out[count].size = size;
        count++;
    }
    return count;
}

// -----------------------------------------------------------------------------
//  Directory listing
// -----------------------------------------------------------------------------

bool bambu_ftp_list(const char* path, FtpEntry* out, int max_entries, int* out_count, String* err) {
    *out_count = 0;
    if (strlen(g_printer_ip) == 0) { *err = "No printer IP configured yet"; return false; }

    ftp_use_psram_tls(); // keep the two TLS contexts out of scarce internal RAM

    // Compact reply-code trail, appended to *err on any failure.
    String log;

    FtpConn ctrl;
    String cerr;
    if (!ftpc_connect(ctrl, g_printer_ip, 990, nullptr, &cerr)) {
        *err = "Couldn't establish the FTPS control connection (" + cerr + ")";
        ftpc_close(ctrl);
        return false;
    }

    // Save the control channel's TLS session for the data connection to resume.
    mbedtls_ssl_session sess;
    mbedtls_ssl_session_init(&sess);
    bool have_sess = (mbedtls_ssl_get_session(&ctrl.ssl, &sess) == 0);
    log += have_sess ? "SESS=ok " : "SESS=fail ";

    // Cleans up ctrl + session and returns false with the message in *err.
    auto fail = [&](const String& msg) -> bool {
        *err = msg + " [" + log + "]";
        ftpc_close(ctrl);
        mbedtls_ssl_session_free(&sess);
        return false;
    };

    String resp;
    int code;

    code = ftp_read_reply(ctrl, &resp);
    log += "GREET=" + String(code) + " ";
    if (code < 200 || code >= 300) return fail("No FTP greeting: " + resp);

    code = ftp_send_cmd(ctrl, "USER bblp", &resp);
    log += "USER=" + String(code) + " ";
    if (code == 331) {
        code = ftp_send_cmd(ctrl, String("PASS ") + g_printer_access_code, &resp);
        log += "PASS=" + String(code) + " ";
    }
    if (code != 230) return fail("FTP login failed (need clean PASS=230): " + resp);

    // Mirror FileZilla's post-login ritual. Only the data-channel steps below
    // are treated as fatal; these are logged but informational.
    ftp_send_cmd(ctrl, "SYST", &resp);
    code = ftp_send_cmd(ctrl, "PBSZ 0", &resp); log += "PBSZ=" + String(code) + " ";
    code = ftp_send_cmd(ctrl, "PROT P", &resp); log += "PROT=" + String(code) + " ";
    ftp_send_cmd(ctrl, "PWD", &resp);
    code = ftp_send_cmd(ctrl, "TYPE I", &resp); log += "TYPE=" + String(code) + " ";

    // CWD only into a real subfolder - the working trace never sends "CWD /".
    if (path && path[0] != '\0' && !(path[0] == '/' && path[1] == '\0')) {
        code = ftp_send_cmd(ctrl, String("CWD ") + path, &resp);
        log += "CWD=" + String(code) + " ";
        if (code < 200 || code >= 300) return fail("Couldn't open folder \"" + String(path) + "\": " + resp);
    }

    // Passive mode: PASV (classic) then EPSV as a fallback.
    char data_ip[16];
    strncpy(data_ip, g_printer_ip, sizeof(data_ip) - 1);
    data_ip[sizeof(data_ip) - 1] = '\0';
    int data_port = -1;

    code = ftp_send_cmd(ctrl, "PASV", &resp);
    log += "PASV=" + String(code) + " ";
    if (code == 227) {
        int p1 = resp.indexOf('('), p2 = resp.indexOf(')', p1);
        if (p1 >= 0 && p2 >= 0) {
            String nums = resp.substring(p1 + 1, p2);
            int parts[6] = {0}, idx = 0, val = 0;
            for (size_t i = 0; i < nums.length() && idx < 6; i++) {
                char ch = nums[i];
                if (ch == ',') { parts[idx++] = val; val = 0; }
                else if (isDigit(ch)) val = val * 10 + (ch - '0');
            }
            if (idx == 5) parts[5] = val;
            snprintf(data_ip, sizeof(data_ip), "%d.%d.%d.%d", parts[0], parts[1], parts[2], parts[3]);
            data_port = parts[4] * 256 + parts[5];
        }
    } else {
        code = ftp_send_cmd(ctrl, "EPSV", &resp);
        log += "EPSV=" + String(code) + " ";
        if (code == 229) data_port = parse_epsv_port(resp);
    }
    if (data_port < 0) return fail("Printer's FTP gave no usable passive data port");

    // Some minimal daemons implement only one of LIST/NLST - try LIST, fall
    // back to NLST if LIST itself is "not implemented" (502).
    static const char* kListCmds[2] = {"LIST", "NLST"};
    for (int i = 0; i < 2; i++) {
        // Data channel resumes the control channel's TLS session (the whole
        // point of the raw-mbedTLS approach).
        FtpConn data;
        String derr;
        if (!ftpc_connect(data, data_ip, data_port, have_sess ? &sess : nullptr, &derr)) {
            ftpc_close(data);
            return fail("Couldn't open the FTP data connection to " + String(data_ip) + ":" + String(data_port) + " (" + derr + ")");
        }

        code = ftp_send_cmd(ctrl, kListCmds[i], &resp);
        log += String(kListCmds[i]) + "=" + String(code) + " ";
        if (code != 150 && code != 125) {
            ftpc_close(data);
            if (i == 0 && code == 502) continue; // try NLST
            return fail(String(kListCmds[i]) + " failed: " + resp);
        }

        String listing;
        uint8_t buf[512];
        unsigned long start = millis();
        while (millis() - start < 8000) {
            int n = ftpc_read(data, buf, sizeof(buf));
            if (n > 0) { listing.concat((const char*)buf, (unsigned int)n); start = millis(); }
            else if (n < 0) break;
            else delay(2);
        }
        ftpc_close(data);

        ftp_read_reply(ctrl, &resp);      // final "226 Transfer complete"
        ftp_send_cmd(ctrl, "QUIT", &resp);
        ftpc_close(ctrl);
        mbedtls_ssl_session_free(&sess);

        *out_count = parse_listing(listing, out, max_entries);
        return true; // 0 entries just means an empty directory
    }

    return fail("Printer's FTP server doesn't support LIST or NLST");
}

// -----------------------------------------------------------------------------
//  File download (RETR) - streams the bytes to a callback
// -----------------------------------------------------------------------------

// Parses a PASV/EPSV reply into data_ip/data_port; returns the port or -1.
static int ftp_open_passive(FtpConn& ctrl, const char* printer_ip, char* data_ip,
                            size_t data_ip_sz, String& log) {
    String resp;
    strncpy(data_ip, printer_ip, data_ip_sz - 1);
    data_ip[data_ip_sz - 1] = '\0';
    int data_port = -1;
    int code = ftp_send_cmd(ctrl, "PASV", &resp);
    log += "PASV=" + String(code) + " ";
    if (code == 227) {
        int p1 = resp.indexOf('('), p2 = resp.indexOf(')', p1);
        if (p1 >= 0 && p2 >= 0) {
            String nums = resp.substring(p1 + 1, p2);
            int parts[6] = {0}, idx = 0, val = 0;
            for (size_t i = 0; i < nums.length() && idx < 6; i++) {
                char ch = nums[i];
                if (ch == ',') { parts[idx++] = val; val = 0; }
                else if (isDigit(ch)) val = val * 10 + (ch - '0');
            }
            if (idx == 5) parts[5] = val;
            snprintf(data_ip, data_ip_sz, "%d.%d.%d.%d", parts[0], parts[1], parts[2], parts[3]);
            data_port = parts[4] * 256 + parts[5];
        }
    } else {
        code = ftp_send_cmd(ctrl, "EPSV", &resp);
        log += "EPSV=" + String(code) + " ";
        if (code == 229) data_port = parse_epsv_port(resp);
    }
    return data_port;
}

bool bambu_ftp_download(const char* full_path, ftp_download_cb cb, void* ctx,
                        uint32_t* out_bytes, String* err) {
    if (out_bytes) *out_bytes = 0;
    if (strlen(g_printer_ip) == 0) { *err = "No printer IP configured yet"; return false; }
    ftp_use_psram_tls();

    String fp = full_path ? String(full_path) : String("");
    int slash = fp.lastIndexOf('/');
    String dir  = (slash > 0) ? fp.substring(0, slash) : String("");
    String name = (slash >= 0) ? fp.substring(slash + 1) : fp;

    String log;
    FtpConn ctrl; String cerr;
    if (!ftpc_connect(ctrl, g_printer_ip, 990, nullptr, &cerr)) {
        *err = "control connect failed (" + cerr + ")"; ftpc_close(ctrl); return false;
    }
    mbedtls_ssl_session sess; mbedtls_ssl_session_init(&sess);
    bool have_sess = (mbedtls_ssl_get_session(&ctrl.ssl, &sess) == 0);

    auto fail = [&](const String& msg) -> bool {
        *err = msg + " [" + log + "]";
        ftpc_close(ctrl); mbedtls_ssl_session_free(&sess); return false;
    };

    String resp; int code;
    code = ftp_read_reply(ctrl, &resp); log += "GREET=" + String(code) + " ";
    if (code < 200 || code >= 300) return fail("no FTP greeting");
    code = ftp_send_cmd(ctrl, "USER bblp", &resp); log += "USER=" + String(code) + " ";
    if (code == 331) { code = ftp_send_cmd(ctrl, String("PASS ") + g_printer_access_code, &resp); log += "PASS=" + String(code) + " "; }
    if (code != 230) return fail("FTP login failed");
    ftp_send_cmd(ctrl, "PBSZ 0", &resp);
    ftp_send_cmd(ctrl, "PROT P", &resp);
    ftp_send_cmd(ctrl, "TYPE I", &resp);
    if (dir.length() > 0 && dir != "/") {
        code = ftp_send_cmd(ctrl, String("CWD ") + dir, &resp); log += "CWD=" + String(code) + " ";
        if (code < 200 || code >= 300) return fail("CWD \"" + dir + "\" failed: " + resp);
    }

    char data_ip[16];
    int data_port = ftp_open_passive(ctrl, g_printer_ip, data_ip, sizeof(data_ip), log);
    if (data_port < 0) return fail("no passive data port");

    FtpConn data; String derr;
    if (!ftpc_connect(data, data_ip, data_port, have_sess ? &sess : nullptr, &derr)) {
        ftpc_close(data); return fail("data connect failed (" + derr + ")");
    }
    code = ftp_send_cmd(ctrl, String("RETR ") + name, &resp); log += "RETR=" + String(code) + " ";
    if (code != 150 && code != 125) { ftpc_close(data); return fail("RETR \"" + name + "\" failed: " + resp); }

    uint32_t total = 0;
    static uint8_t dlbuf[4096];
    bool aborted = false;
    unsigned long start = millis();
    while (millis() - start < 30000) {           // 30 s idle timeout
        int n = ftpc_read(data, dlbuf, sizeof(dlbuf));
        if (n > 0) {
            total += (uint32_t)n;
            start = millis();
            if (cb && !cb(dlbuf, (unsigned int)n, ctx)) { aborted = true; break; }
        } else if (n < 0) {
            break;                               // transfer complete / closed
        } else {
            delay(2);
        }
    }
    ftpc_close(data);
    ftp_read_reply(ctrl, &resp);
    ftp_send_cmd(ctrl, "QUIT", &resp);
    ftpc_close(ctrl);
    mbedtls_ssl_session_free(&sess);

    if (out_bytes) *out_bytes = total;
    if (aborted) { *err = "aborted"; return false; }
    return total > 0;
}
