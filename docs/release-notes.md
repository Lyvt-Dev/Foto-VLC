Photo VLC 0.3.0 introduces a photo gallery and a Windows-inspired interface based on the supplied references.

- Renamed the application and Windows executable to **Photo VLC / PhotoVLC.exe**.
- Gallery with real image previews, file-name/type search, folder import, natural sorting and three tile sizes.
- Collapsible navigation, saved favorites and multiple-photo selection.
- Settings with system/dark/light themes, German/English/system language and opening zoom preferences.
- Complete German and English UI, including dialogs, tooltips, keyboard help and decoder errors. Language changes take effect immediately.
- Fixed button background painting with buffered rendering; consistent hover, pressed, selected and keyboard-focus states.
- Added a built-in WebP decoder, including first-frame preview for animated WebP.
- The portable ZIP includes additional codecs: verified AVIF, HEIC, JPEG XL, JPEG 2000, EXR, DDS and PCX decoding and previews, without manual installation.
- Preserves gallery folders, favorites and preferences locally, and migrates settings from Foto VLC.
- Visible version in the app, About screen and EXE properties.

**Download PhotoVLC-Windows-x64.zip** for the complete portable application with additional codecs and license documentation. Extract it and start PhotoVLC.exe. The standalone PhotoVLC.exe is the smaller download and needs additional Windows codecs or ImageMagick for some formats.

Windows UI tests cover gallery search, selection, favorites, English/German switching, settings persistence, themes and 100/150/200% display scaling. Additional codec tests check real decoded pixels and gallery previews. Test reports and checksums are included with this release.

**Format boundaries:** 194 recognized file extensions do not mean 194 fully supported formats. Proprietary design/CAD projects and game containers remain dependent on available readers. RAW support depends on camera/model. PDF/EPS/PS need Ghostscript. Animations and multipage files currently show the first image. The EXE is not code-signed.

PhotoVLC-Codec-Sources.zip provides the corresponding source/build trees for the included third-party codec runtime; it is not needed to run Photo VLC.
