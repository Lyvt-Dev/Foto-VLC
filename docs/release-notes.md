PAF-Photos 0.7.1 is a focused usability update.

- Resize opens a pixel-dimension dialog with width, height, live resulting dimensions and an aspect-ratio lock. Both enlargement and reduction change the actual raster; unlocking the ratio allows exact independent dimensions. Invalid or excessive dimensions cannot be applied. Undo/redo and Save copy keep working.
- The main window uses one custom titlebar; native caption buttons are removed while resizing, dragging, minimizing, maximizing and the system menu remain available. Maximized windows respect the monitor work area.
- Decoders now recommends the real extended-format package and lists its included format groups (AVIF, HEIC/HEIF, JPEG XL, JPEG 2000, OpenEXR). Install and Reinstall remain actionable instead of becoming a disabled “Decoders available” button. A user-installed pack takes precedence over a bundled pack, so reinstalling actually replaces the runtime used. One verified package supplies these groups; they are not separate downloads.

Download **setup.exe** to install, or the complete portable ZIP. Existing installations can update through Settings. Both include the extended decoder runtime; the single EXE can install it from the sidebar. Builds remain unsigned.

Validation exercises decoder pixels and malformed/truncated inputs, editing math and transparency, actual native controls, background editing and decoded exports, persistent gallery exclusions, permanent deletion of an owned fixture, and the real Windows Recycle Bin. Five portable suites run alongside Windows, codecs, installer and updater checks. The release additionally verifies an actual published decoder download, extraction and AVIF decode on Windows.

GIFs and multiframe files still show the first frame. Editing works on the decoded raster; exports do not preserve original metadata or project layers. Crops use a centered ratio preset, and resize accepts 1–16384 pixels per dimension and at most 32 megapixels. Proprietary design/CAD projects and formats requiring additional delegates are not universally supported. See FORMATS.md / docs/formats.md for exact limits.

The optional PAFPhotos-Codec-Sources.zip provides corresponding codec sources and licenses; it is not needed to run the app.
