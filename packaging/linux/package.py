#!/usr/bin/env python3
"""Build Linux desktop packages from a tested native build; no dependency downloads."""
import argparse, pathlib, shutil, subprocess, os, re, hashlib, json
p=argparse.ArgumentParser();p.add_argument('--build',default='build/linux');p.add_argument('--output',default='dist/linux');p.add_argument('--appimagetool',required=True);p.add_argument('--runtime-file',required=True);p.add_argument('--qt-root',default='/usr');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];out=(root/a.output).resolve();out.mkdir(parents=True,exist_ok=True)
version=re.search(r'set\(PAF_RELEASE_VERSION "([^\"]+)"\)',(root/'CMakeLists.txt').read_text()).group(1)
app=out/'PAFPhotos.AppDir';app.mkdir(exist_ok=True);binary=app/'usr/bin';libs=app/'usr/lib';plugins=app/'usr/plugins';binary.mkdir(parents=True,exist_ok=True);libs.mkdir(parents=True,exist_ok=True)
shutil.copy2(root/a.build/'PAFPhotos',binary/'PAFPhotos')
shutil.copy2(root/a.build/'PAFPhotos.JXL',binary/'PAFPhotos.JXL')
original_modules=[]
qtroots=[pathlib.Path('/usr'),pathlib.Path(a.qt_root)]
for qtroot in qtroots:
 for base in [qtroot/'lib/x86_64-linux-gnu/qt6/plugins',qtroot/'lib/qt6/plugins']:
  if base.exists():
   for category in ('platforms','imageformats','platforminputcontexts','wayland-shell-integration','wayland-graphics-integration-client','xcbglintegrations','tls'):
    if (base/category).exists():
     original_modules.extend((base/category).rglob("*.so"));shutil.copytree(base/category,plugins/category,dirs_exist_ok=True)
# Delegates are isolated command-line tools; preserve all notices and configuration.
for name in ('magick','convert','identify','djxl','dcraw_emu','pdftoppm','gs'):
 src=str(pathlib.Path('/usr/bin')/name) if (pathlib.Path('/usr/bin')/name).is_file() else (shutil.which(name) or str(pathlib.Path(a.qt_root)/'bin'/name))
 if pathlib.Path(src).is_file():shutil.copy2(pathlib.Path(src).resolve(),binary/name)
for prefix in qtroots:
 for base in (prefix/'lib/x86_64-linux-gnu',prefix/'lib'):
  for module in base.glob('ImageMagick*/modules-*/coders'):
   target=libs/'im-coders';shutil.copytree(module,target,dirs_exist_ok=True)
 for conf in (prefix.parent/'etc').glob('ImageMagick*'):
  shutil.copytree(conf,app/'etc'/conf.name,dirs_exist_ok=True)
 for folder in ('ghostscript','poppler'):
  source=prefix/'share'/folder
  if source.exists():shutil.copytree(source,app/'usr/share'/folder,dirs_exist_ok=True,ignore_dangling_symlinks=True)
# Recursively bundle dynamically linked dependencies, excluding host libc/loader.
exclude=re.compile(r'^(ld-linux|libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|libresolv\.so|libnss_|libutil\.so)')
seen=set();package_names=set();owners=set();
for source in original_modules:
 owner=subprocess.run(['dpkg-query','-S',str(source.resolve())],capture_output=True,text=True)
 if owner.returncode==0:package_names.add(owner.stdout.split(': /')[0].strip())
todo=list(binary.iterdir())+list(plugins.rglob('*.so'))+list(libs.rglob('*.so'))
while todo:
 file=todo.pop()
 if str(file) in seen:continue
 seen.add(str(file))
 result=subprocess.run(['ldd',str(file)],capture_output=True,text=True)
 if 'not found' in result.stdout:raise RuntimeError('Missing dependency: '+str(file)+'\n'+result.stdout)
 for line in result.stdout.splitlines():
  match=re.search(r'=>\s+(/\S+)',line)
  if not match:continue
  source=pathlib.Path(match.group(1));name=source.name
  if str(source.resolve()) not in owners and not exclude.match(name):
   owners.add(str(source.resolve()));owner=subprocess.run(['dpkg-query','-S',str(source.resolve())],capture_output=True,text=True)
   if owner.returncode==0:package_names.add(owner.stdout.split(': /')[0].strip())
  if exclude.match(name):continue
  target=libs/name
  if not target.exists():shutil.copy2(source.resolve(),target);todo.append(target)
# Preserve license texts of bundled libraries and delegate packages, including GPL/LGPL.
for file in list(binary.iterdir())+list(plugins.rglob('*.so')):
 original=str(pathlib.Path('/usr/bin')/file.name) if (pathlib.Path('/usr/bin')/file.name).is_file() else shutil.which(file.name)
 if original:
  owner=subprocess.run(['dpkg-query','-S',str(pathlib.Path(original).resolve())],capture_output=True,text=True)
  if owner.returncode==0:package_names.add(owner.stdout.split(': /')[0].strip())
notices=app/'usr/share/doc';notices.mkdir(parents=True,exist_ok=True)
for docs in pathlib.Path('/usr/share/doc').iterdir():
 if docs.name in {p.split(':')[0] for p in package_names} or any(key in docs.name.lower() for key in ('qt6','imagemagick','magick','ghostscript','libraw','jxl','poppler','libwebp','libheif','libavif','openexr','libpng','libjpeg')):
  if docs.is_dir():shutil.copytree(docs,notices/docs.name,dirs_exist_ok=True,symlinks=False,ignore_dangling_symlinks=True)
for name in ('LICENSE','THIRD_PARTY_NOTICES.md'):
 if (root/name).exists():shutil.copy2(root/name,notices/name)
jxl_source=os.environ.get('PAF_JXL_SOURCE')
if jxl_source:
 jxl_notice=notices/'libjxl-upstream';jxl_notice.mkdir(exist_ok=True)
 for source in pathlib.Path(jxl_source).rglob('*'):
  if source.is_file() and source.name.upper().startswith(('LICENSE','COPYING','PATENTS','AUTHORS')):
   target=jxl_notice/source.relative_to(jxl_source);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
records=[]
for package in sorted(package_names):
 result=subprocess.run(['dpkg-query','-W','-f=${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\n',package],capture_output=True,text=True)
 if result.returncode==0:records.append(result.stdout.strip())
(out/'linux-dependencies.tsv').write_text('package\tversion\tsource\tsource_version\n'+'\n'.join(records)+'\n')
shutil.copy2(out/'linux-dependencies.tsv',notices/'linux-dependencies.tsv')
(binary/'qt.conf').write_text('[Paths]\nPrefix=..\nPlugins=plugins\n')
shutil.copy2(root/'packaging/linux/paf-photos.desktop',app/'paf-photos.desktop');shutil.copy2(root/'assets/logo.png',app/'paf-photos.png')
(app/'AppRun').write_text('''#!/bin/sh
set -eu
paf_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$paf_dir/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$paf_dir/usr/plugins"
export PATH="$paf_dir/usr/bin:$PATH"
export MAGICK_CODER_MODULE_PATH="$paf_dir/usr/lib/im-coders"
export MAGICK_CONFIGURE_PATH="$paf_dir/etc/ImageMagick-6:$paf_dir/etc/ImageMagick-7"
for paf_gs in "$paf_dir"/usr/share/ghostscript/*; do
 if [ -d "$paf_gs/Resource" ]; then export GS_LIB="$paf_gs/Resource/Init:$paf_gs/lib"; fi
done
exec "$paf_dir/usr/bin/PAFPhotos" "$@"
''');(app/'AppRun').chmod(0o755)
image=out/'PAFPhotos-Linux-x86_64.AppImage'
env=os.environ.copy();env['ARCH']='x86_64';env['APPIMAGE_EXTRACT_AND_RUN']='1'
for path in app.rglob('*'):
 if path.is_dir():path.chmod(0o755)
 elif path.is_file():path.chmod(0o755 if path.parent==binary or path.name=='AppRun' else 0o644)
app.chmod(0o755)
subprocess.run([str(pathlib.Path(a.appimagetool).resolve()),'--no-appstream','--runtime-file',str(pathlib.Path(a.runtime_file).resolve()),str(app),str(image)],env=env,check=True)
# Debian package uses distribution-maintained libraries and delegates.
debroot=out/'deb';(debroot/'DEBIAN').mkdir(parents=True,exist_ok=True);(debroot/'usr/bin').mkdir(parents=True,exist_ok=True)
shutil.copy2(root/a.build/'PAFPhotos',debroot/'usr/bin/PAFPhotos')
shutil.copy2(root/a.build/'PAFPhotos.JXL',debroot/'usr/bin/PAFPhotos.JXL')
private=debroot/'usr/lib/paf-photos';private.mkdir(parents=True,exist_ok=True)
for file in libs.iterdir():
 if file.name.startswith(('libjxl','libhwy','libbrotli')):shutil.copy2(file,private/file.name)
shutil.copytree(notices,debroot/'usr/share/doc/paf-photos',dirs_exist_ok=True)
(debroot/'usr/share/applications').mkdir(parents=True,exist_ok=True);records=[]
for package in sorted(package_names):
 result=subprocess.run(['dpkg-query','-W','-f=${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\n',package],capture_output=True,text=True)
 if result.returncode==0:records.append(result.stdout.strip())
(out/'linux-dependencies.tsv').write_text('package\tversion\tsource\tsource_version\n'+'\n'.join(records)+'\n')
shutil.copy2(out/'linux-dependencies.tsv',notices/'linux-dependencies.tsv')
(binary/'qt.conf').write_text('[Paths]\nPrefix=..\nPlugins=plugins\n')
shutil.copy2(root/'packaging/linux/paf-photos.desktop',debroot/'usr/share/applications/paf-photos.desktop')
(debroot/'usr/share/icons/hicolor/256x256/apps').mkdir(parents=True,exist_ok=True);shutil.copy2(root/'assets/logo.png',debroot/'usr/share/icons/hicolor/256x256/apps/paf-photos.png')
(debroot/'DEBIAN/control').write_text(f'''Package: paf-photos
Version: {version}
Architecture: amd64
Maintainer: Lyvt-Dev <Lyvt-Dev@users.noreply.github.com>
Depends: libqt6widgets6, libqt6concurrent6, libqt6network6, qt6-image-formats-plugins, imagemagick, libmagickcore-6.q16-6-extra | libmagickcore-7.q16-10-extra, libraw-bin, poppler-utils, ghostscript, libglib2.0-bin
Description: PAF-Photos native photo gallery, editor and converter
 Native C++ Linux photo application with local image editing and broad decoders.
''')
for path in debroot.rglob('*'):
 if path.is_dir():path.chmod(0o755)
 elif path.is_file():path.chmod(0o755 if path.name in ('PAFPhotos','PAFPhotos.JXL') else 0o644)
debroot.chmod(0o755)
subprocess.run(['dpkg-deb','--build','--root-owner-group',str(debroot),str(out/f'PAFPhotos-Linux-x86_64.deb')],check=True)
files=[image,out/'PAFPhotos-Linux-x86_64.deb']
(out/'linux-SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in files))
(out/'linux-update.json').write_text(json.dumps(dict(version=version,sha256=hashlib.sha256(image.read_bytes()).hexdigest(),size=image.stat().st_size),separators=(',',':')))
print('Built',[(f.name,f.stat().st_size) for f in files])
