# Versioned integration builds

The test series starts at **7.1.1.1**, followed by **7.1.1.2**. These are cumulative integrations: each build includes the preceding build's changes. Related changes are integrated, reviewed and checked in separate steps before the final MSI is produced. A successful automated gate permits the next build without waiting for interactive testing of the preceding one.

Each version has a committed change record under `docs/releases`, a committed `app/version.txt`, an annotated Git tag, and an MSI archived in the project root. The adjacent JSON manifest records the source revision, executable hash, MSI hash and installer identity. MSI binaries remain local artifacts rather than Git contents. Archives are never overwritten; a correction receives a new version and is carried forward.

The MSI's executable and dependencies come from the same release build and a new deployment directory. Build and test logs remain under `build`. Existing portable settings and installed copies are not used as packaging inputs. Preparing a build does not install it or push commits to GitHub.

## Installing the series

`7.1.1.1` is newer than `7.1.0.0` for Windows Installer upgrade detection and uses the normal upgrade path. Windows compares only the first three version fields. The existing `AllowSameVersionUpgrades` setting permits replacing `7.1.1.1` with `7.1.1.2`, or the reverse. Use the direct archived MSI for this series.

An MSI replacement does not roll settings back to their earlier contents. When diagnosing a regression, record the exact version shown in the app and which profile/settings were used. Configuration format changes, if any, must be called out in that version's record and need separate compatibility review.

References: [Windows Installer ProductVersion](https://learn.microsoft.com/en-us/windows/win32/msi/productversion), [WiX MajorUpgrade](https://docs.firegiant.com/wix/schema/wxs/majorupgrade/).

## Build records

- [7.1.1.1](releases/7.1.1.1.md): host warnings and confirmed paired-host sleep; streaming core remains at the preceding tested revision. MSI built and payload verified; [artifact manifest](releases/manifests/7.1.1.1.json).
- [7.1.1.2](releases/7.1.1.2.md) adds the fork-compatible core sync, bounded PyroWave missing-tail recovery and extended keyboard input.

Artifact manifests are added after successful MSI assembly. Remaining integration candidates and deferred work are tracked in [moonlight-update-todo.md](moonlight-update-todo.md).

- [7.1.1.3](releases/7.1.1.3.md) corrects the stale GUI version in the second installer. [Artifact manifest](releases/manifests/7.1.1.3.json).
