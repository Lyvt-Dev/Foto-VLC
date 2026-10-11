# Additional image formats

The **PhotoVLC-Windows-x64.zip** download contains a portable ImageMagick
7.1.2-33 Q8 x64 runtime in `codecs/`. Photo VLC discovers it automatically;
no installation, PATH changes or account are required. Keep that directory
next to `PhotoVLC.exe`. The standalone EXE uses built-in decoders and installed
Windows codecs; it can also discover a separately installed ImageMagick.

The runtime is an unmodified upstream program by **ImageMagick Studio LLC**,
invoked in a separate process with time and memory limits. Its complete license
and dependency notices are in `codecs/LICENSE.txt` and `codecs/NOTICE.txt`.
The upstream software bill of materials is `codecs/SBOM.json`.

Actual decoding, decoded pixel colors and gallery thumbnails are tested for
AVIF, HEIC, JPEG XL, JPEG 2000, OpenEXR, DDS and PCX. Test files are original
synthetic images, not downloaded photographs. This verifies those test variants;
it does not guarantee every variant or camera model.

Many camera RAW formats and older raster/vector formats are available through
the runtime's additional readers. Camera/model support depends on LibRaw.
PDF, EPS and PostScript still require a working Ghostscript delegate and
compatible policy. Proprietary design projects, CAD files and game containers
are not universally readable. Animation/multipage viewing currently shows one
frame/page. See FORMATS.md for the complete list and restrictions.

Corresponding upstream source/build trees are provided separately in
**PhotoVLC-Codec-Sources.zip** on the same release page. They are not required
to run the app. That archive includes ImageMagick, its Configure tool and
all exact dependency source revisions from the upstream build recipe,
with original license notices and a manifest of archive checksums.

Upstream release: https://github.com/ImageMagick/ImageMagick/releases/tag/7.1.2-33

Binary archive: `ImageMagick-7.1.2-33-portable-Q8-x64.7z`
SHA-256: `c091195d6671d1f6e9b4333d41a6946ccc6c154574e0c5441dbea4495edb0c7e`

You can override discovery with `PHOTO_VLC_MAGICK` set to the absolute path to
`magick.exe`. The former `FOTO_VLC_MAGICK` variable is accepted for compatibility.
