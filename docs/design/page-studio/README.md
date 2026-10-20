# Page Studio design pack (T15)

Status: approved interaction design, 2026-10-19. These artifacts satisfy backlog task T15
([agent-backlog.md](../../agent-backlog.md)) — the design-validation gate that must close before any
Studio implementation task (T16-T24) begins. They are the "T15 approved wireframes and feature map"
referenced by T18-T24 entry docs. Specification authority remains
[page-studio-redesign.md](../../page-studio-redesign.md) (core Studio) and
[shell-surfaces-design.md](../../shell-surfaces-design.md) (T25-T28); this pack adds the reviewed
screen/state set, the old-to-new feature map and the recorded usability review. Where this pack and
the specifications disagree, the specifications win and this pack must be revised.

## Contents

| Artifact | Backlog item | What it establishes |
| --- | --- | --- |
| [feature-map.md](feature-map.md) | T15.1 | Inventory of every currently supported editor operation plus the source-notice / All Apps / themed-surface / Web-switch shell surfaces, mapped to its Studio destination, with capability gaps called out from code evidence rather than UI labels |
| [wireframes.md](wireframes.md) | T15.2 | Reviewed TV (remote) and desktop (pointer) wireframes for every Studio screen state: blank/existing page, Add, selection, inspector, pick-up/drop, resize, collision, dirty exit, preview, apply error and nested scope |
| [interaction-states.md](interaction-states.md) | T15.3 | Complete D-pad/Confirm/Back/Menu state model, field editing, focus restoration, error feedback and the accessible (non-drag) movement alternatives |
| [review-and-acceptance.md](review-and-acceptance.md) | T15.4 | Recorded uncoached user walkthrough, confusing-transition resolutions and the measurable acceptance journeys that gate T24 cutover |

A static coverage check for this pack is `tools/verify-t15-design.py` (no Qt build required); its
committed run log is [verify-t15.log](verify-t15.log).

## Standing rules carried into this pack

- Layout is validated data, never executable QML/JS/shell; the default must be TV-polished with no
  configuration required (AGENTS.md §8-9).
- Every workflow completes with D-pad + Confirm + Back + Menu/Search only; desktop
  drag/inspector produces byte-identical layout data (AGENTS.md §10).
- Draft-only editing until Apply; opening the editor, preview and cancel write nothing; atomic
  save/rollback with the previous applied design retained on error.
- Profile isolation: edits affect the active profile only; teardown invalidates draft and clients.
- Kodi owns media/progress/playback/artwork; Isolde owns presentation/navigation. The catalog must
  not invent entitlements or perform discovery beyond existing bridge bounds.
- Both standing visual fixtures (home-reference, home-pastel-reference) remain pass gates for every
  later Studio slice; nothing here special-cases either fixture.
