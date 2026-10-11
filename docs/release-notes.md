Photo VLC 0.4.0 adds a Windows installer, a distinct identity and real image conversion.

- Original cyan/violet aperture-P logo, a cool charcoal palette and flat buttons without bevels or toolbar shadows.
- Choose any accent color, a 0–16 px corner radius and light/dark/system themes. Settings persist across restarts.
- Smooth button hover animations; optional and respectful of Windows reduced-motion preferences.
- **Convert** (Ctrl+S): save a copy as PNG, JPEG, BMP, TIFF or lossless WebP. Output extension follows the selected encoder. JPEG quality is configurable; PNG/WebP preserve transparency, JPEG/BMP use a white background.
- Horizontal mirror (M), reset view and copy the transformed image to the clipboard (Ctrl+C, white background).
- Conversion includes rotation and mirroring and refuses to overwrite the source. Large decoded previews and animation/multipage exports show a notice: only the displayed raster / first frame is exported, without project layers or source metadata.
- Optional subfolder scanning (up to 10,000 gallery images, no traversal of directory links).
- German and English interface and installer; visible version information.

Download **setup.exe** for a per-user installation with codecs, Start menu shortcut, optional desktop shortcut and an Open With registration. No admin account is required. Default photo associations are not changed. Uninstall removes the app and its registration while preserving your preferences.

**PhotoVLC-Windows-x64.zip** is the complete portable alternative. Extract the whole ZIP and launch PhotoVLC.exe; keep `codecs` beside it. The individual PhotoVLC.exe includes fewer decoders. Neither build is digitally signed.

Windows UI and conversion tests, actual codec pixel tests, silent install/uninstall checks and checksums accompany the release. Recognized extensions are not a promise that every proprietary project format can be decoded; see the format guide. Animation and multipage viewing still show the first frame/page.

The optional **PhotoVLC-Codec-Sources.zip** contains corresponding source archives and license information for the bundled codec runtime; it is not needed to run the app.
