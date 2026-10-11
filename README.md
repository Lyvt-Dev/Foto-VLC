# Foto VLC

Ein kleiner, nativer Foto-Betrachter für **Windows 10/11 x64**, geschrieben in
C++17. Dunkle Oberfläche, keine Browser-Runtime, kein Account und keine Telemetrie.

> **Foto VLC 0.2.0** ist ein Foto-Betrachter. Die Unterstützung für jedes
> Design-, CAD- oder Spieleformat ist noch nicht vollständig. Alle 194 Endungen sind im
> Dateifilter erfasst; die tatsächliche Unterstützung steht in
> [docs/formats.md](docs/formats.md).

![Foto VLC mit geöffnetem Bild](docs/screenshot.png)

Die Abbildung zeigt den tatsächlichen Windows-Build unter Wine.
Bildnachweise für die gezeigten Testfotos: [docs/screenshots.md](docs/screenshots.md).

## Download

Unter [Releases](https://github.com/Lyvt-Dev/Foto-VLC/releases) gibt es nach einem
bestandenen Release-Build `FotoVLC.exe` und ein portables ZIP mit Lizenz- und
Formatinformationen. Die EXE benötigt keine Installation und keine zusätzliche
C++-Runtime. Der Build ist bislang nicht mit einem Code-Signing-Zertifikat
signiert. Die Prüfsumme steht im jeweiligen Release.

## Funktionen

- Datei oder Ordner öffnen; Dateien und Ordner per Drag-and-drop laden.
- Filmstreifen mit echten Vorschaubildern und natürlicher Dateisortierung (Bild 2 vor Bild 10).
- Startseite mit den vier zuletzt geöffneten Bildern.
- Kompakte Werkzeugleiste, native Windows-Schaltflächen und Hinweise beim Darüberfahren.
- Anpassen, Originalgröße, Zoom auf die Mausposition und Verschieben durch Ziehen.
- Doppelklick wechselt zwischen angepasster Ansicht und Originalgröße.
- In 90°-Schritten drehen, Vollbild, Diashow mit vier Sekunden pro Bild.
- Bilddetails bei Bedarf einblenden: Dateiname, Format, Größe, Auflösung und Änderungsdatum.
- Datei im Explorer anzeigen oder ihren Pfad kopieren.
- Sichtbare Versionsnummer und eigene Hilfeansicht mit Tastenkürzeln und Formatübersicht.
- Fenstergröße, Detailansicht und Filmstreifen werden für den nächsten Start gespeichert.
- Transparenzraster, Unicode-Dateinamen und an die Display-Skalierung angepasste UI.
- Bilder im Hintergrund laden; veraltete Ladeergebnisse werden verworfen.
- PNG, JPEG/JFIF, BMP/DIB, GIF, TIFF, ICO über Windows WIC.
- SVG-Formen, PSD-Komposit, TGA, PGM/PPM, PIC und QOI über eingebettete Decoder.
- Optionaler ImageMagick-Decoder für zusätzliche Formate; keine automatischen Downloads.

Originaldateien werden nicht verändert. Drehen und Zoom betreffen nur die Anzeige.
GIFs, APNGs und mehrseitige Dateien zeigen die erste decodierbare Seite; Animation
und Ebenenbearbeitung gehören nicht zu v0.2. Die Diashow wechselt zwischen Dateien.

## Bedienung

| Aktion | Taste |
| --- | --- |
| Datei / Ordner öffnen | Strg+O / Strg+Umschalt+O |
| Vorheriges / nächstes Bild | ← / → |
| Einpassen / 1:1 | F / 1 |
| Zoom | Mausrad oder + / − |
| Drehen / Informationen | R / I |
| Filmstreifen | T |
| Zwischen Anpassen und 1:1 wechseln | Doppelklick |
| Hilfe | F1 |
| Vollbild | F11 |
| Diashow | Leertaste |
| Hilfe, Vollbild oder Diashow verlassen | Esc |
| Schaltflächen auswählen / auslösen | Tab, Umschalt+Tab / Enter |

Ein Dateipfad kann auch als Startargument übergeben werden:

```powershell
.\FotoVLC.exe 'C:\Fotos\Mein Bild.png'
```

## Zusätzliche Decoder

Für weitere Formate kann eine aktuelle **ImageMagick-7-Installation** verwendet
werden: https://imagemagick.org/script/download.php#windows . Foto VLC findet
`magick.exe` im PATH oder über die Variable `FOTO_VLC_MAGICK`.

```powershell
$env:FOTO_VLC_MAGICK = 'C:\Program Files\ImageMagick-7.x.x-Q16-HDRI\magick.exe'
.\FotoVLC.exe
```

Das ist ein Beispielpfad; verwende den tatsächlichen Installationspfad. Unter
„Info & Hilfe → Dateiformate“ steht, ob ImageMagick gefunden wurde. Mit
`magick identify -list format` kann die installierte Decoderliste geprüft werden.
PDF/EPS/PS und manche AI-Dateien brauchen ggf. Ghostscript. Die App ändert keine
ImageMagick-Sicherheitsrichtlinien. Formate, für die kein Decoder vorhanden ist,
zeigen eine verständliche Fehlermeldung.

## Lokale Einstellungen

Ansichtsoptionen und zuletzt erfolgreich geöffnete Bilder liegen ausschließlich
auf diesem Gerät unter `%LOCALAPPDATA%\FotoVLC\settings.ini`. Die Liste lässt
sich im Menü „Weitere Optionen → Liste »Zuletzt geöffnet« leeren“ löschen.
Die Unicode-Datei wird atomar ersetzt, damit auch internationale Dateinamen
korrekt erhalten bleiben. Es werden keine Bilder in die App kopiert.

## Selbst bauen

Mit Visual Studio 2022 Build Tools, Windows SDK und CMake ≥ 3.20:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
$p = Start-Process build/Release/FotoVLC.exe -ArgumentList '--self-test','windows-self-test.txt' -Wait -PassThru
Get-Content windows-self-test.txt
if ($p.ExitCode -ne 0) { throw 'Windows self-test failed' }
```

Die EXE liegt anschließend in `build/Release/FotoVLC.exe`. MSVC-Runtime und die
kleinen Decoder werden statisch eingebunden; nur Windows-System-DLLs werden benötigt.

Unter Linux lassen sich die portablen Decoder- und Core-Tests ausführen:

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

Ein Windows-Cross-Build mit installiertem MinGW-w64 ist ebenfalls möglich:

```sh
cmake -S . -B build/mingw -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_BUILD_TYPE=Release
cmake --build build/mingw --parallel 2
```

Der GitHub-Workflow prüft die portablen Tests sowie die echten WIC-Decoder,
Pixelwerte, Unicode-Pfade, GDI+-Darstellung, Vorschaubilder, native Klick- und
Tastaturbedienung, Einstellungen und Versionsinformationen auf Windows.
Bei einem `v*`-Tag veröffentlicht er erst nach bestandenen Tests ein Release.

## Grenzen

Native WIC-Vorschauen werden auf maximal 32 Megapixel / 16.384 Pixel pro Achse
begrenzt. Die eingebetteten Rasterdecoder akzeptieren höchstens 32 Megapixel,
SVG wird auf höchstens 6.000 Pixel pro Achse skaliert. ImageMagick-Vorschauen
sind ebenfalls maximal 6.000 × 6.000 Pixel groß und laufen in einem separaten
Windows-Job mit Speicherlimit und 20-Sekunden-Timeout. Es handelt sich um
Ressourcenbegrenzung, keine vollständige Sandbox.

Die App verarbeitet maximal 512 MiB große Eingabedateien; die eingebetteten
Decoder haben ein 128-MiB-Dateilimit, SVG ein 8-MiB-Limit. Keine garantierte
farbverbindliche Darstellung, keine HDR-Ausgabe, kein Export, kein Editor.
Die Oberfläche verwendet native Windows-Schaltflächen mit zugänglichen Namen
und Tastaturfokus. Eine vollständige Screenreader-Prüfung steht noch aus.

[MIT-Lizenz](LICENSE) · [Drittanbieter-Lizenzen](THIRD_PARTY_NOTICES.md)
