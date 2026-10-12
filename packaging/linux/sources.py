#!/usr/bin/env python3
"""Archive exact distribution sources for the runtime packages recorded in the AppImage."""
import os, pathlib, subprocess, tarfile, json, hashlib, shutil, urllib.request, urllib.parse, email
root=pathlib.Path(__file__).resolve().parents[2];out=root/'dist/linux';directory=out/'sources';directory.mkdir(parents=True,exist_ok=True)
packages=set()
for line in (out/'linux-dependencies.tsv').read_text().splitlines()[1:]:
 fields=line.split('\t')
 if len(fields)==4 and fields[2] and fields[3]:packages.add(fields[2]+'='+fields[3])
def archived_source(package):
 name,version=package.split('=',1)
 base='https://launchpad.net/ubuntu/+archive/primary/+sourcefiles/'+urllib.parse.quote(name,safe='')+'/'+urllib.parse.quote(version,safe='')+'/'
 filename=name+'_'+version.split(':')[-1]+'.dsc'
 with urllib.request.urlopen(base+urllib.parse.quote(filename,safe=''),timeout=60) as response:
  if not response.geturl().startswith('https://'):raise ValueError('Source metadata redirected outside HTTPS')
  data=response.read(1024*1024)
 text=data.decode('utf-8')
 if text.startswith('-----BEGIN PGP SIGNED MESSAGE-----'):text=text.split('\n\n',1)[1].split('-----BEGIN PGP SIGNATURE-----',1)[0]
 metadata=email.message_from_string(text)
 if metadata['Source']!=name or metadata['Version']!=version:raise ValueError('Archived source identity mismatch')
 (directory/filename).write_bytes(data)
 records=metadata['Checksums-Sha256']
 if not records:raise ValueError('Archived source has no SHA256 metadata')
 for line in records.strip().splitlines():
  expected,size,filename=line.split();size=int(size)
  if pathlib.Path(filename).name!=filename or size>512*1024*1024:raise ValueError('Invalid source archive name/size')
  target=directory/filename
  if target.exists() and target.stat().st_size==size and hashlib.sha256(target.read_bytes()).hexdigest()==expected:continue
  partial=target.with_suffix(target.suffix+'.part');digest=hashlib.sha256();count=0
  with urllib.request.urlopen(base+urllib.parse.quote(filename,safe=''),timeout=120) as response,partial.open('wb') as output:
   if not response.geturl().startswith('https://'):raise ValueError('Source archive redirected outside HTTPS')
   while chunk:=response.read(1024*1024):
    count+=len(chunk)
    if count>size:raise ValueError('Source archive exceeds signed metadata size')
    output.write(chunk);digest.update(chunk)
  if count!=size or digest.hexdigest()!=expected:raise ValueError('Archived source checksum mismatch')
  partial.replace(target)
 print('Preserved official Ubuntu archived source',package)
for package in sorted(packages):
 if subprocess.run(['/usr/bin/apt-get','source','--download-only',package],cwd=directory).returncode:
  # Runner libraries may predate the latest source index. Preserve the exact
  # corresponding signed .dsc and checksum-verified sources from Ubuntu's archive.
  archived_source(package)
urllib.request.urlretrieve('https://github.com/AppImage/type2-runtime/archive/8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa.tar.gz',directory/'appimage-runtime-source.tar.gz')
jxl_source=os.environ.get('PAF_JXL_SOURCE')
if jxl_source:
 shutil.copytree(jxl_source,directory/'libjxl-v0.11.1',dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git'))
 (directory/'libjxl-revision.txt').write_text(subprocess.check_output(['git','-C',jxl_source,'rev-parse','HEAD'],text=True)+subprocess.check_output(['git','-C',jxl_source,'submodule','status'],text=True))
# Qt modules are redistributed dynamically; keep upstream source and distro patches.
for name in ('src','third_party','assets','packaging','tests','docs'):
 shutil.copytree(root/name,directory/'PAF-Photos'/name,dirs_exist_ok=True)
for name in ('CMakeLists.txt','LICENSE','THIRD_PARTY_NOTICES.md','README.md','FORMATS.md'):
 if (root/name).exists():shutil.copy2(root/name,directory/'PAF-Photos'/name)
shutil.copy2(out/'linux-dependencies.tsv',directory/'linux-dependencies.tsv')
shutil.copy2(out/'linux-build.json',directory/'linux-build.json')
manifest={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.iterdir() if p.is_file()};(directory/'checksums.json').write_text(json.dumps(manifest,indent=2))
with tarfile.open(out/'PAFPhotos-Linux-Sources.tar.xz','w:xz') as archive:archive.add(directory,arcname='PAFPhotos-Linux-Sources')
print('Archived',len(packages),'exact distribution source packages')
