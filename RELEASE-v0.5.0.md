# v0.5.0 — macOS Apple Silicon (preview)

Native C++/Cocoa port of the PC game; no Wine or CrossOver.

- macOS ARM64 application with bundled libpng (Homebrew not required).
- Save-screen fixes and corrected subtractive blending for shadows.
- Keyboard/controller, French text and HUD work remain under validation.

Download BOTH the `.dmg` and `.dmgpart` assets into the SAME folder, without
renaming them. Open the `.dmg`, copy the application to a writable folder such as Applications,
then launch it. This build uses an ad-hoc signature, not an Apple Developer ID
signature or notarization; macOS may require explicit approval on first launch.
Do not disable system-wide security protections.

This is a preview, not a claim of crash-free gameplay. iOS/iPadOS/tvOS builds
are not included. Four automated smoke tests pass; full gameplay and save/load
round trips still require manual testing.

The icon refresh is pending: this package retains the previous icon.

The locally prepared bundle contains PC game data. Before publishing it, ensure
you have permission to redistribute all included material. Ownership of a game
copy is not a redistribution license. Nothing is uploaded by the packaging script.
