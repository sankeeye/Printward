# Contributing to Printward

Thanks for your interest! Printward is a spare-time project, but issues and pull
requests are welcome.

## Reporting a bug or an idea

Open an issue. A screenshot, the tablet's `adb logcat` output, or the exact steps to
reproduce help a lot. For security problems, see [SECURITY.md](SECURITY.md) instead.

## Building it yourself

- **Tablet app** (LVGL UI + web server): see [`android/README.md`](android/README.md)
  for the Android NDK + Gradle build. The same `src/ui_*` code also builds as a **PC
  simulator** for quick UI work — see [`sim/`](sim/).
- **Scale firmware** (ESP32-S3 + HX711): open [`scale/`](scale/) in VS Code with the
  PlatformIO extension and flash over USB-C.

## Code style

- Code, comments and commit messages in **English**. The UI itself is translated
  (EN/NL/DE) — add strings via the tables in [`lang/`](lang/), not hard-coded text.
- Match the style of the surrounding code and keep changes focused.
- Commit messages follow Conventional Commits (`feat:`, `fix:`, `docs:` …).

## License

By contributing, you agree that your work is licensed under the project's
**GNU AGPL-3.0** (see [LICENSE](LICENSE)). The license covers the code, **not the name
"Printward"** or its branding — fork freely, but give your version its own name.
