# Foto VLC 0.2.0

Ein portabler Foto-Betrachter für Windows 10/11 (64-Bit).

## Loslegen

1. ZIP in einen Ordner entpacken.
2. `FotoVLC.exe` starten.
3. „Bild öffnen“ oder „Ordner öffnen“ auswählen. Du kannst Dateien auch direkt ins Fenster ziehen.

Der Filmstreifen zeigt die Bilder im aktuellen Ordner. Klicke auf eine Vorschau
oder benutze die Pfeiltasten, um zum nächsten Bild zu wechseln. „Details“ zeigt
Größe, Auflösung und weitere Dateiinformationen.

## Die wichtigsten Handgriffe

- **Strg+O**: Bild öffnen; **Strg+Umschalt+O**: Ordner öffnen.
- **Mausrad**: zur Mausposition zoomen; **Ziehen**: vergrößertes Bild verschieben.
- **F**: Bild anpassen; **1**: Originalgröße; **Doppelklick**: zwischen beiden Ansichten wechseln.
- **R**: drehen; **F11**: Vollbild; **Leertaste**: Diashow.
- **I**: Details; **T**: Filmstreifen; **F1**: Hilfe.
- **Esc**: Hilfe, Vollbild oder Diashow verlassen.

Die Schaltflächen lassen sich auch mit Tab auswählen und mit Enter auslösen.
Beim Drehen und Zoomen werden deine Originaldateien nicht verändert.

## Verlauf und Einstellungen

Die Startseite merkt sich bis zu vier zuletzt geöffnete Bilder. Unter
„Weitere Optionen → Liste »Zuletzt geöffnet« leeren“ kannst du diese Liste löschen.
Ansichtsoptionen und Verlauf werden lokal in `%LOCALAPPDATA%\FotoVLC\settings.ini`
gespeichert. Es ist kein Konto erforderlich.

## Formate und Updates

Die konkrete Formatunterstützung steht in **FORMATS.md**. Zusätzliche Windows-Codecs
und optional ImageMagick können weitere Formate öffnen. Die Liste der Dateiendungen
bedeutet keine vollständige Unterstützung aller proprietären Projektformate.

Die installierte Version steht oben in der App und unter „Über die App“.
Neue Versionen: https://github.com/Lyvt-Dev/Foto-VLC/releases
Quellcode und vollständige Anleitung: https://github.com/Lyvt-Dev/Foto-VLC

MIT-Lizenz: siehe LICENSE. Weitere Lizenztexte sind in diesem ZIP enthalten.
