#!/usr/bin/env python3
"""One-time helper: encrypt the Bambu password in config.json with Windows
DPAPI so it is no longer stored in plaintext.

Run it once - double-click encrypt_password.bat, or `python encrypt_password.py`.
It reads the plaintext "bambu_password" from config.json, replaces it with an
encrypted "bambu_password_enc" blob, and removes the plaintext. DPAPI ties the
blob to your Windows user account, so only you on this PC can decrypt it and the
hidden relay keeps working without any password prompt. This matters because
config.json may live on a synced drive (e.g. Google Drive).

If config.json has no plaintext password it asks you to type one. Windows only.
"""
import base64
import ctypes
from ctypes import wintypes
import getpass
import json
import os
import sys

CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")


class _DATA_BLOB(ctypes.Structure):
    _fields_ = [("cbData", wintypes.DWORD),
                ("pbData", ctypes.POINTER(ctypes.c_char))]


def dpapi_encrypt(plaintext):
    data = plaintext.encode("utf-8")
    buf = ctypes.create_string_buffer(data, len(data))
    blob_in = _DATA_BLOB(len(data), ctypes.cast(buf, ctypes.POINTER(ctypes.c_char)))
    blob_out = _DATA_BLOB()
    if not ctypes.windll.crypt32.CryptProtectData(
            ctypes.byref(blob_in), None, None, None, None, 0, ctypes.byref(blob_out)):
        raise ctypes.WinError()
    try:
        enc = ctypes.string_at(blob_out.pbData, blob_out.cbData)
    finally:
        ctypes.windll.kernel32.LocalFree(blob_out.pbData)
    return base64.b64encode(enc).decode("ascii")


def main():
    if os.name != "nt":
        print("This script only works on Windows (it uses DPAPI).")
        sys.exit(1)
    if not os.path.exists(CONFIG_PATH):
        print(f"No config.json found at {CONFIG_PATH}")
        sys.exit(1)

    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        cfg = json.load(f)

    if cfg.get("bambu_password_enc") and not cfg.get("bambu_password"):
        print("config.json already uses an encrypted password - nothing to do.")
        return

    pw = cfg.get("bambu_password")
    if not pw:
        pw = getpass.getpass("Enter your Bambu account password to encrypt: ").strip()
    if not pw:
        print("No password given - aborting.")
        sys.exit(1)

    cfg["bambu_password_enc"] = dpapi_encrypt(pw)
    cfg.pop("bambu_password", None)

    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)

    print("Done. Password encrypted as 'bambu_password_enc' and plaintext removed "
          "from config.json.")


if __name__ == "__main__":
    main()
