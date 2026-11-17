# Moonlight update TODO

Last reviewed: November 16, 2026. This is a review and integration queue, not approval to apply every listed commit unchanged.

## First batch — November 13, 2026

- Settings-navigation repair finalized in local commit `a4cc6e8e`. The global-settings QML layout import and StackView-owned profile editor page are covered by the profile/navigation suite.
- Applied in local commit `9c2aa36f`: toolbar layout feedback fix (`4113dd26`), keyboard-selected truncated-hostname tooltip support (`029da3fb`, retaining our existing smaller font and custom cards), CLI quit label cleanup (`67dbfddb`), renderer failure initialization (`465c41b5`), D3D11 swapchain state rename (`ba700d67`, including Nonary's fallback path), and DXVA2 failure cleanup (`d606709d`).
- Already equivalent: adaptive-trigger bound (`9fee675c`) was implemented by existing Nonary commit `72b936173`; no redundant code change was made.
- Clean release build succeeded using Qt 6.11.1 and MSVC 14.52 in `build/batch1-x64-release`. This fresh tree avoids mixing older MSVC-generated objects with the currently installed compiler. It does not change which source branch is built.
- Freshly built regression suites: 25 profile/navigation checks passed; 15 startup-recovery checks passed. Tests require normal Windows temporary-file/lock access; an initial sandboxed baseline run failed because that access was blocked, then passed outside the sandbox.
- Standalone Material toolbar regression: 3 QtTest checks passed with the fixed production component. The same harness failed promptly against a throwaway pre-fix component from `a4cc6e8e` (icon dimensions escaped the bound), establishing regression sensitivity. The harness forces the real Material nonsquare branch with text; it does not simulate physical monitor DPI. Source and run instructions are in `tests/gui`.
- All three submodule pointers and the full Git status were verified with normal Windows access; submodules remain unchanged.
- `build/deploy-x64-release` now contains the new executable, freshly built AntiHooking DLL and matching MSVC runtime DLLs. Prior overwritten binaries were preserved in `build/batch1-previous-deploy`; existing portable settings were retained. The deployed executable's SHA-256 matches the release build. Its isolated headless license smoke check completed with output and no stderr.
- Both Luna Max product implementations needed no corrections. They adapted the hostname improvement to the custom cards and updated all fork-specific D3D11 references. The GUI regression test needed review-driven harness corrections (a visible test surface and explicit nonsquare Material content) before its old/fixed comparison was useful. Independent Sol Medium final review of product changes and the test found no actionable correctness issues. GPU error recovery, HDR transitions and physical mixed-DPI monitor movement remain interactive validation items.
- No version bump, MSI rebuild or GitHub push in this batch. Remaining checklist work is still pending.

## Second batch — November 13, 2026

- Applied in local commit `8ce39873`: pairing/key/certificate validation (`a6149245`, `2bd4b9f7`, `ee557e83`), TCP/TLS connection timeout and Qt-version-specific connection caching (`74173b26`, `781b26fd`), and safe access to incomplete host version fields (`0c4f7aef`). The optional injected network manager, pinned-certificate checks and Nonary launch extensions are preserved.
- The same commit includes the H.264 arithmetic and writer bounds fixes (`fa6ae809`, `289782c6`, `9b8886a6`, `c8474c51`), original-packet fallback when SPS fixup fails (`6fc7e816`), restored vendored source headers (`a57e947d`) and normal compiler warnings (`71c3191e`). Existing read-side array clamps remain unchanged. No renderer threading, VRR policy, core submodule or dependency updates were mixed into this batch.
- Added backend loopback and bitstream boundary suites to the normal qmake test gate. Clean Qt 6.11.1/MSVC 14.52 release build succeeded in `build/batch2-x64-release`; 5 backend QtTest checks (3 behavioral cases), 25 profile/navigation checks and 15 startup-recovery checks passed. All five bitstream boundary groups passed with release-active checks.
- Independent Clang 22 undefined-behavior sanitizer run passed with `NDEBUG` defined. The identical boundary suite against a throwaway copy of the old `1ffc3e18` bitstream sources reproduced `INT32_MAX + 1` signed overflow and failed as expected. These runs exercise actual production parser/writer code with independent known encodings and guard bytes.
- Independent Sol Medium final audit found no concrete regressions in production code, test fixtures, test wiring or instructions. Luna Max's product edits required no corrections; test-project integration needed minor moc/header and macOS OpenSSL guard corrections during parent review. No claim is made that the new certificate/challenge error branches or FFmpeg fallback have direct runtime unit coverage; those changes were source-reviewed. Real-host pairing, macOS and Qt 6.12-specific runtime behavior remain interactive/platform validation items.
- A separate portable test package is at `build/deploy-batch2-test`. Qt deployment succeeded, the packaged executable matches the release-build SHA-256, and the isolated headless smoke exited 0 with no stderr. `build/deploy-x64-release` and installed copies were not replaced by this batch.
- The build uses the user's local `7.1.0.0` version and unused-timestamp cleanup. Both user files remain unchanged from the start of this batch and uncommitted. No MSI was rebuilt and nothing was pushed to GitHub.
- Remaining focused candidates include CLI polling ownership, input/keypad changes, exclusive log creation and usable-window bounds. Installer warning cleanup/upgrade testing and the larger concurrency/Nonary/core/color stages remain deferred as listed below.

## Third batch — November 14, 2026

- Committed the user's unused `waitStartUs` deletion separately as `c4d5a240`. The local `app/version.txt` remains at the user's uncommitted `7.1.0.0`; its contents were preserved throughout this batch.
- Applied in local commit `03e045e7`: GUI-owned versus CLI-owned polling (`74020b53`), exclusive/private log creation (`8f2e52e5`), usable-window-bounds fallback (`d79536b5`), main-thread application quit (`aad0090d`), known Steam Controller identification (`d2f6990b`), and the SDL/libdecor/Qt startup livelock fix (`e3fd29e4`). Log names include a PID so simultaneous app instances do not contend for the same second-based name; existing logs are never truncated. The usable-bounds change is not proof of a fix for the unreproduced Windows 1080p report.
- The same commit adapts Nonary modal controller-focus (`d0b9edb9`) and dialog-first Back/Escape (`f1bd296e`) changes. Our external-settings and unsaved-settings dialogs cancel pending actions on rejection; canceling does not implicitly acknowledge or overwrite external settings. Standalone settings test surfaces remain supported without a `stackView` context.
- Renderer/concurrency backports: Pacer overlay ownership (`10270ca0`), render/HDR/window synchronization (`1ced9dd9`), empty pacing queue checks (`e4db5c2c`), DX V-sync errors/fallback (`9af0e9f5`), and missed wakeups in Pacer and ComputerManager (`fe5a0aea`). Existing Nonary overlay locking is already equivalent to `017b6039`; no redundant lock was added. Registration starts after renderer preparation and detaches before worker joins. Legacy rendering shares a context mutex; VRR keeps the backend presentation/deferred-window protocols rather than holding a generic lock over target waits. VRR context cleanup now runs on its owning worker after preparation stops and queued tickets are discarded.
- Added CUDA unsupported-format/unbound-texture guards (`aa54ba2a`, `a6276aa8`) and libplacebo swapchain/decode-queue initialization guards (`ac296864`). No streaming-core, color-policy or dependency pointer changes were included. In total, 16 selected upstream/fork changes were applied or adapted and one selected change was already equivalent.
- Qt 6.11.1/MSVC 14.52 release build passed in `build/batch3-x64-release`, including a final rebuild after worker cleanup ordering was finalized. Fresh profile/navigation (25), startup recovery (15), backend (5), lifecycle helper (6), and Material toolbar (3) QtTest checks passed. Five bitstream boundary groups passed. Dialog/controller-navigation tests passed under Basic and Material (33 checks each). Standalone overlay callback-lifetime tests and the full VRR pacing-worker executable passed, including the new cleanup thread/order regression. The overlay executable requires the `app` directory as its font-path argument; an initial invocation omitted that required argument and was rerun correctly.
- Luna Max produced useful fork-aware implementations. Parent review prompted PID-separated log naming, standalone-dialog guards, explicit deferred-action cancellation, careful VRR lock scope and a stronger cleanup-order assertion; those refinements were incorporated before final validation. Independent Sol Medium's completed-diff audit found no actionable issues. Native GPU presentation, actual controller hardware, Linux startup, resize/move, reconnect and SDR/HDR transitions still require platform/interactive testing.
- Separate portable package: `build/deploy-batch3-test`. Qt deployment and the isolated headless license smoke passed (exit 0, no stderr); packaged executable SHA-256 matches the final release executable. Existing deployments and installed copies were not replaced. No MSI rebuild or GitHub push was performed.
- Still deferred: keypad Enter requires the missing `MODIFIER_EXTENDED` core definition; larger Nonary VRR/PyroWave/core work, color/dependency updates, other platform fixes and MSI upgrade-warning cleanup remain separate stages below.

## Versioned MSI series — November 16, 2026

- User approved two cumulative MSI builds, **7.1.1.1** and **7.1.1.2**, with small reviewed integration steps inside each release. This supersedes the earlier instruction to hold `version.txt` uncommitted. Starting at 7.1.1 avoids a downgrade/uninstall from the previously used 7.1.0 build. Source/version notes are committed at each boundary; MSI archives and adjacent source/checksum manifests are retained in the project root, with manifests also committed after packaging. No installation or GitHub push is part of preparation.
- First release source increments: `58ae53b9` adapts Nonary host warnings (`3ae5f3b0`, `d54e6c04`); `1b13be23` adapts confirmed paired-host sleep (`696afb07`) and its Windows test headers (`cfef0e8d`). Each increment passed its Windows release build and relevant automated gate before the next step. Detailed scope, tests, delegation review and limits: [7.1.1.1](releases/7.1.1.1.md).
- The first MSI is archived as `MoonlightSetup-x64-7.1.1.1.msi`, source `13b2c100` / tag `codex/build-7.1.1.1`. Extraction verified 1,426 payload files, executable hash equality, runtime presence and absence of user settings/portable markers; the extracted app passed its isolated headless smoke. [Committed artifact manifest](releases/manifests/7.1.1.1.json). The existing four ICE61/ICE69 warnings remain; no native installation was performed.
- First MSI retains the old core `d6a11bc`; the second will take fork-compatible sync `4c05b34e` with the MSVC atomics companion `f586143c`, and input relocation/keypad changes `b75fe0d9`/`c0f62ad6`. The broader VRR redesign, optional new PyroWave negotiation, color/dependency refresh and remaining platform items are still separate work.
- Preparation guidance and artifact index: [versioned-test-builds.md](versioned-test-builds.md). Existing root MSIs, host logs and crash-test folder remain preserved.

## Baseline and scope

- Starting branch: `codex/startup-recovery`, starting HEAD `62154d27`. Our base includes Nonary VRR18/PyroWave, per-PC profiles, multi-instance settings coordination and GUI startup recovery. First-batch commits are recorded above.
- Preserve existing settings-navigation changes in `app/gui/AppSettingsView.qml`, `app/gui/main.qml`, `tests/streamprofile-tests.pro` and `tests/test_streamprofiles.cpp`.
- Original Moonlight review endpoint: [`29f121a7`](https://github.com/moonlight-stream/moonlight-qt/commit/29f121a7d33dacf200fa14840685a3f4534a1f10). Last cached endpoint was `dc92b6d2`; 40 newer commits were inspected at the inventory level.
- Nonary review endpoint: [`f586143c`](https://github.com/Nonary/moonlight-qt/commit/f586143cf1589d513fb91f7fbc64157b2fa742d6), daily.200. There are 39 commits, including merges, after our October snapshot `b35ce457`; that is not the total missing since VRR18.
- Nonary's latest stable release is still VRR18, and `release/6.1.0-vrr18` remains `43225b52`.
- The original-upstream inventory below accounts for all 103 commit IDs in the starting reviewed history difference: 63 from the earlier backlog and 40 newer commits. At that snapshot five were already backported; subsequent dispositions are recorded above and checked below. The other entries require patch-equivalence and dependency checks; they are not 98 proven missing or independently applicable fixes.
- Commit inventory coverage is complete for this pinned original-upstream range. Full implementation review, compatibility validation and testing remain to be done. Nonary work is grouped by subsystem below, not inventoried commit by commit.
- Do not merge original `upstream/master` wholesale. Preserve profiles, settings coordination, startup recovery, VRR, PyroWave and our build/version conventions.
- Record each eventual outcome as Applied (with local commit), Already equivalent, Deferred (reason), or Inapplicable (reason). Checkboxes here mean that disposition and its necessary validation are complete, not merely that a cherry-pick ran.

## 0. Finish the existing local repair

- [x] Review and finalize the pending global-settings/profile-navigation repair. Local commit `a4cc6e8e`; fresh 25-check suite passed and independent review found no issues.
- [ ] Verify global gear, per-PC settings, repeated enter/back navigation, and profile save/reopen behavior on the intended portable build.
- [x] Preserve the user's MSI, crash-test directory and host diagnostic logs. Keep Vibepollo host display troubleshooting separate from Moonlight client integration.
- [x] Capture the starting branch/submodule/build state before integrating. Full status and submodule pointers verified outside the sandbox after metadata access was denied inside it.

## 1. Focused correctness and usability batch

- [x] Fix mixed-DPI toolbar resize feedback (`4113dd26`); include the long-hostname display/tooltip improvement (`029da3fb`) if compatible with our computer cards. Local commit `9c2aa36f`; focused toolbar regression passes and detects the old code.
- [x] Review CLI launch polling ownership (`74020b53`) and undefined quit-progress label (`67dbfddb`), preserving our settings guards and startup recovery. Label: `9c2aa36f`; polling ownership: `03e045e7`.
- [x] Review renderer failure-reason initialization (`465c41b5`), D3D11 swapchain/base-class color-state shadowing (`ba700d67`), and DXVA2 failure cleanup (`d606709d`). Applied, independently reviewed and built; GPU-specific interactive checks remain recorded above.
- [ ] Review adaptive-trigger controller bounds (`9fee675c`), known Steam Controller identification (`d2f6990b`), and Enter versus keypad Enter (`c0f62ad6`, with its required core/protocol changes). Bounds are already equivalent through `72b936173`; identification is applied in `03e045e7` using the existing core's `LI_CTYPE_STEAM`. Keypad remains deferred until the fork-compatible core provides `MODIFIER_EXTENDED`. Treat `b75fe0d9` as context/dependency, not a feature to pull for its own sake.
- [x] Integrate applicable pairing/key/certificate checks (`a6149245`, `2bd4b9f7`, `ee557e83`). Applied in `8ce39873`; source review, release build and backend tests passed, with direct branch-coverage limits recorded above.
- [x] Integrate applicable bitstream/version parsing fixes (`fa6ae809`, `289782c6`, `9b8886a6`, `c8474c51`, `0c4f7aef`, `6fc7e816`); retain source headers and useful warnings (`a57e947d`, `71c3191e`). Applied in `8ce39873`; boundary/UBSan and backend suites passed; FFmpeg fallback remains source-reviewed only.
- [x] Review TCP/TLS timeouts (`74173b26`) and Qt-version-specific connection caching (`781b26fd`). Applied in `8ce39873`; loopback tests verify stalled TLS times out and a sent request may receive a body after 5 seconds. Qt 6.12-only semantics remain platform validation.
- [x] Review exclusive/private log creation (`8f2e52e5`), especially shared temporary directories; adapted to PID-separated diagnostic/log naming and tested in `03e045e7`.
- [x] Review usable-window-bounds fallback (`d79536b5`). Applied and helper-tested in `03e045e7`; platform/1080p reproduction limits recorded above.
- [x] Run targeted build/regression checks for the changed paths and verify the produced portable directory. Batch 3 has a separate reviewed commit and portable test directory.

## 2. Concurrency and pacing correctness

- [x] Review the overlay lifetime group together: `017b6039`, `10270ca0`, `1ced9dd9`. Equivalent callback lock retained; ownership and render synchronization adapted in `03e045e7` with overlay/worker lifecycle tests and independent review.
- [x] Review empty pacing queue checks (`e4db5c2c`) and DXVsyncSource error handling/fallback (`9af0e9f5`), including required interface changes. Applied in `03e045e7`.
- [x] Review missed-wakeup fixes (`fe5a0aea`) in both ComputerManager and pacing. Applied in `03e045e7`; interruption/wakeup and predicates now share their mutex.
- [x] Review main-thread application quit (`aad0090d`) against our close/save confirmation handling. Applied in `03e045e7`; existing settings-close logic retained.
- [ ] Verify stream startup/shutdown/reconnect, active overlays, resize/move, and SDR/HDR transitions. Exercise both conventional pacing and VRR; use meaningful automated lifecycle checks where practical, then user GUI/stream testing.

## 3. Nonary post-VRR18 and streaming core

- [ ] Review Nonary's full post-VRR18 difference to the pinned endpoint, checking for fixes already represented locally before selecting the integration boundary.
- [ ] Review PyroWave calibration quality, bandwidth/FEC budgeting, loss/shimmering behavior, UDP completion reporting and D3D11 import fixes as related changes with their tests.
- [ ] Review the newer Windows native timing/waitable-DXGI behavior and the VRR buffer/goal-solver revisions as a subsystem. Include subsequent smoothing/recovery/accounting fixes through daily.200 rather than isolating an early redesign commit.
- [x] Review dialog controller-focus and dialog-first Back/Escape handling (`d0b9edb9`, `f1bd296e`), preserving our custom dialogs and settings-access checks. Adapted in `03e045e7`; Basic and Material regression suites passed.
- [x] Review nonfatal host warnings (`3ae5f3b0`, `d54e6c04`). Applied in `58ae53b9` for 7.1.1.1; parser/overlay tests and independent review passed. These improve diagnostics and do not repair host display topology.
- [x] Review paired-host sleep actions (`696afb07`) as an optional user-visible feature, including confirmation/capability behavior. Applied in `1b13be23` for 7.1.1.1; loopback TLS and real-QML tests passed, with live sleep/wake remaining interactive validation.
- [ ] Review DualSense waveform/speaker and Steam Controller 2026 support as separate input features, including device lifetime and core ABI dependencies.
- [ ] Review Nonary's upstream streaming-core sync (`4c05b34e`) and companion MSVC C11 atomics change (`f586143c`). Prefer the compatible Nonary core over substituting original upstream's submodule pointer.
- [ ] Reconcile the older frame-loss backlog (`14c26d8c`) and later core updates (`dc92b6d2`, `8016954c`) against that synced fork. Our current core `d6a11bc` diverges from upstream frame-fix pointer `62e0663`; ancestry alone does not establish equivalent patch presence.
- [ ] Verify per-PC persistence/equality/copy paths for every new setting, plus settings concurrency, PyroWave framing/transport/decoder tests, applicable VRR tests, a consistent-toolchain release build and portable deployment.
- [ ] Have an independent review of the integration and report any delegated-development corrections/quality as requested. Existing development preference: Luna Max; if that is insufficient, consider Sol Low/Medium instead of Luna Ultra.

## 4. Color and renderer/dependency refresh

- [ ] Review the coordinated color-range set (`2bea406f`, `a903c5ce`, `4570fba8`, `3f26217d`, `b2f0828d`, `c1623ff4`, `7cf8b46c`) with the supporting FFmpeg fix (`2e13ed99`).
- [ ] Verify black/white levels, SDR/HDR, full/limited ranges, 8/10-bit and supported 4:2:0/4:4:4 combinations on the relevant renderers. Preserve PyroWave's format handling.
- [x] Review CUDA NV12-only format rejection and unbound-texture guards (`aa54ba2a`, `a6276aa8`). Applied in `03e045e7`; fallback selection source-reviewed. Actual CUDA fallback remains a hardware test.
- [ ] Review libplacebo missing-swapchain/decode-queue error handling (`ac296864`) and internally synchronized queues (`3fead131`) against Nonary's rendering and the selected libplacebo version. Error handling is applied in `03e045e7`; internally synchronized queues remain deferred with the dependency refresh.
- [ ] Plan a consistent FFmpeg/libplacebo/SDL/Qt/OpenSSL dependency set; do not stack historical dependency bumps mechanically.
- [ ] Review OpenSSL startup/CFG fixes (`b78956db`, `8ec57251`), Qt TLS backend deployment (`b0af84ea`) and OpenSSL 4.1 API compatibility (`31780228`) against that chosen set.
- [ ] Review qmdnsengine discovery update (`1da6ff43`) and confirm whether the required changes are already present.
- [ ] Retain MSVC toolchain consistency and the existing Qt-before-deploy DLL search order for tests; verify the actual packaged artifacts.

## 5. Platform coverage and maintenance dispositions

These remain consideration items even when our immediate release is Windows-only. Record a reason if deferring; do not imply these fixes are irrelevant merely because this workstation cannot validate them.

- [ ] Review Linux VAAPI/EGL/VDPAU/DRM correctness: initialization/failure paths, invalid mmap checks, buffer/VAO lifetime, missing attributes/modifiers and descriptor leaks. Individual IDs appear below.
- [ ] Review Linux AppImage/libva handling, SDL/libdecor/Qt startup livelock, desktop detection, Steam Link input cost, BSD AES feature detection, and RISC-V musl header compatibility.
- [ ] Review macOS Bonjour metadata, minimum-OS/dependency alignment, mouse polling and common-c non-prebuilt build compatibility.
- [ ] Review Windows ARM64 compiler hardening if that target remains supported.
- [ ] Review MSI signing/build-script hygiene against our packaging. Keep our custom tooling conventions; do not delete required tools just to mirror upstream.
- [ ] Decide deliberately on the original upstream default-resolution change (`018c851e`). Preserve fork defaults unless there is an explicit product decision to change them.
- [ ] Retain our own version/release notes rather than importing upstream release metadata (`e8fefc8c`).
- [ ] Record the final applied/already-equivalent/deferred/inapplicable outcomes, validation and release notes. Commit/version/push at the agreed release boundary.

## Original-upstream commit inventory

Grouped review inventory, not a claim that every unmarked commit should be cherry-picked. Some commits are packaging-only, superseded dependency bumps, dependencies of other changes, or may already have equivalent Nonary implementations.

### Already integrated

- [x] [`09873287`](https://github.com/moonlight-stream/moonlight-qt/commit/09873287) — Only use separate D3D11 devices on Intel and Nvidia — included in local backport `8e761eba`.
- [x] [`b86b5f71`](https://github.com/moonlight-stream/moonlight-qt/commit/b86b5f71) — Allow separate devices for Vega+ and block them for Fermi — included in local backport `8e761eba`.
- [x] [`31ce68f1`](https://github.com/moonlight-stream/moonlight-qt/commit/31ce68f1) — Log the GPU driver version in the D3D11VA renderer — included in local backport `8e761eba`.
- [x] [`e9d77967`](https://github.com/moonlight-stream/moonlight-qt/commit/e9d77967) — Replace Nvidia FL 11.1 check with driver version check — included in local backport `8e761eba`.
- [x] [`f743c236`](https://github.com/moonlight-stream/moonlight-qt/commit/f743c236) — Fix PixelShaderMinPrecision bitfield comparison — included in local backport `8e761eba`.

### Small GUI / CLI / window behavior

- [x] [`74020b53`](https://github.com/moonlight-stream/moonlight-qt/commit/74020b53) — Don't start polling from the GUI for a CLI launch — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`d79536b5`](https://github.com/moonlight-stream/moonlight-qt/commit/d79536b5) — Only accept usable bounds if they differ from the full bounds — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`029da3fb`](https://github.com/moonlight-stream/moonlight-qt/commit/029da3fb) — Improve readability of longer hostnames — Applied in local commit `9c2aa36f`.
- [x] [`4113dd26`](https://github.com/moonlight-stream/moonlight-qt/commit/4113dd26) — Fix toolbar button resize loop when moving between displays (#2093) — Applied in local commit `9c2aa36f`.
- [x] [`67dbfddb`](https://github.com/moonlight-stream/moonlight-qt/commit/67dbfddb) — Fix use of undefined variable — Applied in local commit `9c2aa36f`.

### Small renderer / fallback correctness

- [x] [`465c41b5`](https://github.com/moonlight-stream/moonlight-qt/commit/465c41b5) — Fix uninitialized InitFailureReason field — Applied in local commit `9c2aa36f`.
- [x] [`ba700d67`](https://github.com/moonlight-stream/moonlight-qt/commit/ba700d67) — Fix shadowed member variable with base class — Applied in local commit `9c2aa36f`.
- [x] [`d606709d`](https://github.com/moonlight-stream/moonlight-qt/commit/d606709d) — Add some missing error handling — Applied in local commit `9c2aa36f`.

### Pairing / request robustness

- [x] [`a6149245`](https://github.com/moonlight-stream/moonlight-qt/commit/a6149245) — Don't hardcode the QSslKey algorithm — Applied in local commit `8ce39873`.
- [x] [`2bd4b9f7`](https://github.com/moonlight-stream/moonlight-qt/commit/2bd4b9f7) — Add missing checks for X509 parsing failures — Applied in local commit `8ce39873`.
- [x] [`ee557e83`](https://github.com/moonlight-stream/moonlight-qt/commit/ee557e83) — Include the trailing 16 bytes in the challengeResponseData check — Applied in local commit `8ce39873`.
- [x] [`74173b26`](https://github.com/moonlight-stream/moonlight-qt/commit/74173b26) — Enforce a TCP/TLS HTTP timeout on Qt 6.3+ — Applied in local commit `8ce39873`.
- [x] [`781b26fd`](https://github.com/moonlight-stream/moonlight-qt/commit/781b26fd) — Completely disable connection caching on Qt 6.12+ — Applied in local commit `8ce39873`.
- [x] [`8f2e52e5`](https://github.com/moonlight-stream/moonlight-qt/commit/8f2e52e5) — Refuse to open a pre-existing log file (#2068) — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.

### Bitstream correctness and source hygiene

- [x] [`a57e947d`](https://github.com/moonlight-stream/moonlight-qt/commit/a57e947d) — Re-add the h264bitstream file headers mistakenly removed during vendoring — Applied in local commit `8ce39873`.
- [x] [`71c3191e`](https://github.com/moonlight-stream/moonlight-qt/commit/71c3191e) — Remove warn_off for h264bitstream — Applied in local commit `8ce39873`.
- [x] [`fa6ae809`](https://github.com/moonlight-stream/moonlight-qt/commit/fa6ae809) — Clamp additional values that can be used as array indices — Applied in local commit `8ce39873`.
- [x] [`289782c6`](https://github.com/moonlight-stream/moonlight-qt/commit/289782c6) — Fix possible UB shift in bs_write_u() for large values of n — Applied in local commit `8ce39873`.
- [x] [`9b8886a6`](https://github.com/moonlight-stream/moonlight-qt/commit/9b8886a6) — Fix integer truncation warning on MSVC — Applied in local commit `8ce39873`.
- [x] [`c8474c51`](https://github.com/moonlight-stream/moonlight-qt/commit/c8474c51) — Avoid signed overflow in bs_read_se() (#2071) — Applied in local commit `8ce39873`.
- [x] [`0c4f7aef`](https://github.com/moonlight-stream/moonlight-qt/commit/0c4f7aef) — Handle unexpected appVersion/gfeVersion quads — Applied in local commit `8ce39873`.
- [x] [`6fc7e816`](https://github.com/moonlight-stream/moonlight-qt/commit/6fc7e816) — Handle H.264 SPS read/write failure in fixup path — Applied in local commit `8ce39873`.

### Input behavior; coordinate core dependencies

- [x] [`d2f6990b`](https://github.com/moonlight-stream/moonlight-qt/commit/d2f6990b) — Send LI_CTYPE_STEAM for known Steam Controllers — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [ ] [`b75fe0d9`](https://github.com/moonlight-stream/moonlight-qt/commit/b75fe0d9) — Reorganize a couple mouse and keyboard functions
- [ ] [`c0f62ad6`](https://github.com/moonlight-stream/moonlight-qt/commit/c0f62ad6) — Distinguish between Enter and Numpad Enter
- [x] [`9fee675c`](https://github.com/moonlight-stream/moonlight-qt/commit/9fee675c) — Fix controller number check for adaptive triggers — Already equivalent: Nonary `72b936173`; no code change.

### Concurrency / pacing; adapt to Nonary

- [x] [`e4db5c2c`](https://github.com/moonlight-stream/moonlight-qt/commit/e4db5c2c) — Check for pacing queue emptiness before dequeuing — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`9af0e9f5`](https://github.com/moonlight-stream/moonlight-qt/commit/9af0e9f5) — Improve robustness and error handling of DXVsyncSource — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`017b6039`](https://github.com/moonlight-stream/moonlight-qt/commit/017b6039) — Add a lock to ensure no overlay callbacks are active during unregistration — Already equivalent through Nonary overlay callback locking; existing lifetime regression passed in batch 3.
- [x] [`10270ca0`](https://github.com/moonlight-stream/moonlight-qt/commit/10270ca0) — Move ownership of the overlay callback registration to Pacer — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`1ced9dd9`](https://github.com/moonlight-stream/moonlight-qt/commit/1ced9dd9) — Synchronize several renderer callbacks — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`aad0090d`](https://github.com/moonlight-stream/moonlight-qt/commit/aad0090d) — Invoke QCoreApplication::quit() on the main thread — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`fe5a0aea`](https://github.com/moonlight-stream/moonlight-qt/commit/fe5a0aea) — Prevent lost wakupes in Pacer and ComputerManager — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.

### Color-range / FFmpeg bundle

- [ ] [`2bea406f`](https://github.com/moonlight-stream/moonlight-qt/commit/2bea406f) — Guess frame color metadata if left unspecified
- [ ] [`a903c5ce`](https://github.com/moonlight-stream/moonlight-qt/commit/a903c5ce) — Explicitly request limited range for renderers that require it
- [ ] [`4570fba8`](https://github.com/moonlight-stream/moonlight-qt/commit/4570fba8) — Add full range handling to SdlRenderer
- [ ] [`3f26217d`](https://github.com/moonlight-stream/moonlight-qt/commit/3f26217d) — Select color range based on plane COLOR_RANGE property values
- [ ] [`b2f0828d`](https://github.com/moonlight-stream/moonlight-qt/commit/b2f0828d) — Use requested color range as a fallback in isFrameFullRange()
- [ ] [`c1623ff4`](https://github.com/moonlight-stream/moonlight-qt/commit/c1623ff4) — Switch the default renderer color range to full
- [ ] [`7cf8b46c`](https://github.com/moonlight-stream/moonlight-qt/commit/7cf8b46c) — Pass the correct renderer to EGLImageFactory
- [ ] [`2e13ed99`](https://github.com/moonlight-stream/moonlight-qt/commit/2e13ed99) — Update FFmpeg to fix 8-bit D3D11VA full range decoding

### Streaming core; use fork-compatible sync

- [ ] [`14c26d8c`](https://github.com/moonlight-stream/moonlight-qt/commit/14c26d8c) — Update moonlight-common-c with frame loss fixes
- [ ] [`dc92b6d2`](https://github.com/moonlight-stream/moonlight-qt/commit/dc92b6d2) — Update moonlight-common-c
- [ ] [`8016954c`](https://github.com/moonlight-stream/moonlight-qt/commit/8016954c) — Update moonlight-common-c
- [ ] [`29f121a7`](https://github.com/moonlight-stream/moonlight-qt/commit/29f121a7) — Fix moonlight-common-c with CONFIG+=disable-prebuilts on macOS

### CUDA / Vulkan renderer; feature and dependency checks

- [x] [`ac296864`](https://github.com/moonlight-stream/moonlight-qt/commit/ac296864) — Fix some error handling in the libplacebo renderer — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`aa54ba2a`](https://github.com/moonlight-stream/moonlight-qt/commit/aa54ba2a) — Reject 10-bit and YUV 4:4:4 in the CUDA path — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [x] [`a6276aa8`](https://github.com/moonlight-stream/moonlight-qt/commit/a6276aa8) — Check for unbound texture before registering with CUDA — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [ ] [`3fead131`](https://github.com/moonlight-stream/moonlight-qt/commit/3fead131) — Dynamically enable VK_KHR_internally_synchronized_queues for libplacebo v7.387.0

### Linux / BSD / Steam Link correctness

- [ ] [`526bde55`](https://github.com/moonlight-stream/moonlight-qt/commit/526bde55) — Stop bundling libva in the AppImage so VAAPI works on modern hosts
- [ ] [`513ceca9`](https://github.com/moonlight-stream/moonlight-qt/commit/513ceca9) — Disable everything except libva-x11 for AppImages
- [ ] [`b1824e9d`](https://github.com/moonlight-stream/moonlight-qt/commit/b1824e9d) — Check for vaMapBuffers2 which is required by FFmpeg with libva 2.21+
- [ ] [`3516591b`](https://github.com/moonlight-stream/moonlight-qt/commit/3516591b) — Fix bad APPDIR when running an extracted AppImage
- [ ] [`da19fa73`](https://github.com/moonlight-stream/moonlight-qt/commit/da19fa73) — Fix typo in vaMapBuffer2
- [ ] [`54f8fed5`](https://github.com/moonlight-stream/moonlight-qt/commit/54f8fed5) — Don't try fallback driver paths with the host libva
- [x] [`e3fd29e4`](https://github.com/moonlight-stream/moonlight-qt/commit/e3fd29e4) — Fix a livelock that can occur between SDL, libdecor-gtk, and Qt — Applied/adapted in local commit `03e045e7`; validation and platform limits recorded in the third-batch notes.
- [ ] [`8369d1a0`](https://github.com/moonlight-stream/moonlight-qt/commit/8369d1a0) — Reduce gamepad CPU usage on Steam Link
- [ ] [`a2486eaf`](https://github.com/moonlight-stream/moonlight-qt/commit/a2486eaf) — Check elf_aux_info() return value in hasFastAes()
- [ ] [`7927da33`](https://github.com/moonlight-stream/moonlight-qt/commit/7927da33) — Fix matching with colon-separated XDG_CURRENT_DESKTOP values
- [ ] [`b709bf54`](https://github.com/moonlight-stream/moonlight-qt/commit/b709bf54) — Handle musl headers without __NR_riscv_hwprobe defined
- [ ] [`40f83bd2`](https://github.com/moonlight-stream/moonlight-qt/commit/40f83bd2) — Avoid wedging VDPAU renderer if mixer creation fails
- [ ] [`1826d4c2`](https://github.com/moonlight-stream/moonlight-qt/commit/1826d4c2) — Fix incorrect mmap() failure check
- [ ] [`372887b8`](https://github.com/moonlight-stream/moonlight-qt/commit/372887b8) — Don't copy unpopulated EGLAttribs
- [ ] [`4158fa78`](https://github.com/moonlight-stream/moonlight-qt/commit/4158fa78) — Don't add EGL modifiers for VA images if they aren't set
- [ ] [`2b690d14`](https://github.com/moonlight-stream/moonlight-qt/commit/2b690d14) — Initialize VAAPIRenderer class members
- [ ] [`bfc6751f`](https://github.com/moonlight-stream/moonlight-qt/commit/bfc6751f) — Fix fd leak in VAAPI KMSDRM codepath
- [ ] [`df7db401`](https://github.com/moonlight-stream/moonlight-qt/commit/df7db401) — Fix some EGL error handling paths
- [ ] [`fb9daf85`](https://github.com/moonlight-stream/moonlight-qt/commit/fb9daf85) — Keep the VBO alive while the VAO references it
- [ ] [`29a280eb`](https://github.com/moonlight-stream/moonlight-qt/commit/29a280eb) — Fix return type of EGLRenderer::loadAndBuildShader()

### macOS-specific behavior / compatibility

- [ ] [`131cb270`](https://github.com/moonlight-stream/moonlight-qt/commit/131cb270) — Add NSBonjourServices entry to Info.plist
- [ ] [`5ac25421`](https://github.com/moonlight-stream/moonlight-qt/commit/5ac25421) — Update LSMinimumSystemVersion to match Qt 6.10+ and our prebuilts
- [ ] [`f018a71a`](https://github.com/moonlight-stream/moonlight-qt/commit/f018a71a) — Override NSApplicationUpdateCycleEnabled to fix reduced mouse polling rate

### Dependency / TLS deployment / build baseline

- [ ] [`b5d7f3b9`](https://github.com/moonlight-stream/moonlight-qt/commit/b5d7f3b9) — Update Windows, macOS, and AppImage dependencies
- [ ] [`1da6ff43`](https://github.com/moonlight-stream/moonlight-qt/commit/1da6ff43) — Update qmdnsengine submodule with latest upstream changes
- [ ] [`1c8b23c7`](https://github.com/moonlight-stream/moonlight-qt/commit/1c8b23c7) — Update dependencies
- [ ] [`0eff3b9b`](https://github.com/moonlight-stream/moonlight-qt/commit/0eff3b9b) — Update CI builds to Qt 6.11.2
- [ ] [`c045ae89`](https://github.com/moonlight-stream/moonlight-qt/commit/c045ae89) — Update to SDL 3.4.16
- [ ] [`ec71a4f5`](https://github.com/moonlight-stream/moonlight-qt/commit/ec71a4f5) — Update SDL3 in AppImage to fix several X11 issues
- [ ] [`49bf1e80`](https://github.com/moonlight-stream/moonlight-qt/commit/49bf1e80) — Update libplacebo to 3330a515
- [ ] [`032529d7`](https://github.com/moonlight-stream/moonlight-qt/commit/032529d7) — Update FFmpeg to 9.0.2 and libplacebo to e2972fd
- [ ] [`b78956db`](https://github.com/moonlight-stream/moonlight-qt/commit/b78956db) — Update SDL to 383fb13 and disable CFG for OpenSSL
- [ ] [`8ec57251`](https://github.com/moonlight-stream/moonlight-qt/commit/8ec57251) — Fix CFG violation on startup in OpenSSL
- [ ] [`6e699e4d`](https://github.com/moonlight-stream/moonlight-qt/commit/6e699e4d) — Update libplacebo, sdl2-compat, and SDL3
- [ ] [`32c6ab6e`](https://github.com/moonlight-stream/moonlight-qt/commit/32c6ab6e) — Update Windows CI to Qt 6.12
- [ ] [`b0af84ea`](https://github.com/moonlight-stream/moonlight-qt/commit/b0af84ea) — Deploy the OpenSSL TLS backend for Qt
- [ ] [`31780228`](https://github.com/moonlight-stream/moonlight-qt/commit/31780228) — Remove use of deprecated functions in OpenSSL 4.1
- [ ] [`8cfc9437`](https://github.com/moonlight-stream/moonlight-qt/commit/8cfc9437) — Update SDL3, sdl2-compat, and libplacebo
- [ ] [`966638f9`](https://github.com/moonlight-stream/moonlight-qt/commit/966638f9) — Remove libplacebo patch that's no longer needed

### Optional build / release maintenance

- [ ] [`fcba7df8`](https://github.com/moonlight-stream/moonlight-qt/commit/fcba7df8) — Update Windows and macOS build requirements
- [ ] [`6cae32bf`](https://github.com/moonlight-stream/moonlight-qt/commit/6cae32bf) — Remove checked-in jom.exe binary
- [ ] [`da331234`](https://github.com/moonlight-stream/moonlight-qt/commit/da331234) — Remove checked-in vswhere.exe binary
- [ ] [`256022d3`](https://github.com/moonlight-stream/moonlight-qt/commit/256022d3) — Quiet the 'where' invocations in build-arch.bat
- [ ] [`afcd08cc`](https://github.com/moonlight-stream/moonlight-qt/commit/afcd08cc) — Require SIGNTOOL_PARAMS to be set externally
- [ ] [`b7e0deb1`](https://github.com/moonlight-stream/moonlight-qt/commit/b7e0deb1) — Don't require SIGNTOOL_PARAMS to contain the 'sign' command
- [ ] [`de2467e4`](https://github.com/moonlight-stream/moonlight-qt/commit/de2467e4) — Fix MSI signing
- [ ] [`10523db2`](https://github.com/moonlight-stream/moonlight-qt/commit/10523db2) — Enable /guard:signret on Windows ARM64

### Product defaults / release metadata; retain fork policy

- [ ] [`018c851e`](https://github.com/moonlight-stream/moonlight-qt/commit/018c851e) — Change the default streaming resolution to 1080p
- [ ] [`e8fefc8c`](https://github.com/moonlight-stream/moonlight-qt/commit/e8fefc8c) — Prepare for v6.2.0

## Reference comparisons

- [Original upstream since the October snapshot](https://github.com/moonlight-stream/moonlight-qt/compare/dc92b6d2...29f121a7)
- [Nonary since the October snapshot](https://github.com/Nonary/moonlight-qt/compare/b35ce457...f586143c)
- [Nonary daily.200 release](https://github.com/Nonary/moonlight-qt/releases/tag/v6.2.0-daily.200)
- Initial working notes: `build/upstream-review-2026-11-13.md` (ignored research file; this checklist is the persistent repository document).
