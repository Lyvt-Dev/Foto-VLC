"""Package the exact upstream source/build trees for the unmodified portable codec runtime."""
import concurrent.futures
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
import urllib.request
import zipfile

VERSION = "7.1.2-33"
DEPENDENCIES = "2026.10.09.2225"
CONFIGURE = "2026.09.21.1729"
ROOT = Path("build/codec-source-archives")
ROOT.mkdir(parents=True, exist_ok=True)

def fetch(repo, ref):
    target = ROOT / (repo + "-" + ref + ".tar.gz")
    if not target.exists():
        url = "https://codeload.github.com/ImageMagick/" + repo + "/tar.gz/" + ref
        with urllib.request.urlopen(url, timeout=90) as response:
            target.write_bytes(response.read())
    return target

def member_text(archive, suffix):
    # Read build metadata as data; do not execute any upstream scripts.
    with tarfile.open(archive) as tree:
        name = next(x for x in tree.getnames() if x.endswith(suffix))
        return tree.extractfile(name).read().decode()

base = [("ImageMagick", VERSION), ("Dependencies", DEPENDENCIES), ("Configure", CONFIGURE)]
archives = [fetch(*entry) for entry in base]
recipe = member_text(archives[1], "/clone-dependencies.sh")
components = list(dict.fromkeys(re.findall(r"clone '([^']+)' '([0-9a-f]{40})'", recipe)))
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    archives.extend(pool.map(lambda entry: fetch(*entry), components))
Path("dist").mkdir(exist_ok=True)
with zipfile.ZipFile("dist/PhotoVLC-Codec-Sources.zip", "w", compression=zipfile.ZIP_STORED) as output:
    manifest = []
    for archive, (repo, ref) in zip(archives, base + components):
        output.write(archive, "archives/" + archive.name)
        manifest.append({"repository": "ImageMagick/" + repo, "ref": ref,
                         "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()})
    output.writestr("MANIFEST.json", json.dumps(manifest, indent=2))
    output.writestr("README.txt", """Corresponding source for ImageMagick 7.1.2-33 portable Q8 x64.
Photo VLC invokes this unmodified upstream program as a separate process.
These archives contain the original source, copyright/license notices, and build
scripts for ImageMagick, its Configure tool and all pinned dependency trees.
No source patches were applied by Photo VLC.

Extract ImageMagick and Configure alongside Dependencies. Extract component
archives beneath Dependencies/Dependencies using their repository names, as
specified in Dependencies/clone-dependencies.sh. Follow the Windows build
instructions in the ImageMagick and Dependencies source trees. The Configure
source and ImageMagick/.github/build/windows scripts supply project generation,
compiler configuration and link settings. The portable binary was built upstream;
Photo VLC does not claim to have built it.

Upstream binary and SBOM:
https://github.com/ImageMagick/ImageMagick/releases/tag/7.1.2-33
Build dependency release:
https://github.com/ImageMagick/Dependencies/releases/tag/2026.10.09.2225
Source manifests pin every component and record archive SHA-256 values.
License texts for the binary runtime are in codecs/LICENSE.txt and
codecs/NOTICE.txt in PhotoVLC-Windows-x64.zip; each source archive includes
its original licensing information.
""")
print("Packaged", len(archives), "source trees:", Path("dist/PhotoVLC-Codec-Sources.zip").stat().st_size, "bytes")
