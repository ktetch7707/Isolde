# VM runner verification — 2026-10-20

User-requested testing maintenance; no Page Studio implementation or backlog completion credit is claimed. Existing uncommitted T21 application work was preserved. Canonical instructions: [vm-oracle.md](vm-oracle.md).

## Commands and evidence

Run from the repository root in PowerShell 7. Evidence paths below are local, ignored build artifacts; do not commit generated profiles/caches/cookies.

| Run | Command | Result |
| --- | --- | --- |
| `oracle-tooling-preflight-20261020` | `./tools/vm-test.ps1 Check -RunId oracle-tooling-preflight-20261020` | Connection/tools/disk/service check passed; no release change. |
| `oracle-tooling-cold-20261020` | `./tools/vm-test.ps1 Test -RunId oracle-tooling-cold-20261020` | First Linux cache population, 223 build steps, 14/14 native CTest suites passed; complete payload staged; 322 seconds. |
| `oracle-tooling-warm-20261020` | `./tools/vm-test.ps1 Test -RunId oracle-tooling-warm-20261020` | Cache reused, no C++ compile/link, 14/14 suites and 16 Linux runner regressions passed; complete payload staged; 91 seconds. |

Each run's `build/vm-oracle/<id>/report/` contains status, stage logs, CTest results and payload identity. Cold/warm durations are status timestamps, including full CTest and staging, excluding host packaging/transfers.

Warm snapshot archive SHA-256: `fd2a59b66b721bf6dc014351dfba1f6bbb20c01284b84dfeb3469c8d2d5a83e9`. Source digest: `312d52c71aa058f4d9ffdd8b378d136f5cb1de020ad4664d64a9e57b14cf6a06`. Driver SHA-256: `c060878d35863b110bb25f72a8926a094fc8cc1f17144e540f9c6e0ed81676de`.

## Failure/recovery test

Intentionally invoked the guarded Studio capture against the warm staged payload with a nonexistent VirtualBox capture target. Expected screenshot failure occurred after `CATALOG_READY`. The host cancelled the verified guardian; the guardian terminated its test process group and restored the normal service to active/running with zero restarts. No broad process kill, lock deletion, VM reset or repeated build was used.

Normal layout SHA-256 remained `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`. Installed links remained current `4e78f5b019a4f554a4c6c450`, previous `4ac297501a5e0dc03384cc84`. Diagnostics: `build/vm-oracle/oracle-tooling-warm-20261020/fault-capture/`. Latest-action Status correctly reported failure; the separate passing full-test result remained available for a same-run Visual retry.

## Local regressions

`tests/test_vm_oracle.py`: original 16/16 passed on Linux; final 17-test version passed locally with five Linux-only skips. Covers immutable archives, unsafe extraction, incremental timestamp preservation, bounded sync deletion, report/evidence allowlists, targeted-test deployment refusal, service restoration, verified cancellation, silent deadlines and lock exclusion.

Live Visual validation found a result-contract mismatch: the reference harness uses `component_support`, `reset_verified`, skin round-trip and empty `missing`, rather than Studio's `passed`. The runner stopped and restored the service. The fix checks every required reference field explicitly, with a regression rejecting missing fields and a bare `passed`. Final driver SHA-256: `0d6bc355a89921c8ab334e974aea4fbbde89fa5674954007ddd908539b0d3b6f`. Only the driver/result regression changed; the passing staged application was reused without compilation.

`tools/test-vm-process.ps1`: passed stdout/stderr draining, exit status, silent deadline, literal argument preservation, capture callbacks and callback-failure cleanup. PowerShell parser checks, Python compilation and scoped whitespace checks passed.

## Successful graphical runs and deployment

`./tools/vm-test.ps1 Visual -RunId oracle-tooling-warm-20261020` passed Studio (11 captures); after the result-contract fix, `-Cases lounge,pastel` passed both recreation/reset pairs (four captures). Every image was viewed. Studio controls/focus were visible; the lounge adapts the portrait reference into a wide composition; pastel preserves its two icon rows and translucent shelves. This records renderer/test functionality, not user design approval. Normal profile hashes and release links were preserved by each graphical run.

Captures: `build/vm-oracle/oracle-tooling-warm-20261020/visual/studio/attempt-20261020-205104-189/`, `visual/lounge/attempt-20261020-205405-750/`, and `visual/pastel/attempt-20261020-205455-132/` under the same run. Earlier failed attempts remain present.

`./tools/vm-test.ps1 Deploy -RunId oracle-tooling-warm-20261020` passed in about three seconds including host transport/report retrieval; activation/restart itself took under one second. The running executable resolved to `~/.local/share/isolde-appliance/releases/237dd5ba7b8b534fc4ec88f6/bin/isolde`, current points there, previous is `4e78f5b019a4f554a4c6c450`; service active/running, zero restarts. See `report/deploy-result.json`. No repeat compile/CTest or startup edits occurred.

`./tools/vm-test.ps1 Visual -RunId oracle-tooling-warm-20261020 -Installed -Cases lounge` also passed, verifying that exact selected executable. Both installed recreation/reset captures were viewed at `visual/lounge/attempt-20261020-205606-609/`. Normal service was restored active/running with zero restarts; layout hash remained unchanged. Studio/pastel used the byte-verified staged payload; the installed lounge check exercises the explicit installed-release path without repeating all native tests.

## Limits

This verifies the testing machinery and exercises existing application tests. It does not accept T21 product design, fix its separate review findings, validate physical HDMI/CEC/audio, or replace the disconnected cold-boot Generator Test. Screenshot capture is never automatic visual acceptance; every generated image must be viewed.
