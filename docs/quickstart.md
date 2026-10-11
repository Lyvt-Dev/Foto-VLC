# PAF-Photos 0.6.0

A portable photo gallery and image viewer for Windows 10/11 (64-bit).

## Get started / Loslegen

1. Extract the entire ZIP into a folder / ZIP vollständig in einen Ordner entpacken.
2. Start **PAFPhotos.exe**.
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

## Convert and personalize

Open a photo and choose **Convert** or press Ctrl+S. Select PNG, JPEG, BMP, TIFF or WebP
first, then choose the destination in the Save dialog. The source folder is the default.
After saving, a persistent card shows the full path and offers **Open copy** and
**Show in folder**. PAF-Photos actually encodes the selected format and sets its extension.
The original cannot be overwritten. Rotation and mirroring are included.
PNG/WebP preserve alpha; JPEG/BMP use white behind transparency. WebP is lossless.
Export uses the decoded raster without source metadata or project layers. If a large
file was downsampled, or has multiple frames/pages, a notice explains the limitation.

Use **Settings** to choose any accent color, corner radius, animations, JPEG quality
and whether gallery folders include their subfolders. Gallery and settings scroll smoothly
with pixel movement and gentle deceleration, including small touchpad deltas. Turning
Animations off also disables scroll motion and honors Windows reduced-motion settings. Reduced-motion settings in
Windows disable hover animations. Folder scanning is limited to 10,000 images.

For installation, download **setup.exe**. It installs for your user without admin
rights, creates a Start menu shortcut and offers an optional desktop shortcut.
Uninstall through Windows Settings; your local preferences are retained.

## Konvertieren und Anpassen

Bild öffnen → **Konvertieren** oder Strg+S. PNG, JPEG, BMP, TIFF oder WebP auswählen.
Das Bild wird als Kopie im gewählten Format gespeichert. Drehung und Spiegelung
werden übernommen. PNG/WebP bewahren Transparenz, JPEG/BMP verwenden Weiß.
Bei verkleinerten Vorschauen bzw. mehreren Bildern/Seiten erscheint ein Hinweis.
Unter **Einstellungen** findest du Akzentfarbe, Eckenradius, Animationen,
JPEG-Qualität und die optionale Suche in Unterordnern.
`setup.exe` installiert die App ohne Administratorrechte; alternativ die ZIP entpacken.

## Viewing shortcuts

- Ctrl+O: open image; Ctrl+Shift+O: open folder.
- Left/Right: previous/next image; mouse wheel: zoom; drag: pan.
- F: fit; 1: actual size; double-click: switch between the two.
- Ctrl+S: convert/save a copy; Ctrl+C: copy image to clipboard (white background).
- M: mirror; R: rotate; F11: fullscreen; Space: slideshow.
- I: details; T: filmstrip; F1: keyboard help.
- Esc: close help/settings/fullscreen/slideshow or return to the gallery.
- Tab / Shift+Tab: move focus; Enter: activate the focused button.

Viewing, zooming, rotating and favoriting do not change your original files.

## Local preferences

Folders, favorites, viewing preferences and recent images are stored in
`%LOCALAPPDATA%\PAFPhotos\settings.ini`. Existing Photo VLC / Foto VLC settings are migrated.
No account or telemetry is required. Removing a gallery folder does not delete
its files. Clear the recent-image list in Settings.

See **FORMATS.md** and **CODECS.md** for actual supported formats and limits.
New versions: https://github.com/Lyvt-Dev/PAF-Photos/releases
MIT license: LICENSE. Third-party terms: THIRD_PARTY_NOTICES.md and codecs/NOTICE.txt.

Image conversion runs in the background; viewing controls stay responsive during encoding.

## Updates

Installed apps check GitHub on startup and daily, download and verify new setups, then
install on exit. Turn this off in Settings, or use Check for updates / Install now.
Portable apps open the release page to download updates. Coming from 0.4.0? Run the
new setup once; older versions cannot update automatically. The P logo is retained,
but application branding, shortcuts and executable names now use PAF-Photos.
