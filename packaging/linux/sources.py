#!/usr/bin/env python3
"""Archive exact distribution sources for the runtime packages recorded in the AppImage."""
import os, pathlib, subprocess, tarfile, json, hashlib, shutil, urllib.request
root=pathlib.Path(__file__).resolve().parents[2];out=root/'dist/linux';directory=out/'sources';directory.mkdir(parents=True,exist_ok=True)
packages=set()
for line in (out/'linux-dependencies.tsv').read_text().splitlines()[1:]:
 fields=line.split('\t')
 if len(fields)==4 and fields[2] and fields[3]:packages.add(fields[2]+'='+fields[3])
for package in sorted(packages):subprocess.run(['apt-get','source','--download-only',package],cwd=directory,check=True)
urllib.request.urlretrieve('https://github.com/AppImage/type2-runtime/archive/8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa.tar.gz',directory/'appimage-runtime-source.tar.gz')
jxl_source=os.environ.get('PAF_JXL_SOURCE')
if jxl_source:
 shutil.copytree(jxl_source,directory/'libjxl-v0.11.1',dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git'))
 (directory/'libjxl-revision.txt').write_text(subprocess.check_output(['git','-C',jxl_source,'rev-parse','HEAD'],text=True)+subprocess.check_output(['git','-C',jxl_source,'submodule','status'],text=True))
# Qt modules are redistributed dynamically; keep upstream source and distro patches.
for name in ('src','third_party','assets','packaging'):
 shutil.copytree(root/name,directory/'PAF-Photos'/name,dirs_exist_ok=True)
for name in ('CMakeLists.txt','LICENSE','THIRD_PARTY_NOTICES.md'):
 if (root/name).exists():shutil.copy2(root/name,directory/'PAF-Photos'/name)
shutil.copy2(out/'linux-dependencies.tsv',directory/'linux-dependencies.tsv')
manifest={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.iterdir() if p.is_file()};(directory/'checksums.json').write_text(json.dumps(manifest,indent=2))
with tarfile.open(out/'PAFPhotos-Linux-Sources.tar.xz','w:xz') as archive:archive.add(directory,arcname='PAFPhotos-Linux-Sources')
print('Archived',len(packages),'exact distribution source packages')
