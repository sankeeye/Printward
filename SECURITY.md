# Security Policy

Printward talks to a real 3D printer and runs a small web server on your LAN, so a
few things are worth knowing — both about how it's built and about reporting an issue.

## Security model

- **Local network only.** The web interface refuses connections from outside your own
  network, so a forwarded port or UPnP can't put the printer on the internet.
- **Password.** The tablet generates a random web password on first run (shown under
  Settings ▸ Web password); there is no shared default. Sign in as user `printward`.
- **The printer's LAN access code is a secret.** It lives only on the tablet
  (`/sdcard/printward.conf`), is never committed to git, and is excluded from the
  `/backup` export.
- It is plain HTTP: the password crosses your own LAN unencrypted. That is enough to
  keep the printer off the internet and away from other devices on the Wi‑Fi — it is
  not a defence against someone actively sniffing your network.

## Reporting a vulnerability

Please report security problems **privately**: go to the repository's **Security** tab
and choose **"Report a vulnerability"** (a private security advisory). That keeps the
details out of public view until there's a fix.

For low-risk issues you can also just open a regular issue.

This is a small hobby project maintained in spare time, so please allow some time for
a response. Thanks for helping keep it safe.
