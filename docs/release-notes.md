# 0.7.4 — native Linux support

- Add a native C++/Qt 6 Linux desktop with the shared Windows image-editing engine and PAF styling.
- Include gallery/favorites, folder imports, context-menu deletion/Trash, viewer/filmstrip, editor, customizable settings, decoder catalog, English/German, and Linux AppImage update verification.
- Publish x86-64 installer AppImage and Debian/Ubuntu packages alongside the existing Windows installer. Both platforms report 0.7.4; their updater manifests and packages remain separate.
- Exercise real Linux decoder fixtures, all five output formats, actual AppImage installation/uninstallation, transparent backgrounds, full-resolution commit, undo/redo, package startup and the published Linux update download.
- Desktop-provided window borders/dialogs differ by Linux desktop; proprietary formats and camera variants remain decoder-dependent. See docs/linux.md.

# 0.7.3

- Keep editor buttons and slider controls stable during adjustments: unchanged captions, bounds, enabled states and visibility are no longer reset on every preview.
- Generate cached previews at up to 1024 pixels while dragging; calculate the final edit at the original decoded resolution after release. Undo records one full-resolution result per gesture.
- Match editor buttons to the app's raised surfaces, subtle outlines, corner-radius setting and accent-color contrast.
- Align the About/help back button with its content, clear of the navigation rail.
- Add a searchable catalog of built-in readers, all registered Windows WIC decoders, and the complete format list from the installed ImageMagick runtime. The catalog is populated on a worker thread.
- Add official Ghostscript, Windows image-extension and ImageMagick format-documentation links. These are external downloads; a reader name does not guarantee every file variant or missing delegate.
- Verify bounded live previews, full-resolution commit, stable enabled controls, and the actual installed decoder catalog in native Windows tests.

# 0.7.2-fix

- Editor buttons paint their full pixel bounds with no native square outlines or stale corner pixels.
- Trackbars suppress native theme painting and repaint their entire buffered surface on position and focus changes, avoiding duplicate thumb rings and movement trails.
- Slider pointer input uses the same geometry as the visible knob, fixing missed drags and incorrect positions. Keep keyboard navigation, capture release, and sidebar wheel scrolling; match the surrounding canvas or sidebar background.
- Add native rendering regressions for button edges and incremental slider painting, and validate fix-version update ordering.
- This release is named `0.7.2-fix`. Install it manually from `setup.exe`: older releases accept numeric-only update manifests. This fix accepts both numeric releases and the `-fix` suffix for subsequent updates.

PAF-Photos 0.7.2 introduces a full photo-editing workspace inspired by the supplied Windows Photos references, with PAF colors, rounded controls and offline processing.

- A large image canvas, centered icon tool tabs, zoom/fit, reset, undo/redo, Save options and Cancel/Done replace the small editing sidebar. The filename and app version stay visible. The gallery window is hidden while editing, leaving one visible app frame. Hold Space to compare the original.
- Crop directly on the image using eight drag handles and a rule-of-thirds grid. Free and locked ratios (1:1, 4:3, 16:9, 3:4, 9:16), quarter-turn rotation, horizontal/vertical flips and a -45° to +45° straighten slider are included. Straightening zooms to cover the canvas without empty corners. Resize retains the exact pixel-dimension dialog from 0.7.1.
- Adjust offers brightness, exposure, contrast, highlights, shadows, vignette, saturation, warmth, tint and sharpness. Dragging shows asynchronous full-raster previews; the latest change wins. Each completed gesture can be undone. The native, keyboard-accessible sliders sit in a clipped panel with smooth scrolling.
- Filters includes 15 real thumbnails generated from the current photo, including warm/cool looks and black & white variants. Auto enhance uses the image luminance histogram. Presets apply from the tool's starting image so repeated selection does not compound the filter.
- Draw includes pen, translucent highlighter, color and brush size. The drawing eraser restores the photo under the marks. Retouch repairs brushed regions from neighboring pixels. Background offers blur, transparency removal and solid-color replacement, real preview cards, tolerance/blur controls and remove/keep brushes for manual refinement.
- Save options exports an edited copy as PNG, JPEG, BMP, TIFF or lossless WebP through a Windows destination dialog. Saving runs in the background and provides a saved-file status and Show in folder button. Closing warns about unsaved changes; original files stay unchanged.

Background selection is deterministic, edge-connected corner-color selection, not AI subject segmentation. It works best with distinct backgrounds and may need manual brushing. Retouch is local repair, not generative inpainting. No models, accounts or online image uploads are used.

Download **setup.exe** to install, or the complete portable ZIP. Existing installations can update through Settings. Both include the extended decoder runtime; the single EXE can install it from the sidebar. Builds remain unsigned.

Six portable suites cover decoding, core behavior, updates, motion, legacy edits and the new editor engine. Native editor automation exercises 14 stages covering text rendering, adjustments/undo, actual crop dimensions, straightening, rotation, drawing erasure, retouch, background/brush refinement, panel reparenting, real filter thumbnails and decoded transparent PNG export with an unchanged source. Windows controls, codecs, installer/updater and actual published decoder installation are also checked.

GIFs and multiframe files still show the first frame. Editing operates on the decoded 8-bit raster and does not preserve metadata or project layers. Processing is bounded to 32 megapixels; resize accepts 1–16384 pixels per dimension. Proprietary design/CAD projects and formats requiring additional delegates are not universally supported. See FORMATS.md / docs/formats.md for exact limits.

The optional PAFPhotos-Codec-Sources.zip provides corresponding codec sources and licenses; it is not needed to run the app.
