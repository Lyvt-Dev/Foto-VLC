PAF-Photos 0.7.0 adds built-in formats, in-app decoder installation, gallery actions and photo editing.

- The single EXE now decodes PCX/PCC (8-bit palette/RGB), DDS (RGB24/RGBA32 and DXT1/3/5), PBM, PAM, SGI/RGB/RGBA/BW (8-bit raw/RLE), XBM and HDR/RGBE without extra installation. ASCII Netpbm and 16-bit Netpbm/PAM variants are also supported. Unsupported variants remain listed in the format guide.
- A Decoders sidebar page shows built-in and optional support. The app downloads its versioned decoder ZIP over HTTPS, checks its SHA256 and size, extracts with built-in Windows components and activates the complete package under LocalAppData. No administrator rights or separate ImageMagick setup are needed. The full installer and portable ZIP already include the optional runtime.
- Right-click a gallery image to add/remove a favorite, hide it from the gallery, move it to the Windows Recycle Bin, or permanently delete it with explicit confirmation. Hiding keeps the file on disk; the gallery menu can restore hidden images. Shift+F10 supports keyboard access.
- Edit offers centered crops (1:1, 4:3, 16:9, 3:4), aspect-preserving resize, brightness, contrast, saturation, black & white and sepia. Changes run in the background, preserve transparency and have undo/redo. Save copy exports the actual edited pixels; the original stays unchanged. Leaving a photo or closing the app warns about unsaved edits.

Download **setup.exe** to install, or extract the complete **PAFPhotos-Windows-x64.zip** for portable use. Existing installed PAF-Photos versions can update via Settings → Check for updates → Install now. Preferences are preserved. The standalone EXE has built-in formats and can install the optional decoder pack from its sidebar. Builds remain unsigned.

Validation exercises decoder pixels and malformed/truncated inputs, editing math and transparency, actual native controls, background editing and decoded exports, persistent gallery exclusions, permanent deletion of an owned fixture, and the real Windows Recycle Bin. Five portable suites run alongside Windows, codecs, installer and updater checks. The release additionally verifies an actual published decoder download, extraction and AVIF decode on Windows.

GIFs and multiframe files still show the first frame. Editing works on the decoded raster; exports do not preserve original metadata or project layers. Crops use a centered ratio preset, and resize presets only reduce dimensions. Proprietary design/CAD projects and formats requiring additional delegates are not universally supported. See FORMATS.md / docs/formats.md for exact limits.

The optional PAFPhotos-Codec-Sources.zip provides corresponding codec sources and licenses; it is not needed to run the app.
