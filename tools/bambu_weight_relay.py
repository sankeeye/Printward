#!/usr/bin/env python3
"""
Bambu Cloud -> Panda Touch weight relay
========================================

Why this exists: the Panda Touch's own attempt to log into Bambu Cloud
directly (to find out how many grams a finished print used) gets blocked by
Cloudflare's bot-protection - it flat out returns an HTML challenge page
instead of ever reaching Bambu's login handler. That's a TLS-fingerprint
level block that an ESP32 can't get around.

A normal computer's HTTP client isn't blocked the same way, so this script
runs here on your PC instead: it logs into Bambu Cloud, periodically checks
your print history, and whenever it sees a finished print it hasn't reported
yet, sends the grams used to the Panda Touch over your local network. The
device then subtracts that from whichever spool/tray was active for that
print - same bookkeeping as if it could talk to Bambu Cloud itself.

Setup
-----
1. Install Python 3 if you don't have it (python.org).
2. Install the one dependency:
       pip install requests
3. Copy `config.example.json` to `config.json` (same folder as this script)
   and fill in your details - see comments in that file.
4. Run it:
       python bambu_weight_relay.py
   Leave it running in the background while you print. It only needs your
   PC on and connected to the internet + your home network; the printer and
   Panda Touch don't need anything extra.

Security note: the Bambu password is stored ENCRYPTED in config.json as
"bambu_password_enc" (Windows DPAPI, tied to your Windows user account) - run
encrypt_password.py once to convert a plaintext "bambu_password" into it and
strip the plaintext. This matters because config.json may live on a synced
drive (e.g. Google Drive), so a plaintext password would sync to the cloud.
A legacy plaintext "bambu_password" still works but isn't recommended.
Alternatively put a token into "custom_token" to skip the login entirely.
Either way, don't share config.json or commit it anywhere.

Keeping the device's own token fresh automatically
----------------------------------------------------
Since 2026-07-04 the Panda Touch can poll Bambu Cloud directly itself (it
turns out only the device's own *login* is Cloudflare-blocked - a request
using an already-issued token goes through fine). So this script's other
job now is to notice whenever the device has lost its token (expired,
rejected, or simply never set) and push the current one over, via
POST /api/cloud_set_token - no manual copy-pasting into the dashboard.
Bambu's tokens are valid for about 90 days and there is no working refresh
endpoint (Bambu's own /user/refreshtoken always returns 401 as of writing),
so getting a new one always means a full login - which, once every ~90
days or so, may require you to type a 6-digit code emailed to you. That one
step can't be automated away without giving this script access to your
email inbox, which it deliberately does not do. When it does happen, this
script asks for the code via the Panda Touch's web dashboard (open
http://<device_ip>/ in a browser) rather than a terminal, so it still works
when the relay runs hidden in the background. If the device is unreachable
it falls back to prompting in the terminal.
"""

import base64
import json
import os
import sys
import time
import urllib.parse
from datetime import datetime, timezone

try:
    import requests
except ImportError:
    print("Missing dependency: run 'pip install requests' first, then try again.")
    sys.exit(1)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(SCRIPT_DIR, "config.json")
STATE_PATH = os.path.join(SCRIPT_DIR, "relay_state.json")

BAMBU_API = "https://api.bambulab.com"


# --- Password protection via Windows DPAPI (optional, Windows-only) --------
# On Windows the Bambu password can be stored in config.json as an encrypted
# blob ("bambu_password_enc") instead of plaintext - handy because config.json
# may live on a synced drive (e.g. Google Drive). DPAPI ties the encryption to
# the current Windows user account: only this user on this machine can decrypt
# it, with no passphrase, so the hidden relay keeps working unattended. Convert
# a plaintext password once with encrypt_password.py.
#
# This is entirely optional. On Linux/macOS (or if you just prefer it) put a
# plaintext "bambu_password" in your own config.json (which is gitignored) or a
# token in "custom_token". The DPAPI code below is only defined on Windows so
# the relay still imports and runs everywhere.
_IS_WINDOWS = os.name == "nt"

if _IS_WINDOWS:
    import ctypes
    from ctypes import wintypes

    class _DATA_BLOB(ctypes.Structure):
        _fields_ = [("cbData", wintypes.DWORD),
                    ("pbData", ctypes.POINTER(ctypes.c_char))]

    def _dpapi(func, data):
        buf = ctypes.create_string_buffer(bytes(data), len(data))
        blob_in = _DATA_BLOB(len(data), ctypes.cast(buf, ctypes.POINTER(ctypes.c_char)))
        blob_out = _DATA_BLOB()
        if not func(ctypes.byref(blob_in), None, None, None, None, 0, ctypes.byref(blob_out)):
            raise ctypes.WinError()
        try:
            return ctypes.string_at(blob_out.pbData, blob_out.cbData)
        finally:
            ctypes.windll.kernel32.LocalFree(blob_out.pbData)

    def dpapi_encrypt(plaintext):
        """Encrypt a string with the current Windows user's DPAPI key -> base64."""
        return base64.b64encode(_dpapi(ctypes.windll.crypt32.CryptProtectData,
                                       plaintext.encode("utf-8"))).decode("ascii")

    def dpapi_decrypt(b64):
        """Reverse of dpapi_encrypt; only works for the same Windows user + machine."""
        return _dpapi(ctypes.windll.crypt32.CryptUnprotectData,
                      base64.b64decode(b64)).decode("utf-8")
else:
    def dpapi_encrypt(plaintext):
        raise RuntimeError("Password encryption uses Windows DPAPI and is only "
                           "available on Windows.")

    def dpapi_decrypt(b64):
        raise RuntimeError(
            "config.json has an encrypted 'bambu_password_enc', but DPAPI "
            "decryption is Windows-only and that blob only works on the Windows "
            "account that created it. On this OS, put a plaintext 'bambu_password' "
            "in config.json (it's gitignored) or a token in 'custom_token' instead.")


def resolve_password(cfg):
    """Bambu password from config, preferring the DPAPI-encrypted field and
    falling back to a legacy plaintext field."""
    if cfg.get("bambu_password_enc"):
        return dpapi_decrypt(cfg["bambu_password_enc"])
    return cfg.get("bambu_password", "")


def load_json(path, default):
    if not os.path.exists(path):
        return default
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_json(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def load_config():
    if not os.path.exists(CONFIG_PATH):
        print(f"No config.json found at {CONFIG_PATH}")
        print("Copy config.example.json to config.json and fill it in first.")
        sys.exit(1)
    cfg = load_json(CONFIG_PATH, {})
    required = ["bambu_email", "printer_serial", "device_ip"]
    missing = [k for k in required if not cfg.get(k)]
    if missing:
        print(f"config.json is missing: {', '.join(missing)}")
        sys.exit(1)
    # A login needs one of: an existing token, a DPAPI-encrypted password, or a
    # legacy plaintext password.
    if not (cfg.get("custom_token") or cfg.get("bambu_password_enc") or cfg.get("bambu_password")):
        print("config.json needs one of: bambu_password_enc (run encrypt_password.py), "
              "bambu_password, or custom_token.")
        sys.exit(1)
    cfg.setdefault("poll_interval_sec", 120)
    return cfg


def load_state():
    return load_json(STATE_PATH, {"access_token": "", "last_task_key": ""})


def save_state(state):
    save_json(STATE_PATH, state)


def bambu_request_email_code(email):
    resp = requests.post(
        f"{BAMBU_API}/v1/user-service/user/sendemail/code",
        json={"email": email, "type": "codeLogin"},
        timeout=15,
    )
    data = resp.json() if resp.content else {}
    if resp.status_code != 200 or data.get("success") is False:
        raise RuntimeError(f"Couldn't request a verification code: HTTP {resp.status_code}: {data}")


def bambu_login_with_code(email, code):
    resp = requests.post(
        f"{BAMBU_API}/v1/user-service/user/login",
        json={"account": email, "code": code},
        timeout=15,
    )
    data = resp.json()
    if resp.status_code != 200 or not data.get("accessToken"):
        raise RuntimeError(f"Code login failed: HTTP {resp.status_code}: {data.get('error') or data.get('message') or data}")
    return data["accessToken"]


def bambu_login(email, password, get_code=None):
    """Logs into Bambu Cloud, returns an access token.

    Bambu now requires an emailed 6-digit verification code on top of the
    password for most logins (their "loginType": "verifyCode" response) -
    this isn't something that can be skipped or automated without reading
    your email, so when it happens this triggers Bambu to send the code and
    then collects it. You only need to do this occasionally (whenever the
    cached token expires or gets rejected), not every single time.

    get_code(email) -> str supplies that code. By default it's read from this
    terminal, but main() passes one that instead asks for it via the Panda
    Touch's web dashboard, so the relay can run hidden with no console (see
    make_code_prompt()).
    """
    if get_code is None:
        get_code = lambda em: input(f"Check {em} for a 6-digit code from Bambu and enter it here: ").strip()

    resp = requests.post(
        f"{BAMBU_API}/v1/user-service/user/login",
        json={"account": email, "password": password},
        timeout=15,
    )
    data = resp.json()
    if resp.status_code != 200 and not data.get("loginType"):
        raise RuntimeError(f"Login failed: HTTP {resp.status_code}: {data.get('error') or data.get('message')}")

    if data.get("loginType") == "verifyCode":
        print("Bambu wants a verification code for this login - requesting one by email...")
        bambu_request_email_code(email)
        code = get_code(email)
        return bambu_login_with_code(email, code)

    if data.get("loginType"):
        raise RuntimeError(
            f"Bambu wants login type '{data['loginType']}', which this script doesn't support "
            "(e.g. app-based two-factor auth). Try logging in via Bambu Studio or the Bambu Handy "
            "app first, then retry here."
        )

    token = data.get("accessToken", "")
    if not token:
        raise RuntimeError("Login response had no accessToken - unexpected response shape.")
    return token


def get_recent_tasks(token, printer_serial, limit=5):
    resp = requests.get(
        f"{BAMBU_API}/v1/user-service/my/tasks",
        params={"deviceId": printer_serial, "limit": limit},
        headers={"Authorization": f"Bearer {token}"},
        timeout=15,
    )
    if resp.status_code == 401:
        return None  # token expired/invalid - caller should re-login
    resp.raise_for_status()
    return resp.json().get("hits", [])


def report_weight_to_device(device_ip, weight_g, task_key):
    url = f"http://{device_ip}/api/report_weight"
    resp = requests.post(url, data={"weight_g": weight_g, "task_key": task_key}, timeout=10)
    resp.raise_for_status()
    return resp.json()


def get_device_cloud_status(device_ip):
    resp = requests.get(f"http://{device_ip}/api/cloud_status", timeout=10)
    resp.raise_for_status()
    return resp.json()


def push_token_to_device(device_ip, token):
    resp = requests.post(f"http://{device_ip}/api/cloud_set_token", data={"token": token}, timeout=10)
    resp.raise_for_status()
    return resp.json()


def device_request_verification_code(device_ip, email):
    """Tells the Panda Touch to show a 'enter your Bambu code' field in its web
    dashboard. Doesn't ask Bambu for anything - bambu_login() already triggered
    the email; this just routes the prompt to a browser instead of a terminal."""
    resp = requests.post(f"http://{device_ip}/api/cloud_need_code", data={"email": email}, timeout=10)
    resp.raise_for_status()
    return resp.json()


def device_get_pending_code(device_ip):
    resp = requests.get(f"http://{device_ip}/api/cloud_pending_code", timeout=10)
    resp.raise_for_status()
    return resp.json()  # {"needed": bool, "code": str}


def device_clear_code(device_ip):
    """Best-effort reset of the dashboard's code prompt after login finishes."""
    try:
        requests.post(f"http://{device_ip}/api/cloud_code_done", timeout=10)
    except requests.exceptions.RequestException:
        pass


def make_code_prompt(device_ip):
    """Returns a get_code(email) that collects the 6-digit Bambu code via the
    Panda Touch's web dashboard, so it works even when this relay runs hidden
    with no console. Falls back to a terminal prompt if the device can't be
    reached (e.g. it's off, or you started start_relay.bat directly)."""
    def get_code(email):
        try:
            device_request_verification_code(device_ip, email)
        except requests.exceptions.RequestException:
            print(f"(Couldn't reach the Panda Touch at {device_ip} to ask for the code "
                  f"there - falling back to this terminal.)")
            return input(f"Check {email} for a 6-digit code from Bambu and enter it here: ").strip()

        print(f"Bambu needs a verification code. Open the Panda Touch dashboard at "
              f"http://{device_ip}/ and type the 6-digit code emailed to {email}.")
        deadline = time.time() + 15 * 60  # wait up to 15 min for it to be typed
        while time.time() < deadline:
            try:
                pending = device_get_pending_code(device_ip)
                code = (pending.get("code") or "").strip()
                if code:
                    return code
            except requests.exceptions.RequestException:
                pass  # transient (e.g. device rebooting) - keep waiting
            time.sleep(3)
        raise RuntimeError("Timed out waiting for the verification code from the dashboard.")
    return get_code


def ensure_device_has_token(device_ip, token):
    """Checks whether the Panda Touch currently has a working Bambu Cloud
    token and, if not, pushes the one this script is using. Cheap (one GET)
    on the common case where the device is already fine; only writes to the
    device's flash when it's actually needed, so this is safe to call every
    cycle."""
    try:
        status = get_device_cloud_status(device_ip)
    except requests.exceptions.RequestException as e:
        print(f"Couldn't check device's Bambu Cloud status: {e}")
        return
    if status.get("logged_in"):
        return
    print("Device has no active Bambu Cloud token - pushing the current one so it can poll directly...")
    try:
        push_token_to_device(device_ip, token)
        print("Token pushed to device.")
    except requests.exceptions.RequestException as e:
        print(f"Couldn't push token to device: {e}")


def is_real_finish(task):
    """Bambu Cloud lists a print as soon as it starts, with endTime ~=
    startTime until it actually finishes - skip those still-in-progress
    placeholder rows, only real completed jobs have a meaningfully later
    endTime and a non-zero weight."""
    weight = task.get("weight") or 0
    if weight <= 0:
        return False
    start = task.get("startTime")
    end = task.get("endTime")
    if not start or not end:
        return False
    try:
        start_dt = datetime.fromisoformat(start.replace("Z", "+00:00"))
        end_dt = datetime.fromisoformat(end.replace("Z", "+00:00"))
    except ValueError:
        return True  # can't parse - don't block on this check
    return (end_dt - start_dt).total_seconds() > 60


def main():
    cfg = load_config()
    state = load_state()

    token = cfg.get("custom_token") or state.get("access_token") or ""

    # Collect any verification code Bambu asks for via the Panda Touch's web
    # dashboard instead of this terminal, so the relay works while hidden.
    code_prompt = make_code_prompt(cfg["device_ip"])

    def ensure_token():
        nonlocal token
        if token:
            return
        print("Logging into Bambu Cloud...")
        try:
            token = bambu_login(cfg["bambu_email"], resolve_password(cfg), code_prompt)
        finally:
            # Whatever happened, don't leave the dashboard showing a stale prompt.
            device_clear_code(cfg["device_ip"])
        state["access_token"] = token
        save_state(state)
        print("Logged in.")

    print(f"Bambu weight relay starting. Keeping the Panda Touch at {cfg['device_ip']}'s Bambu Cloud "
          f"token fresh and reporting any prints it missed, every {cfg['poll_interval_sec']}s. Ctrl+C to stop.")

    while True:
        try:
            ensure_token()
            ensure_device_has_token(cfg["device_ip"], token)
            tasks = get_recent_tasks(token, cfg["printer_serial"])

            if tasks is None:
                print("Bambu token expired/rejected - logging in again.")
                token = ""
                state["access_token"] = ""
                save_state(state)
                time.sleep(2)
                continue

            # Bambu's list is newest-first; walk backwards so we report older
            # finished prints before newer ones if several piled up.
            new_tasks = [
                t for t in tasks
                if is_real_finish(t) and t.get("endTime", "") > state.get("last_task_key", "")
            ]
            new_tasks.sort(key=lambda t: t.get("endTime", ""))

            for t in new_tasks:
                weight = t.get("weight")
                end_time = t.get("endTime")
                name = t.get("title", "?")
                print(f"Reporting finished print '{name}': {weight}g (endTime {end_time})")
                report_weight_to_device(cfg["device_ip"], weight, end_time)
                state["last_task_key"] = end_time
                save_state(state)

            if not new_tasks:
                print(f"[{datetime.now(timezone.utc).isoformat(timespec='seconds')}] No new finished prints.")

        except requests.exceptions.RequestException as e:
            print(f"Network error, will retry next cycle: {e}")
        except RuntimeError as e:
            print(f"Bambu Cloud error: {e}")
            token = ""
            state["access_token"] = ""
            save_state(state)

        time.sleep(cfg.get("poll_interval_sec", 120))


if __name__ == "__main__":
    main()
