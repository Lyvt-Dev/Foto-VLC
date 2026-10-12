#!/usr/bin/env python3
"""Enable corresponding Ubuntu/PPA source indexes, keeping repository signatures."""
import pathlib,re
root=pathlib.Path('/etc/apt');official=re.compile(r'ubuntu\.com|launchpad(?:content)?\.net')
for file in [root/'sources.list',*(root/'sources.list.d').glob('*.list')]:
 if not file.exists():continue
 rows=[]
 for row in file.read_text().splitlines():
  if row.startswith('deb ') and official.search(row):rows.append(re.sub('^deb ','deb-src ',row))
 if rows:(root/'sources.list.d'/('paf-source-'+file.name)).write_text('\n'.join(rows)+'\n')
for file in (root/'sources.list.d').glob('*.sources'):
 blocks=[]
 for block in file.read_text().split('\n\n'):
  if official.search(block):
   block=re.sub(r'^Types:\s*deb\s*$', 'Types: deb deb-src',block,flags=re.M)
  blocks.append(block)
 file.write_text('\n\n'.join(blocks))
