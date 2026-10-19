# Isolde 0.0.1.1 manual tests

Run the debug build fullscreen and complete these checks without a mouse.

1. **Startup:** Home appears and Movies has a visible white focus border.
2. **Navigation:** Arrow keys move predictably across the 3×2 grid and stop at its edges.
3. **Activation:** Focus Switch and press Enter; a fullscreen Switch simulated session appears.
4. **Back and focus:** Press Escape; Home returns with Switch still focused.
5. **Second app:** Launch YouTube, press Escape, and verify YouTube regains focus.
6. **Home:** Launch any tile and press Home or H; Home appears immediately with the launching tile focused.
7. **Configuration:** Add a valid mock object to `config/apps.json`, rebuild, and verify its generic tile launches without QML changes.
8. **Fullscreen/no mouse:** Repeat navigation and launching in fullscreen using only the mapped keys.
