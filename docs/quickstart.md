# Photo VLC 0.3.0

A portable photo gallery and image viewer for Windows 10/11 (64-bit).

## Get started / Loslegen

1. Extract the entire ZIP into a folder / ZIP vollständig in einen Ordner entpacken.
2. Start **PhotoVLC.exe**.
3. Choose **Folder / Ordner** to add your photo folders to the gallery.
4. Click a thumbnail to open a photo, or drag a file into the window.

Keep the **codecs** directory next to the EXE. It provides additional formats
without installing another application. The gallery reads the files directly;
adding a folder does not copy or modify its contents. Import scans that folder,
not its subfolders; add other folders separately. The gallery indexes up to
10,000 recognized files.

## Language and appearance / Sprache und Design

Open **Settings / Einstellungen** using the gear at the top right.
Choose **Deutsch**, **English** or the system language. Changes are immediate.
The system language defaults to German on German Windows and English otherwise.
Choose system, dark or light appearance and your preferred opening zoom.

## Gallery and favorites

Search by file name or extension. Use Select to select several photos, then
Toggle favorites. You can also use the heart while viewing a photo.
The sidebar shows your favorites; Sort reverses filename order and the grid
button changes thumbnail size. Scroll to browse more images.

## Viewing shortcuts

- Ctrl+O: open image; Ctrl+Shift+O: open folder.
- Left/Right: previous/next image; mouse wheel: zoom; drag: pan.
- F: fit; 1: actual size; double-click: switch between the two.
- R: rotate; F11: fullscreen; Space: slideshow.
- I: details; T: filmstrip; F1: keyboard help.
- Esc: close help/settings/fullscreen/slideshow or return to the gallery.
- Tab / Shift+Tab: move focus; Enter: activate the focused button.

Viewing, zooming, rotating and favoriting do not change your original files.

## Local preferences

Folders, favorites, viewing preferences and recent images are stored in
`%LOCALAPPDATA%\PhotoVLC\settings.ini`. Existing Foto VLC settings are migrated.
No account or telemetry is required. Removing a gallery folder does not delete
its files. Clear the recent-image list in Settings.

See **FORMATS.md** and **CODECS.md** for actual supported formats and limits.
New versions: https://github.com/Lyvt-Dev/Foto-VLC/releases
MIT license: LICENSE. Third-party terms: THIRD_PARTY_NOTICES.md and codecs/NOTICE.txt.
