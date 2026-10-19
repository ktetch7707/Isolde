# Isolde 0.0.1.2

Isolde is a remote-first television shell built around one rule: **everything is an app**. The current development build includes isolated profiles, customizable Home, a curated Store, web/native sessions, settings and recovery, and Kodi-backed movies, shows/seasons/episodes, recent items, Continue Watching, local artwork and movie collection shelves. Kodi owns media organization, progress and playback. HDMI capture and the full appliance product remain unfinished. See the [implementation order](docs/implementation-order.md) for the current assessment, dependencies and release gates.

The [component contract](docs/components.md) documents versioned Home layouts, validated actions, remote source repair and the permanent Settings/Edit Home recovery controls.

The [interface direction](docs/interface-design.md) defines the polished default preset and user-owned navigation, categories, shelves and appearance. Customize Home → Make it yours previews per-profile colors, text size, density, spacing, corners and motion before applying them. Full Library category/shelf browsing and complete remote editing remain in progress.

[Home composition](docs/home-composition.md) now supports optional configured tabs and banner, media, source and guide-summary cards, with independent section layouts and local artwork crops. Customize Home → Component layout edits section styles, density and height. The [reference recreation fixture](tests/visual/home-reference/recreation-config.json) is an opt-in visual test; its media progress and guide/capture entries do not establish backend readiness.

Next major package, planned: [Isolde Studio](docs/studio-builder.md) consolidates the supplied October 17 Home Screen Studio plan — universal grid builder, Simple/Studio/Advanced editing modes, typed components, templates/tokens and a canonical versioned layout schema. It is requirements and sequencing only; nothing there is implemented yet.

## Requirements

- Linux (Debian 12, Ubuntu 24.04, or a comparable distribution)
- CMake 3.21+
- Qt 6.4+ development packages: Core, Gui, Network, Qml, Quick, Test, WebEngineQuick, and WebChannel
- A C++17 compiler and Ninja (recommended)

On Ubuntu, the relevant packages are typically `cmake`, `ninja-build`, `g++`, `qt6-base-dev`, `qt6-declarative-dev`, and `qt6-tools-dev`.

## Build and run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/isolde
```

Isolde starts fullscreen. During development, use `./build/isolde --windowed`.

## Controls

| Input | Action |
|---|---|
| Arrow keys | Move tile focus |
| Enter, Return, or Space | Launch focused tile |
| Escape | Back |
| Home or H | Home |

Normal prototype use does not require a mouse. Escape and Home are distinct commands even though both return from a top-level mock session to Home in this milestone.

## Adding an app

Add an object to the deployed `config/apps.json` beside the executable with a unique `id`, a display `name`, `type: "mock"`, `section`, color `accent`, and short `symbol`. Restart Isolde; no app-specific QML or rebuild is required. The source config is also embedded as an appliance-safe fallback.

## Project layout

- `src/app`: config-backed application definitions and registry
- `src/sessions`: session abstraction, mock implementation, and lifecycle manager
- `src/input`: physical-key to abstract-command normalization
- `qml`: presentation-only TV shell and mock-session views
- `config`: external app metadata
- `tests`: registry and session lifecycle tests
- `docs`: architecture and navigation decisions

See the [development plan](docs/development-plan.md), [architecture](docs/architecture.md), [audio strategy](docs/audio.md), [remote setup plan](docs/remotes.md), [install path](docs/install.md), [package lifecycle](docs/packages.md), [navigation](docs/navigation.md), and [manual test procedure](tests/MANUAL_TESTS.md).

Home appearance: [select and save skins](docs/skins.md) from Settings → Home, including Isolde default, Cinema lounge and Pastel sky.

Custom destinations: [design pages and Kodi library widgets](docs/pages.md) in Settings → Home → Pages, then link them from Home tabs/buttons or Movies/Shows.
