# Formatstatus in v0.1.0

Alle **194 unterschiedlichen Endungen** der Anfrage sind erfasst. Die App prüft
zuerst den Dateiinhalt mit Windows WIC, dann die eingebetteten Decoder und
anschließend – sofern installiert – ImageMagick. Eine Endung ist kein Codec.

- **WIC**: Windows-eigene Decoder; zusätzliche installierte WIC-Codecs können
  mehr Formate öffnen. APNG wird als statisches PNG angezeigt.
- **Eingebettet**: vorhandener Decoder mit den genannten Einschränkungen.
- **Optional / nicht zugesichert**: nur wenn die konkrete ImageMagick-Installation
  einen geeigneten Decoder/Delegate enthält. Kein Format dieser Gruppe wurde hier
  durch das bloße Eintragen in die Liste als funktionsfähig bestätigt.
- **Nicht nativ unterstützt**: kein eigener Decoder in v0.1; kein Versprechen,
  dass ImageMagick das Format lesen kann. Häufig proprietär, ein Projektformat,
  ein Cloud-Dokument, ein uneindeutiges Dateiformat oder ein Container.

Bei RAW-Dateien hängen Kameramodell und Variante vom Decoder ab. PDF/PS/EPS/AI
benötigen häufig Ghostscript; nur kompatible PDF-/PostScript-Varianten von AI sind
als Vorschau denkbar. Bei Designprojekten entspricht eine mögliche Vorschau nicht
dem Rendern aller Ebenen, Schriften, Effekte und verknüpften Ressourcen.

GIF/Animationen und mehrseitige Dateien zeigen **nur die erste decodierbare Seite**.
„RAW“ ist außerdem kein einheitliches standardisiertes Dateiformat. `.fig` kann
unter anderem ein Figma- oder Xfig-Dokument sein; `.tex` kann Bilddaten oder Text
enthalten. `.emf+` bezeichnet oft Inhalt innerhalb einer EMF-Datei.

| Endung | Decoder / Status |
| --- | --- |
| `.3fr` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.afdesign` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.afphoto` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.ai` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.apng` | WIC (statisches erstes Bild) |
| `.art` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.arw` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ase` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.aseprite` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.astc` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.atf` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.avif` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.bay` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.bclim` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.bflim` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.bli` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.blp` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.bmp` | WIC (statisches erstes Bild) |
| `.bnr` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.bntx` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.bpg` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.bti` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.bw` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.cap` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.cdr` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.cdt` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.cgm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.cmx` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.cpt` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.cr2` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.cr3` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.crw` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.cur` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.cut` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.dcr` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.dcx` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.dds` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.dib` | WIC (statisches erstes Bild) |
| `.dng` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.dwg` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.dxf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.eip` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.emf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.emf+` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.eps` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.epsf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.epsi` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.erf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.fff` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.fh` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh10` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh11` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh3` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh4` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh5` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh7` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh8` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fh9` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.fig` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.flif` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.fpx` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.gif` | WIC (statisches erstes Bild) |
| `.gim` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.gnf` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.gtf` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.gtx` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.gxt` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.hdp` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.heic` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.heics` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.heif` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.heifs` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.hpgl` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ico` | WIC (statisches erstes Bild) |
| `.idml` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.iff` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.iiq` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ilbm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.indd` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.iwi` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.j2c` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.j2k` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jfif` | WIC (statisches erstes Bild) |
| `.jng` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jp2` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jpc` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jpe` | WIC (statisches erstes Bild) |
| `.jpeg` | WIC (statisches erstes Bild) |
| `.jpf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jpg` | WIC (statisches erstes Bild) |
| `.jpm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jpx` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jxl` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.jxr` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.k25` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.kdc` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.kra` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ktx` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ktx2` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.lbm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.mac` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.mdc` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.mef` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.miff` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.mj2` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.mng` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.mos` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.mpc` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.mrw` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.msp` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.nef` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.nrw` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.obm` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.odg` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ora` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.orf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ota` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pam` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pbm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pcc` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pcd` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pct` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pcx` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pdf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pdn` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.pef` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pfm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pgm` | stb_image: binäres P5 |
| `.pic` | stb_image: Softimage PIC, nicht jedes Format mit dieser Endung |
| `.pict` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pjp` | WIC (statisches erstes Bild) |
| `.pjpeg` | WIC (statisches erstes Bild) |
| `.plt` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.png` | WIC (statisches erstes Bild) |
| `.pnm` | stb_image: binäres P5/P6 |
| `.ppm` | stb_image: binäres P6 |
| `.procreate` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.ps` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.psb` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.psd` | stb_image: unterstützte 8/16-Bit-Komposite, keine Ebenen |
| `.psdc` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.ptx` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pvr` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.pxr` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.qoi` | eingebetteter QOI-Decoder |
| `.qtk` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.raf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.ras` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.raw` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.rd` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.rgb` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.rgba` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.rw1` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.rw2` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.rwl` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.sgi` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.sketch` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.sr2` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.srf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.srw` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.sti` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.sun` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.svg` | NanoSVG: Formen und Verläufe; kein Text, keine Skripte, keine vollständige SVG-Spezifikation |
| `.svgz` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.swf` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.sxd` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.targa` | stb_image |
| `.tex` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.tex0` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.texture` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.tga` | stb_image |
| `.tif` | WIC (statisches erstes Bild) |
| `.tiff` | WIC (statisches erstes Bild) |
| `.tim` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.tim2` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.tpl` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.vif` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.viff` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.vmt` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.vsd` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.vsdx` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.vtf` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.wad` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.wal` | Nicht nativ unterstützt; externer Decoder nicht zugesichert |
| `.wbmp` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.wdp` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.webp` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.wmf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.wpg` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.x3f` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.xbm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.xcf` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.xpm` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
| `.xwd` | Optional / nicht zugesichert: installierter WIC-Codec oder ImageMagick |
