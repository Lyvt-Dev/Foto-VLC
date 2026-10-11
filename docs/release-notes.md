PAF-Photos 0.5.0 replaces the previous Photo VLC branding and makes saving and updates easier.

- PAF-Photos branding throughout the app, executable, installer, shortcuts and Windows metadata. The original aperture-P logo remains. Existing installation and settings migrate.
- Fixed white button backgrounds during scrolling: controls repaint their complete surface, and layout changes redraw the parent and children together.
- Conversion now asks for the output format first, defaults to the original folder and keeps a confirmation with the full saved path, Open copy and Show in folder. PNG, JPEG, BMP, TIFF and lossless WebP are encoded in the background.
- Automatic updates for installed apps: checks on startup and daily, bounded HTTPS downloads, version and SHA256 verification, installation after exit, and an optional Install now action. Updates can be disabled. Portable builds offer the release download.

**Install setup.exe once to move from 0.4.0:** previous versions have no updater. Installation is per user, needs no admin account, retains your settings and leaves default photo associations unchanged.

The full **PAFPhotos-Windows-x64.zip** includes the codecs and update helper. Extract the complete ZIP; keep codecs beside PAFPhotos.exe. The individual EXE supports fewer formats. Builds are currently unsigned.

Windows rendering, five-format conversion, actual codec pixels and thumbnails, install/uninstall, updater checksum rejection and installation checks accompany this release. After publication, a Windows test checks the actual HTTPS manifest and setup download. Recognized extensions do not guarantee decoding every proprietary format; see the format guide. Animation and multipage viewing show the first frame/page; conversion exports the displayed raster without project layers or source metadata.

The optional PAFPhotos-Codec-Sources.zip contains corresponding codec sources and licenses and is not needed to run the app.
