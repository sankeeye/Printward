#include "bambu_ftp.h"
#include "storage.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Reads one FTP reply off `c`, handling multi-line replies (RFC 959: a
// continuation line looks like "150-...", the reply ends with a line that
// has the same 3-digit code followed by a space, e.g. "150 Ready"). Returns
// the numeric code, or -1 on timeout/disconnect before a final line arrived.
static int ftp_read_reply(WiFiClientSecure& c, String* full_out, unsigned long timeout_ms = 8000) {
    String all;
    String line;
    unsigned long start = millis();
    while (millis() - start < timeout_ms) {
        if (c.available()) {
            char ch = c.read();
            line += ch;
            start = millis();
            if (ch == '\n') {
                all += line;
                bool final_line = line.length() >= 4 &&
                    isDigit(line[0]) && isDigit(line[1]) && isDigit(line[2]) && line[3] == ' ';
                if (final_line) {
                    if (full_out) *full_out = all;
                    return line.substring(0, 3).toInt();
                }
                line = "";
            }
        } else if (!c.connected()) {
            break;
        } else {
            delay(5);
        }
    }
    if (full_out) *full_out = all;
    return -1;
}

static int ftp_send_cmd(WiFiClientSecure& c, const String& cmd, String* full_out = nullptr) {
    c.print(cmd);
    c.print("\r\n");
    return ftp_read_reply(c, full_out);
}

// Parses EPSV's reply (RFC 2428), e.g. "229 Entering Extended Passive Mode
// (|||6446|)." - the delimiter character (usually '|') is whatever comes
// right after the opening paren; the port is between the last two
// occurrences of it. Unlike PASV, EPSV doesn't give a separate data-channel
// IP - the same host as the control connection is used. Returns -1 if the
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

// Parses Unix-style `ls -l` lines, e.g.:
//   "-rwxrwxrwx 1 user group    12345 Jan  1 00:00 my_print.3mf"
//   "drwxrwxrwx 1 user group        0 Jan  1 00:00 cache"
// Fields before the name are whitespace-separated and (on the embedded FTP
// daemons Bambu printers use) don't vary in count, but the name itself may
// contain spaces, so it's taken as "everything after the 8th field" rather
// than split further.
static int parse_listing(const String& listing, FtpEntry* out, int max_entries) {
    int count = 0;
    int line_start = 0;
    int len = listing.length();
    while (line_start < len && count < max_entries) {
        int nl = listing.indexOf('\n', line_start);
        if (nl < 0) nl = len;
        String line = listing.substring(line_start, nl);
        line.trim();
        line_start = nl + 1;
        if (line.length() == 0) continue;

        bool is_dir = (line[0] == 'd');
        int i = 0;
        int n = line.length();
        int fields_seen = 0;
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

bool bambu_ftp_list(const char* path, FtpEntry* out, int max_entries, int* out_count, String* err) {
    *out_count = 0;
    if (strlen(g_printer_ip) == 0) {
        *err = "No printer IP configured yet";
        return false;
    }

    // Compact step-by-step trail of every command's reply code, appended to
    // *err on any failure. After several rounds of guessing blind from a
    // single reply code reported over chat, this is worth more than any one
    // more speculative protocol fix - it tells us exactly which step in the
    // sequence actually failed instead of us guessing from one number.
    String log;

    WiFiClientSecure ctrl;
    // Same trust trade-off as the LAN MQTT link and Bambu Cloud calls
    // elsewhere in this firmware - no CA bundle loaded on-device.
    ctrl.setInsecure();
    ctrl.setTimeout(8000);
    if (!ctrl.connect(g_printer_ip, 990)) {
        *err = "Couldn't connect to the printer on port 990 (FTPS)";
        return false;
    }

    String resp;
    int code;

    // A captured FileZilla session against this exact printer shows PASS
    // succeeding immediately (230) with no ACCT step at all. Every attempt
    // from this firmware instead got 332 ("need account") on PASS - and
    // every time that happened, every later command (PBSZ, TYPE, PASV,
    // CWD-to-root) came back 502, the signature of some kind of
    // degraded/conflicting session - most likely a control connection from
    // an earlier attempt that the printer hadn't fully torn down yet (this
    // code has been reconnecting a lot during testing). Rather than treating
    // 332 as a normal step to answer with ACCT, close the connection and
    // retry once with a fresh one first, which is effectively what a client
    // that always opens a brand-new connection (like FileZilla) gets for
    // free. ACCT is still there as a last-resort fallback if even a clean
    // retry doesn't get a direct 230.
    for (int reconnect_attempt = 0; reconnect_attempt < 2; reconnect_attempt++) {
        if (reconnect_attempt > 0) {
            ctrl.stop();
            delay(300);
            if (!ctrl.connect(g_printer_ip, 990)) {
                *err = "Couldn't reconnect to the printer on port 990 (FTPS) [" + log + "]";
                return false;
            }
        }

        code = ftp_read_reply(ctrl, &resp);
        log += "GREET=" + String(code) + " ";
        if (code < 200 || code >= 300) {
            *err = "FTP didn't answer with a greeting: " + resp + " [" + log + "]";
            ctrl.stop();
            return false;
        }

        code = ftp_send_cmd(ctrl, "USER bblp", &resp);
        log += "USER=" + String(code) + " ";
        if (code == 331) {
            code = ftp_send_cmd(ctrl, String("PASS ") + g_printer_access_code, &resp);
            log += "PASS=" + String(code) + " ";
        }

        if (code == 230) break; // clean login, matching the working FileZilla trace

        if (reconnect_attempt == 1 && code == 332) {
            // Still not a clean login on the second, fresh connection -
            // fall back to answering ACCT so we at least log in, even if
            // the session ends up degraded.
            code = ftp_send_cmd(ctrl, String("ACCT ") + g_printer_access_code, &resp);
            log += "ACCT=" + String(code) + " ";
        }
    }
    if (code != 230) {
        *err = "FTP login failed - check the LAN access code: " + resp + " [" + log + "]";
        ctrl.stop();
        return false;
    }

    // From here on, mirror the working FileZilla trace command-for-command:
    // SYST/FEAT (informational, printer replies 502 "no-features" to FEAT -
    // expected and harmless), PBSZ/PROT (implicit-FTPS ritual this daemon
    // apparently does expect despite port 990 already being all-TLS), PWD,
    // then TYPE I, all logged but not treated as fatal by themselves - only
    // the actual PASV/EPSV/LIST/NLST failures below abort the listing.
    ftp_send_cmd(ctrl, "SYST", &resp);
    ftp_send_cmd(ctrl, "FEAT", &resp);
    code = ftp_send_cmd(ctrl, "PBSZ 0", &resp);
    log += "PBSZ=" + String(code) + " ";
    code = ftp_send_cmd(ctrl, "PROT P", &resp);
    log += "PROT=" + String(code) + " ";
    ftp_send_cmd(ctrl, "PWD", &resp);
    code = ftp_send_cmd(ctrl, "TYPE I", &resp);
    log += "TYPE=" + String(code) + " ";

    // CWD only when navigating into an actual subfolder - the working trace
    // never sends "CWD /" for the root listing itself (only PWD/TYPE/PASV),
    // and a live test against this printer confirmed "CWD /" alone gets 502.
    if (path && path[0] != '\0' && !(path[0] == '/' && path[1] == '\0')) {
        code = ftp_send_cmd(ctrl, String("CWD ") + path, &resp);
        log += "CWD=" + String(code) + " ";
        if (code < 200 || code >= 300) {
            *err = "Couldn't open folder \"" + String(path) + "\": " + resp + " [" + log + "]";
            ctrl.stop();
            return false;
        }
    }

    char data_ip[16];
    strncpy(data_ip, g_printer_ip, sizeof(data_ip) - 1);
    data_ip[sizeof(data_ip) - 1] = '\0';
    int data_port = -1;

    code = ftp_send_cmd(ctrl, "PASV", &resp);
    log += "PASV=" + String(code) + " ";
    if (code == 227) {
        int p1 = resp.indexOf('(');
        int p2 = resp.indexOf(')', p1);
        if (p1 < 0 || p2 < 0) {
            *err = "Couldn't parse PASV reply: " + resp + " [" + log + "]";
            ctrl.stop();
            return false;
        }
        String nums = resp.substring(p1 + 1, p2);
        int parts[6] = {0, 0, 0, 0, 0, 0};
        int idx = 0, val = 0;
        for (size_t i = 0; i < nums.length() && idx < 6; i++) {
            char ch = nums[i];
            if (ch == ',') {
                parts[idx++] = val;
                val = 0;
            } else if (isDigit(ch)) {
                val = val * 10 + (ch - '0');
            }
        }
        if (idx == 5) parts[5] = val;
        snprintf(data_ip, sizeof(data_ip), "%d.%d.%d.%d", parts[0], parts[1], parts[2], parts[3]);
        data_port = parts[4] * 256 + parts[5];
    } else {
        // Some embedded FTP daemons (this printer included, apparently)
        // don't implement classic PASV at all (502 "not implemented") - try
        // the newer Extended Passive Mode instead before giving up.
        code = ftp_send_cmd(ctrl, "EPSV", &resp);
        log += "EPSV=" + String(code) + " ";
        if (code == 229) {
            data_port = parse_epsv_port(resp);
        }
    }

    // Whichever listing verb we try, some minimal embedded FTP daemons only
    // implement one of LIST/NLST (both are supposed to be standard, but
    // Bambu's seems to reject at least LIST outright in some configurations)
    // - try LIST first, then NLST as a fallback if LIST itself comes back
    // "not implemented" rather than that being a data-channel problem.
    static const char* kListCmds[2] = {"LIST", "NLST"};

    if (data_port >= 0) {
        // Passive mode (PASV or EPSV worked): we connect out to the printer
        // for the data channel, same TLS trust trade-off as the control
        // connection. A fresh data connection is needed per attempt (FTP
        // data channels are single-use), so this re-connects for each
        // listing verb tried.
        for (int i = 0; i < 2; i++) {
            WiFiClientSecure data;
            data.setInsecure();
            data.setTimeout(8000);
            if (!data.connect(data_ip, data_port)) {
                *err = "Couldn't open the FTP data connection [" + log + "]";
                ctrl.stop();
                return false;
            }

            code = ftp_send_cmd(ctrl, kListCmds[i], &resp);
            log += String(kListCmds[i]) + "=" + String(code) + " ";
            if (code != 150 && code != 125) {
                data.stop();
                if (i == 0 && code == 502) continue; // try NLST next
                *err = String(kListCmds[i]) + " failed: " + resp + " [" + log + "]";
                ctrl.stop();
                return false;
            }

            String listing;
            unsigned long start = millis();
            while (millis() - start < 8000) {
                while (data.available()) {
                    listing += (char)data.read();
                    start = millis();
                }
                if (!data.connected() && !data.available()) break;
                delay(5);
            }
            data.stop();

            // Final "226 Transfer complete" on the control channel.
            ftp_read_reply(ctrl, &resp);
            ftp_send_cmd(ctrl, "QUIT", &resp);
            ctrl.stop();

            *out_count = parse_listing(listing, out, max_entries);
            return true;
        }
        *err = "Printer's FTP server doesn't support LIST or NLST in passive mode [" + log + "]";
        ctrl.stop();
        return false;
    }

    // Neither PASV nor EPSV worked (this printer's FTP daemon apparently
    // doesn't implement either) - fall back to active mode: we listen, tell
    // the printer where via PORT, and it connects to us instead of the
    // other way around. Tried here as a plain (non-TLS) data connection -
    // if this particular daemon insists on encrypting the data channel too,
    // this will fail at the accept/read step below, since running a TLS
    // *server* would need a certificate this device doesn't have.
    IPAddress my_ip = WiFi.localIP();

    for (int i = 0; i < 2; i++) {
        int active_port = 50100 + (int)(millis() % 100) + i;
        WiFiServer active_server(active_port);
        active_server.begin();

        char port_cmd[48];
        snprintf(port_cmd, sizeof(port_cmd), "PORT %d,%d,%d,%d,%d,%d",
                 my_ip[0], my_ip[1], my_ip[2], my_ip[3], active_port / 256, active_port % 256);
        code = ftp_send_cmd(ctrl, port_cmd, &resp);
        log += "PORT=" + String(code) + " ";
        if (code != 200) {
            *err = "Active mode (PORT) failed too - printer's FTP server doesn't support PASV, EPSV, or PORT: " + resp + " [" + log + "]";
            active_server.end();
            ctrl.stop();
            return false;
        }

        code = ftp_send_cmd(ctrl, kListCmds[i], &resp);
        log += String(kListCmds[i]) + "=" + String(code) + " ";
        if (code != 150 && code != 125) {
            active_server.end();
            if (i == 0 && code == 502) continue; // try NLST next
            *err = String(kListCmds[i]) + " failed (active mode): " + resp + " [" + log + "]";
            ctrl.stop();
            return false;
        }

        WiFiClient active_data;
        unsigned long accept_start = millis();
        while (millis() - accept_start < 8000) {
            active_data = active_server.accept();
            if (active_data) break;
            delay(20);
        }
        if (!active_data) {
            *err = "Printer never connected back for the active-mode data transfer (a firewall or router on this network may be blocking it) [" + log + "]";
            active_server.end();
            ctrl.stop();
            return false;
        }

        String listing;
        unsigned long start = millis();
        while (millis() - start < 8000) {
            while (active_data.available()) {
                listing += (char)active_data.read();
                start = millis();
            }
            if (!active_data.connected() && !active_data.available()) break;
            delay(5);
        }
        active_data.stop();
        active_server.end();

        ftp_read_reply(ctrl, &resp);
        ftp_send_cmd(ctrl, "QUIT", &resp);
        ctrl.stop();

        *out_count = parse_listing(listing, out, max_entries);
        return true;
    }

    *err = "Printer's FTP server doesn't support LIST or NLST in active mode either [" + log + "]";
    ctrl.stop();
    return false;
}
