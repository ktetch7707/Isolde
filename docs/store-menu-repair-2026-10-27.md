# Store Menu regression repair — 2026-10-27

User authorized fixing the Store blocker and continuing T11. Store feature progress was real; the integrated test exposed a specific navigation regression, not a rollback of its new package/update features.

The expanded Menu always exposed Android/F-Droid tools. On the Web tab, Menu → Confirm therefore toggled F-Droid instead of opening Add website. The first tool now matches the selected platform and uses the same `storeTool()` action as the visible button. That action restores the tools origin, so Back from the form changes nothing and Confirm can reopen it. Android's F-Droid toggle, update and setup actions remain available.

Independent `store_remote_ui` coverage exercises the real AppStoreView without a Page Studio fixture: Web Menu/form/cancel/reopen/name/address/save-once, no accidental F-Droid change, unchanged Android toggle, and Menu cancellation restoring its original area/header selection. Existing StudioSelection coverage is retained.

Delegated `store-menu-20261027-a` passes **1/1 suite**, with three behavior cases. Source digest `dc19a28328c1cb9e2da94b3bcb842de6388e2858f60bf761176289c207600682`; host evidence `build/vm-oracle/store-menu-20261027-a/report/`.

The clean candidate's apparent audio stall had a separate source-portability defect: executable Python fixtures had LF in Git but CRLF in the Windows checkout. The copied Linux scripts then had invalid shebang interpreter paths. The previously untracked minimal `.gitattributes` rules (`*.py` / `*.sh` with `eol=lf`) are now committed; no global Git configuration, host toolchain or audio behavior changed. Registered `audio_backend` scoped verification passes **1/1** in `audio-fixture-20261027-a`, using the same source digest and original timeouts. Evidence `build/vm-oracle/audio-fixture-20261027-a/report/`.

Both checks preserved normal service/release links, with zero automatic restarts. A fresh clean checkout and full CTest remain required to close the prior full-run blockers. No graphical, subscription playback, deployment, full T29 or full T11 acceptance is claimed by these scoped passes. T29 stays 1/8 = 13%; T11 stays 0/3 = 0%; sum 860 across 27 ACTIVE tasks gives Overall 32%.
