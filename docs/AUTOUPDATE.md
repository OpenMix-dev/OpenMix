# Auto-update setup

OpenMix ships silent auto-updates via Sparkle (macOS) and WinSparkle (Windows).
Linux builds fall back to a notify-and-link check against GitHub Releases.

At runtime the app polls a signed appcast, downloads the new installer in the
background, verifies its EdDSA signature, and installs on next launch. The whole
pipeline hinges on one signing key and a place to host the appcasts.

## One-time key setup

1. Generate an EdDSA key pair with Sparkle's tool (ships in the Sparkle release,
   `bin/generate_keys`):

   ```
   ./bin/generate_keys
   ```

   It prints a public key and stores the private key in the login keychain. To
   export the private key for CI:

   ```
   ./bin/generate_keys -x sparkle_private_key.pem
   ```

2. Put the **public** key in `packaging/Info.plist.in`, replacing the
   `REPLACE_WITH_SPARKLE_ED_PUBLIC_KEY` placeholder in the `SUPublicEDKey` entry.
   This is compiled into the macOS bundle and is what Sparkle checks signatures
   against.

3. Add the **private** key to the GitHub repo as an Actions secret named
   `OPENMIX_SPARKLE_ED_PRIVATE_KEY`. CI uses it to sign the release DMG and emit
   the `sparkle:edSignature` in the macOS appcast. Without it, the appcast job
   logs a warning and macOS clients will not accept the update.

## Appcast hosting (GitHub Pages)

The apps poll these URLs (set in `Info.plist.in` `SUFeedURL` and in
`AutoUpdater.cpp` for WinSparkle):

- macOS: `https://johnqherman.github.io/OpenMix/appcast-macos.xml`
- Windows: `https://johnqherman.github.io/OpenMix/appcast-windows.xml`

The `appcast` job in `.github/workflows/build.yml` builds both files on every
`v*` tag and publishes them to the `gh-pages` branch (via
`peaceiris/actions-gh-pages`, `keep_files: true` so old entries survive). Enable
GitHub Pages for the repo, serving from the `gh-pages` branch root.

## Release flow

1. Bump `VERSION` / `project(... VERSION x.y.z)` in `CMakeLists.txt`.
2. Tag and push: `git tag vx.y.z && git push origin vx.y.z`.
3. CI: `build-*` jobs make installers, `release` attaches them to the GitHub
   Release, `appcast` signs + publishes the appcasts to `gh-pages`.
4. Installed clients pick up the update on their next scheduled check.

## Code signing (required for a clean silent update)

Silent install only works without a Gatekeeper / SmartScreen prompt if the
installer is signed by a trusted authority:

- **macOS**: Developer ID sign the `.app` and notarize the `.dmg`. Unsigned
  builds will refuse to auto-install or warn the user.
- **Windows**: Authenticode sign the NSIS `.exe`. Unsigned builds trip
  SmartScreen and break the silent flow.

Sparkle's EdDSA signature protects the download's integrity; OS code signing is
what suppresses the security prompt on install. Both are needed for a truly
hands-off update.

## Files

- `src/app/AutoUpdater.{h,cpp}` — platform abstraction; picks WinSparkle /
  Sparkle / notify-fallback at build time.
- `src/app/AutoUpdaterSparkle.mm` — macOS Sparkle bridge.
- `src/app/UpdateChecker.{h,cpp}` — Linux GitHub Releases notify-and-link check.
- `packaging/Info.plist.in` — macOS bundle plist with `SUFeedURL` /
  `SUPublicEDKey`.
- `.github/workflows/build.yml` — `appcast` job that signs + publishes appcasts.
