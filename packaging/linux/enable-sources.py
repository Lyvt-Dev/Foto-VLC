#!/usr/bin/env python3
"""Enable corresponding Ubuntu/PPA source indexes, keeping repository signatures."""
import pathlib,re
root=pathlib.Path('/etc/apt');official=re.compile(r'ubuntu\.com|launchpad(?:content)?\.net|mirror\+file:\S*/ubuntu[^\s]*\.list')
for file in [root/'sources.list',*(root/'sources.list.d').glob('*.list')]:
 if not file.exists() or file.name.startswith('paf-source-'):continue
 rows=[]
 for row in file.read_text().splitlines():
  if row.startswith('deb ') and official.search(row):rows.append(re.sub('^deb ','deb-src ',row))
 if rows:
  (root/'sources.list.d'/('paf-source-'+file.name)).write_text('\n'.join(rows)+'\n');print('Enabled source indexes from',file.name,len(rows))
for file in (root/'sources.list.d').glob('*.sources'):
 blocks=[]
 for block in file.read_text().split('\n\n'):
  if official.search(block):
   block=re.sub(r'^Types:\s*deb\s*$', 'Types: deb deb-src',block,flags=re.M)
  blocks.append(block)
 file.write_text('\n\n'.join(blocks))

# Hosted runner mirror configurations vary. Also register the standard signed
# Ubuntu source repositories explicitly, using the runner's existing keyring.
os_release=(pathlib.Path('/etc/os-release')).read_text()
codename=re.search(r'^VERSION_CODENAME=(.+)$',os_release,re.M)
if not codename or codename.group(1).strip('"')!='jammy':
 raise RuntimeError('This release source recipe expects the Ubuntu 22.04 runner')
rows=[f'deb-src https://archive.ubuntu.com/ubuntu jammy{suffix} main restricted universe multiverse' for suffix in ('','-updates')]
rows.append('deb-src https://security.ubuntu.com/ubuntu jammy-security main restricted universe multiverse')
(root/'sources.list.d/paf-source-ubuntu.list').write_text('\n'.join(rows)+'\n')
print('Enabled official signed Ubuntu 22.04 source repositories')
