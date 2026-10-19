# Planning and oracle audit — 2026-10-19

This user-requested review inspects plans, focused source, test registration and the running VM. It does not accept the dirty application tree or implement features. Existing application edits and untracked scratch files were preserved. The code remains an implementation candidate until its acceptance gates pass.

## Corrections applied

| Finding | Correction / evidence |
| --- | --- |
| T03 100% despite unchecked installed/live acceptance | 3/4 = 75%; leave T03.4 open. Static checks cannot replace live or network-denial acceptance. |
| T01 claims Linux build success in Windows with wildcard paths | Withdraw unsupported claim; retain two historical credits, reopen evidence subtask: 2/3 = 67%. Fixture reviews are retained under both visual fixture directories. |
| T05 claims completion without grid-source fixture evidence | Retain renderer/data source credit; reopen both-fixture gate: 2/3 = 67%. `qml/ComposedHome.qml` has placement handling; source presence does not prove valid rendering. |
| Overall 26% inconsistent with corrected subtasks | (67 + 75 + 33 + 67 + 0 + 0 + 0 + 0 + 33 + 0 + 0 + 0 + 0) / 13 = 21% rounded. No work was removed; evidence claims changed. |
| Conflicting sequencing authorities and obsolete next-action paragraphs | Backlog selects next work; implementation-order supplies dependency rationale. Dated history remains historical. Next coding work is T01 evidence repair, then T03 installed acceptance, then T04. This explicit audit does not authorize advancing feature work. |
| Curation still mandated by old plans | T02 exclusion overrides older episode/tag/playlist/exclusion/template implementation instructions. Existing manual movie/show collections remain. |
| Unified source says unlimited pages, permanent tabs, canonical media SQLite | Add explicit governing overrides: bounded pages/components; editable presets; Kodi authority, disposable references, schedule-only persistence. Retain original proposals for provenance. |
| Plan treats capture package presence as pipeline architecture | Identify current Qt Multimedia/SerialPort source versus GStreamer provisioning/requirements. Hardware and backend isolation remain unqualified. |
| Studio says no placement renderer and accepts unknown types ambiguously | Correct renderer status; unknown versions/types/actions rejected; only permitted inert extension metadata may be retained. |
| Backlog says keyboard absent / Web toggle merely a label | `qml/OnScreenKeyboard.qml` is registered and used by Store/Network Settings; `SearchSettings` persists toggle, Web ON remains a stub. Keep runtime acceptance open. |
| Fixed thirteen-suite language | Current `CMakeLists.txt` registers fourteen Linux tests, including `capture_protocol`; actual discovery/results govern each run. Historical thirteen-suite runs remain historical. |

## T04 architectural blockers — do not accept from UI labels

Focused review of `src/media/KodiImporter.cpp` at HEAD `6ff2bcb`:

- `kodiUserdataDir()` always selects the OS user's `~/.kodi/userdata`; profile-scoped state/backup naming does not itself isolate that input. Define explicit consent and source/target ownership before importing any real setup.
- `import()` declares source variables and `ok = true` but performs no transfer or binding to the managed Kodi runtime before reporting success. “Preserved by reference” is not an implemented handoff or round-trip proof.
- `createBackup()` creates only the backup parent before copying initial files into the timestamp child. It may fail before creating that child; directory copying is shallow, installed add-ons are not generally under `userdata/addons`, and there is no demonstrated consistent live-database snapshot. Backup completeness/atomicity needs explicit design and tests.
- `rollback()` uses `QFile::copy` onto existing destination files, which does not replace them, and restores only guisettings/sources. It does not implement the advertised complete atomic rollback of skin/add-ons/watch state. Do not test it against normal user Kodi data.
- Scan conflict text promises sources will be merged; import instead tells the user to configure Kodi sources natively. Resolve the operation's intended semantics, preview and success criteria before implementing it.

Keep T04.1/T04.2 unchecked. Require temporary-home/source fixtures, deterministic write-failure/partial-copy tests, explicit source/target validation, and real non-default skin/sources/add-ons/watch-resume hash round-trip before closing the gate. Existing nested-folder playback evidence does not cover these operations.

## VM observations and verification limits

Read-only VirtualBox/SSH inspection confirmed `Isolde-Test` running, NAT loopback 2222 → guest 22, Ubuntu 26.04.1, `isolde.service` active/running with NRestarts=0 and current release `161ea2e866cae3d1f982416e`. Guest CMake, Ninja, CTest and Python are installed. `/tmp/isolde-order-vsDgGY` exists but is not proof of source freshness. The Windows shell lacks discoverable CMake/CTest and its Python app alias was inaccessible. Initial sandbox VM/SSH denial was resolved by approved read-only execution; it was not a VM fault.

No service stop, restart, deployment, snapshot operation, import, rollback or graphical acceptance was performed by this audit. No current-source CTest, offline Generator Test, runtime network-denial or hardware pass is claimed. The procedure in [vm-oracle.md](vm-oracle.md) was checked against the actual harness/installer source and read-only guest configuration; its full end-to-end rerun remains pending. Historical `/tmp` evidence paths are references, not guaranteed durable artifacts.

The complete Studio/unified expansion is broader than the fourteen task summaries (for example variables/action sequences, motion/background resources and accessibility). This audit does not silently add implementation scope or claim those are all covered. Before declaring the entire product plan implemented, map remaining normative requirements to explicit task sub-gates and evidence; thirteen task percentages alone cannot prove untracked requirements complete.

## Documentation verification record

PowerShell audit checks on 2026-10-19: `git diff --check` exited 0; relative Markdown `.md` links in the eight audited Markdown documents resolved; checkbox recount returned T01 2/3=67, T03 3/4=75, T04 1/3=33, T05 2/3=67, T10 1/3=33, remaining tasks zero, overall 21%. These are documentation checks, not application test results. Read-only VM inspection exited 0 and observed the baseline recorded above. Application CTest and graphical acceptance were deliberately not run against the unidentified dirty source tree.
