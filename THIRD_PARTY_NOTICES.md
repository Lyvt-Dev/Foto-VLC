# Third-party notices

Foto VLC includes these small libraries directly in its executable. No Electron,
Qt, browser engine, network telemetry, or third-party runtime installation is required.

- **stb_image**, Sean Barrett and contributors. Vendored from
  https://github.com/nothings/stb at commit
  `2c980bb59875b0d32144a71867fbdebb2f77cd20`. Used under its MIT license:
  [third_party/stb/LICENSE](third_party/stb/LICENSE). Its full license is also
  included at the bottom of the vendored header.
- **NanoSVG**, Mikko Mononen and contributors. Vendored from
  https://github.com/memononen/nanosvg at commit
  `239e102ec2c691f2902e20ace2ed36ee4a35cfe6`. Used under its zlib license:
  [third_party/nanosvg/LICENSE.txt](third_party/nanosvg/LICENSE.txt). Its full
  license is included at the top of both vendored headers.
- The QOI decoder in this project is an original implementation of the public
  specification, https://qoiformat.org/qoi-specification.pdf.
- Windows WIC, GDI+ and system libraries are supplied by Windows.
- **ImageMagick** and **Ghostscript** are optional, separately installed software.
  They are not bundled or downloaded by Foto VLC. Their own licenses apply.

The project's name does not imply affiliation with VideoLAN or VLC media player.
