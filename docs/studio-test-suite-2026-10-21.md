# T19: focused Studio test suite setup

User requested unit/behavior tests with fast feedback between deployments, protecting the personal host. Added Qt Quick Test using the VM's existing Qt installation, six production-QML scenarios, focused registrations of seven existing data tests and four existing editor journeys, runner selection/allowlist regressions, and [coder instructions](studio-testing.md). No Cucumber runtime, host Qt toolchain, external service or system installation was introduced.

## Evidence

- `studio-tests-20261021-a`: new focused `studio_unit` passed in 1.25 seconds and `studio_behavior` in 21.07 seconds. QML compile rejected the fixture's reserved `baseline` property; renamed to `diskBaseline`.
- `studio-tests-20261021-b`: assertion failure correctly bundled `report/studio-test-size-slider.png`, which was opened and inspected. The pointer test clicked before its layout settled. Added condition/frame synchronization and drained deferred callbacks before destroying fixtures; this also prevented a previous scenario's delayed cancel callback from running against a destroyed preview in the next scenario. No production QML or assertion was weakened to make these tests pass.
- `studio-tests-20261021-c`: all six QML scenarios passed; CTest `studio_quick` took **7.14 seconds**. These durations exclude host snapshot/transfer/configure overhead and are measured observations, not guarantees.

Final full native run **`studio-test-suite-20261021` passed 17/17 CTest registrations in 130.89 seconds**, plus all **19 runner safety/cache tests**. Focused timings in that run: QML 7.01 seconds, unit 1.04 seconds, behavior 22.75 seconds; combined 30.80 seconds. Full suite includes the original 14 plus three focused registrations. Six new QML scenarios are separate function results inside `studio_quick`; registrations are not counted as unique scenarios.

Archive SHA-256 `77021cee46cb199ff4a5563052285f699f79231c17cb4ff2a320156beed9b6c4`; source digest `339ec2ff90555bd2a75a13b44e9f195a8805922418327614fe60c4fbe0f82672`. All six implementation/test configuration files match the frozen snapshot. Evidence: `build/vm-oracle/studio-test-suite-20261021/report/{ctest.log,ctest.xml,LastTest.log,oracle-self-test.log,test-result.json}`.

The recorded before/after service identities are identical: active/running, PID **1405231**, NRestarts **0**. Current release remains **`6d558525d9ead04d09f31aa9`** and previous remains `5064b3ae1002063c2d68e018`. The tested payload was staged by the full gate but never deployed.

No application runtime file was changed by this setup. The prior accepted 19-capture Studio/lounge/pastel graphical record remains [the scrolling delivery record](studio-scrolling-2026-10-21.md). No graphical run, release deployment or service restart is part of this test-infrastructure change. The pre-existing `src/home/PageData.h` working-tree change is included by the normal snapshot mechanism but not authored/staged by this setup.

T19.6 and other Studio feature gates stay open. Accounting unchanged: T19 83%, T20 80%, T21 36%; round(849/24) = **35% overall**, 24 active tasks.
