# 0.7.2-fix

- Editor buttons paint their full pixel bounds with no native square outlines or stale corner pixels.
- Trackbars suppress native theme painting and repaint their entire buffered surface on position and focus changes, avoiding duplicate thumb rings and movement trails.
- Keep keyboard navigation and native slider interaction; match the surrounding canvas or sidebar background.
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
