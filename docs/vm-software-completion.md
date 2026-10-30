# 100% means reasonable software confidence from the VM

**User scope revision, 2026-10-29.** This is the highest-priority acceptance definition for S1 and all 27 active T-tasks. It supersedes conflicting acceptance language in AGENTS, the backlog, task Entry docs, historical plans, fixture READMEs and evidence ledgers. The user will move the finished software to the actual hardware and perform their own testing after 100%.

**S1 / 100%:** the declared software scope is implemented, the shipped artifact works in a clean VM, relevant automated and VM runtime checks pass, the agent has reviewed its visible behavior, and no known required software defect remains. This establishes reasonable belief that the software is done. It is not a guarantee of zero bugs, universal platform compatibility, actual-device performance, third-party service availability or user satisfaction.

Every percentage-bearing acceptance criterion must have an explicit VM-provable pass condition. No actual TV, physical remote/controller/drive/capture/switcher, target-computer model, purchased account, entitlement, provider-region success, user appearance approval or independent human participant may block S1. A requirement that depends on those is redesigned below, not left as an impossible software gate. The separate handoff checklist is outside all 27 weights and percentages.

## Completion evidence, without endless gates

Use the existing task-level full-suite budget. These are final evidence categories, not demands for a full suite at every step:

1. Implement the promised in-scope workflows and their visible errors/cancel/retry/focus recovery. Configuration remains validated declarative data, isolated by profile and atomically saved. Known data-loss, security or functional defects remain software work.
2. Exercise production code with focused automated tests, including relevant malformed input, write/crash/cancellation/profile/late-callback boundaries. Use virtual devices and deterministic service fixtures only at external boundaries. Do not mock the feature being accepted or replace real renderer/session/store behavior with a success stub.
3. Run actual shipped application and available dependencies in the VM for core end-to-end journeys: edit/apply/reopen, local Kodi playback/return, local search/Guide, app/game session ownership, offline behavior, install/update/rollback/recovery. Observe actual output, not merely process spawn or a request acknowledgment. For dependency-specific VM limitations use the explicit substitution rules below.
4. Agent-review actual screenshots for focus, clipping, contrast, consistent state and both reference recreation/reset pairs. Named tasks, expected outcomes and annotated results replace representative usability research. No user approval or subjective exact aesthetic matching is required for 100%; obvious defects or missing required generic components still need fixes.
5. T31 freezes one final source/artifact, runs the full suite, installs it in a fresh VM and verifies core integration plus failure/recovery/offline/profile isolation. Publish hashes, evidence, supported matrix and the user's hardware/service checklist. A developer-checkout-only demo is insufficient.

If something can be exercised in the VM, actually exercise it at an appropriate scope. External uncertainty must not be mistaken for a known implementation defect. A current failing software route cannot be moved to the handoff list merely to make progress green.

## Mandatory substitutions for non-VM gates

| Former blocker | S1 replacement that can be proven in the VM | User handoff only |
| --- | --- | --- |
| Uncoached user/representative, separate reviewer, user visual approval, TV-distance usability | Agent executes named tasks through visible product controls with permitted remote keys/pointer; record expected result, focus/origin, no hidden config surgery, errors/cancel and captures. Agent may review their own output; independent participant not required. | Personal usability, appearance preferences and actual TV-distance comfort |
| Real remote/CEC/Xbox controller/hotplug/TV/GPU/audio/capture/RS-232/eight-input switcher | Exercise production input/adapters/settings with virtual input/OS devices or protocol boundary fixtures; verify mapping, capabilities, timeout/disconnect/reconnect/recovery and actual application state. Measure VM performance only. | Physical compatibility, real signals/latency/sync, controller feel and target performance |
| Actual CD/DVD drive/disc or hardware eject/mount | Virtual optical images/mount identities and adapter fault fixtures exercise production install/play/ownership/changed-media/cancel/eject flows. | Real-drive formats, insertion/eject, multi-disc and damaged-media behavior |
| Specific host/test-computer firmware, USB creation/boot and destructive real-disk install | Build a VM-bootable artifact, boot/install to a disposable virtual disk, reboot/recover/rollback; deliver general USB/disk safety instructions with verified product screens. No unknown model blocks completion. | User's actual firmware/USB tool/boot menu/disk selection and real power loss |
| Actual subscription/DRM login/entitlement, paid VPN country exit, provider geo-unblock or service uptime | Production adapter and authentication/lifecycle/error contracts exercised with controlled account/region/DRM/HTTP fixtures; real account-free VM smoke when available. Verify user-visible unsupported/auth/region states, cancellation, credential isolation, no unwanted requests and rollback. For VPN prove routing/DNS/leak/rollback mechanics in an isolated virtual topology. | Owned account sign-in/content, actual CA/US exit IP, CBC/Disney+/other entitlement and provider compatibility |
| GPU/firmware/legal-media-dependent emulator or runtime unavailable in the VM | Declare bounded supported routes; test actual provisioner/identity/version/path/config/session/save contracts, virtual input and honest missing-dependency/error recovery. Run a lawful playable VM-capable positive fixture through the common integration path. Document precisely which per-runtime rendering/gameplay assertions could not run. | Target GPU/API/controller/firmware and per-platform gameplay/performance beyond VM capability |
| Architecture/provider/model choice requires user involvement | Agent chooses a documented conservative implementation within existing scope/dependencies, or a pluggable contract plus deterministic boundary fixture when the live service requires unavailable secrets/entitlements. No paid account, new host toolchain or fabricated live result. | User's optional provider/account/model and unsupported external constraints |
| Twelve-hour real wait as the only proof of outage continuity | Inject elapsed time through the production clock boundary, test twelve-hour/seven-day/clock-shift invariants, and perform a real stopped-process cold reopen with persisted schedule/seek and offline app use in the VM. A long real-time soak is optional extra confidence. | Optional extended soak and actual-device suspend/power behavior |

Fixtures must specify their external boundary and expected supported/denied responses and drive the actual production adapter. Network fixtures do not prove third-party service acceptance; virtual I/O does not prove hardware compatibility. State those limitations in the final handoff, with no software percentage penalty once the VM replacement passes. Do not silently call a missing adapter or an unimplemented advertised workflow “external.”

## Task-by-task audit and VM gate mapping

All task weights and milestone weights remain fixed. No parent or child is automatically marked done by this redesign. Apply these mappings to **every** linked requirement, not just a gate whose name happens to mention hardware.

| Tasks | VM completion boundary / redesigned criterion |
| --- | --- |
| T01, T05 | Exact-source build, generic geometry, actual agent-reviewed VM fixtures and reset; no user aesthetic sign-off or physical display comparison |
| T03 | Production search with actual local Kodi fixture, paging/cancel/return and network denial in VM |
| T04, T09 | Actual VM Kodi setup/browse/library/playlist/artwork/playback/controls/return; disposable existing-install import preservation and rollback; provider content outside VM uses explicit boundary fixtures |
| T10 | Production adapters/protocols and shared editors through virtual-device success/error/disconnect and actual application state; real HDMI/audio/serial measurements excluded |
| T11 | Actual VM Guide/source/local playback/transition/repair and offline cold reopen; deterministic elapsed-time durability replaces required twelve-hour wait; agent-reviewed source/art/portability/remote tasks replace representative approval |
| T12 | VM system settings/input/keyboard/session/idle/profile/error workflows; virtual network/VPN topology and provider boundary fixtures replace paid CA/US/CBC or subscription account requirements |
| T13 | VM-bootable shipped artifact, clean virtual-disk install/reboot/recovery/offline/upgrade failure, illustrated VM-validated user guide; no actual-computer specifics or participant dependency |
| T14 | Implemented discovery/subtitle contracts and UI, boundary provider responses, OFF cancellation and actual local/offline SRT output/queue/review/cancel; external keys/paid model availability not required |
| T15 | T15.4 becomes recorded agent-run task walkthrough against actual installed VM UI and design requirements; no representative needed |
| T16, T17, T18, T19 | Existing software foundations remain; any physical/user-review claims in linked docs are excluded from S1. Reopen only actual regressions |
| T20, T21 | Production renderer/data/input/parity, actual fixture providers, remote/pointer edit/focus/overflow/atomicity and agent-reviewed VM presentation; history provider unavailable live is tested at its boundary |
| T22, T23 | Actual template/token/portability/preview/apply/restore data and VM workflows, malformed assets/actions/profile/atomicity failure tests; no user approval required |
| T24 | Recorded agent-run installed VM discovery/task checklist, accessibility/scaled rendering/performance/offline and cutover; no uncoached participant |
| T25, T26, T27, T28 | Production source/container/catalog/theme/search models and visible VM workflows, injected roots/registry/late callbacks, profile/OFF network invariants; no physical plug-in or provider account needed |
| T29 | Actual available VM Store/install/open/return/remove/update and browser input plus platform boundary fixtures for unavailable runtime/entitlement/content cases; SmartTube real content if available, deterministic playback/session/error integration otherwise with honest handoff note |
| T30 | Supported route/runtime catalog, actual common VM gameplay positive fixture, production provision/config/import/install/save/session/return and virtual optical tests; target-only per-runtime GPU/controller/media requirements become user checklist items, no claim of universal console coverage |
| T31 | Frozen final clean-VM integration with honest boundary fixtures and limitations; agent task/guide walkthrough and source-tied risk review are sufficient; no separate human, target machine or external subscription prerequisite |

## Guide and walkthrough pass conditions

T13.4 delivers `docs/user-setup.md` in the artifact/offline package. It explains obtaining/checking the image, general USB preparation and disk-erasure safety, product boot/install/first-run screens, profile/library/input/display/audio/network setup, local playback, updates/rollback and recovery. Use actual VM captures, exact product control names, expected results, waits and symptom-based troubleshooting. No default passwords, placeholder product steps or developer build/config/terminal prerequisites in the ordinary product journey.

T31.7 is a recorded **agent** walkthrough using only the guide and shipped artifact on a clean VM: boot virtual media, choose disposable virtual disk, install/reboot, set up a local library, play and return, then exercise one documented recovery. Record guide/artifact versions, steps/results and any missing/incorrect instruction; correct failures. This establishes reasonable documentation/software confidence, not independent-human usability. Generic USB/firmware steps are reviewed for consistency and safe consequences; actual host tools/model/USB/disk actions are tested by the user after S1.

## Handoff outside the software percentage

Create `docs/hardware-handoff.md` as part of final T31 evidence, listing the exact artifact/hash, VM environment, tested/fixture-only/untested boundaries, supported runtime matrix, recovery route and concise checks for the user. Preserve H01–H05 as user-owned equipment checklists; add account/service and personal-usability items there as needed. None is a dependency of any active T-task or S1. No requirement that the user conduct a formal H1/R1 campaign is imposed by this software backlog; public-release decisions are separate from this task.

Hardware/service findings may reveal genuine software defects later. Record a regression against the affected software milestone; the earlier 100% remains a statement about the frozen candidate and evidence available then, not a promise of infallibility.

## Acceptance authoring rule

For every new or existing child, name the VM setup, production code under test, stimulus, observable result and bounded evidence. If a proposed criterion depends on the user, real equipment, unavailable account/service or target-only capability, use the mapping above and record the residual user check. Do not ask the user to waive an old gate already superseded here, do not mark the old non-VM experiment as passed, and do not introduce another impossible prerequisite in an Entry doc. Scope changes alone earn no completion credit; the redesigned VM test must still pass.
