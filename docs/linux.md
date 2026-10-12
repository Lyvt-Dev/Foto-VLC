# PAF-Photos on Linux

PAF-Photos 0.7.4 has a **native C++/Qt 6 Linux application**. It does not need
Wine, Electron, a browser, an account or a Windows executable.

## Downloads

- **PAFPhotos-Linux-x86_64.AppImage**: portable application including Qt,
  ImageMagick, JPEG XL, LibRaw, Poppler and Ghostscript readers/delegates.
  Intended for glibc-based x86-64 desktop distributions with glibc 2.35 or newer
  (Ubuntu 22.04+, Debian 12+, recent Fedora). Make it executable and start it:

  ```sh
  chmod +x PAFPhotos-Linux-x86_64.AppImage
  ./PAFPhotos-Linux-x86_64.AppImage
  ```

  If FUSE is unavailable, use:

  ```sh
  APPIMAGE_EXTRACT_AND_RUN=1 ./PAFPhotos-Linux-x86_64.AppImage
  ```

- **PAFPhotos-Linux-x86_64.deb**: Ubuntu/Debian package with application-menu and
  Open With integration. Libraries and additional readers are dependencies
  maintained by the distribution:

  ```sh
  sudo apt install ./PAFPhotos-Linux-x86_64.deb
  ```

- **linux-SHA256SUMS.txt**, actual package self-test reports and exact bundled
  dependency sources are available on the same GitHub release.

The AppImage is built on Ubuntu 22.04. Older glibc, Alpine/musl and ARM are not
covered by the published binaries. X11/Wayland clients use Qt; the window frame,
file dialogs, clipboard and Trash follow the Linux desktop. They are not copies
of Windows system chrome.

## Features

Gallery imports, recursive folders, search, sorting, thumbnail sizes, favorites,
selection, hidden photos, right-click actions, Trash and separately confirmed
permanent deletion; image viewer, filmstrip, navigation, zoom, fit, rotation,
clipboard, fullscreen, slideshow and file information.

The editor uses the **same portable C++ engine as Windows**: crop handles and
aspect ratios, straighten, rotate/flip, exact resizing, ten light/color controls,
15 thumbnail filters, auto enhancement, pen/highlighter/eraser, local retouch,
background blur/removal/replacement and manual selection brushes. Processing is
asynchronous; smaller previews do not replace the full-resolution working image.
Undo/redo and original comparison are supported. PNG/JPEG/BMP/TIFF/WebP are saved
as atomic copies, protecting the original. The save confirmation shows the path
and provides an Open Folder action.

Theme, German/English, accent color, radius, file-name visibility, folder recursion,
zoom default, export quality and animation settings are stored in
`~/.config/Lyvt-Dev/PAF-Photos.conf`. Wheel scrolling uses eased animation;
trackpad pixel scrolling remains direct. Space compares the original in the
focused editor canvas. Ctrl+O/S/Z/Y, arrows, F11 and Escape are supported.

The searchable decoder catalog lists Qt readers, recognized extensions and the
complete installed ImageMagick format table. Missing readers can be installed
through the distribution's package manager. Reader availability and proprietary
formats remain backend-dependent; recognition of 197 extensions does not mean
197 guaranteed decoders. Camera-model support depends on LibRaw. Animated and
multipage files currently show one frame/page, as on Windows. Local background
selection and retouch are not generative AI.

Writable AppImages check the Linux-only HTTPS update manifest, verify the complete
SHA256 before replacement, and apply downloaded updates on exit with a unique
backup. System-installed DEB applications direct updates to the release/package
manager rather than writing protected system directories. Windows updater assets
are not used on Linux.

## Build and tests

```sh
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-svg-dev \
  qt6-image-formats-plugins qt6-wayland imagemagick libmagickcore-6.q16-6-extra libraw-bin libjxl-tools \
  poppler-utils ghostscript libglib2.0-bin
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DPAF_BUILD_LINUX=ON
cmake --build build/linux --parallel 4
ctest --test-dir build/linux --output-on-failure
QT_QPA_PLATFORM=offscreen build/linux/PAFPhotos --self-test linux-self-test.txt
build/linux/PAFPhotos
```

The six existing portable suites remain available with the default CMake
configuration. `PAF_BUILD_LINUX=ON` explicitly enables the Qt desktop target;
Windows builds do not acquire a Qt dependency. Package builds use pinned,
checksum-verified AppImage tooling and runtime binaries. CI starts both the
AppImage and extracted DEB, runs actual decoder and export checks, and downloads
and verifies the published update asset.

## Bundled software

Qt and other libraries are dynamically linked and replaceable after
`--appimage-extract`; original copyright/license texts and an exact distribution
package/source manifest are retained in `usr/share/doc` inside the AppImage.
`PAFPhotos-Linux-Sources.tar.xz` includes corresponding distribution source
archives, patches, application source and the pinned AppImage runtime source.
