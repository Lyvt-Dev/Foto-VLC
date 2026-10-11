Foto VLC 0.2.0 überarbeitet die Oberfläche und macht die tägliche Bildbetrachtung einfacher.

- Ruhige Fotoansicht mit kompakter Werkzeugleiste, eigenen Symbolen und klarer Navigation.
- Filmstreifen mit echten Vorschaubildern; Klick auf eine Vorschau öffnet das Bild.
- Startseite mit zuletzt geöffneten Bildern und direktem Datei-/Ordnerzugriff.
- Native Windows-Schaltflächen mit Tooltips, Tastaturfokus und zugänglichen Namen.
- Zoom auf die Mausposition, Zoomauswahl und Doppelklick für Originalgröße/Anpassen.
- Optionale Bilddetails, „Im Explorer zeigen“ und „Dateipfad kopieren“.
- Natürliche Dateisortierung; „Bild 2“ steht vor „Bild 10“.
- Gespeicherte Fenstergröße, Ansichtsoptionen und Unicode-sicherer lokaler Verlauf.
- Version 0.2.0 sichtbar in der App, unter „Über die App“ und in den EXE-Eigenschaften.
- Eigene Hilfeansicht mit Tastenkürzeln, Formatinformationen und Link zu neuen Versionen.

Die Windows-App bleibt portabel und benötigt keine zusätzliche C++-Runtime.
Das Release wird erst nach bestandenen Decoder- und Windows-Laufzeittests veröffentlicht.

Lade `FotoVLC-Windows-x64.zip` für die App mit Lizenz- und Formatinformationen
oder `FotoVLC.exe` für die einzelne Datei herunter. `SHA256SUMS.txt` enthält die
EXE-Prüfsumme; `windows-self-test.txt` dokumentiert die Windows-Tests.

**Formatgrenzen:** Die 194 Endungen im Dateifilter sind keine Garantie für 194
funktionierende Decoder. Proprietäre Design-/CAD-Projekte und Spielecontainer
sind weiterhin nicht durchgehend unterstützt. GIF/APNG und mehrseitige Dateien
zeigen das erste Bild; Animation und Ebenenbearbeitung sind nicht enthalten.
Die EXE ist nicht code-signiert.
