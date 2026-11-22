"""Build local installable purpose modules; never opens a device or uploads."""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,tempfile,contextlib
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--arduino-cli',default=shutil.which('arduino-cli') or '/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli')
p.add_argument('--family',choices=['esp32s3'],default='esp32s3')
p.add_argument('--profile',choices=['desktop','gym','all'],default='all')
a=p.parse_args();catalog=json.loads((ROOT/'device_profiles/catalog.json').read_text())
for module in catalog['profiles']:
 if a.profile not in ('all',module['id']):continue
 if a.family not in module['installer_families']:continue
 source=ROOT/module['source'];dest=ROOT/'device_profiles/build'/a.family/module['id'];dest.mkdir(parents=True,exist_ok=True)
 cache=Path(tempfile.gettempdir())/('xiao-module-'+ROOT.name+'-'+a.family+'-'+module['id']);cache.mkdir(parents=True,exist_ok=True)
 with contextlib.nullcontext(str(cache)) as tmp:
  subprocess.run([a.arduino_cli,'compile','--fqbn',catalog['targets'][a.family]['fqbn'],'--libraries',str(ROOT/'libraries'),'--build-path',tmp,str(source)],check=True)
  app=Path(tmp)/(source.name+'.ino.bin');partition=Path(tmp)/(source.name+'.ino.partitions.bin')
  shutil.copyfile(app,dest/'application.bin');shutil.copyfile(partition,dest/'partitions.bin')
  info={'schema_version':1,'profile':module['id'],'family':a.family,'chip':catalog['targets'][a.family]['chip'],'offset':65536,
        'ota':1,'elf_sha256':app.read_bytes()[176:208].hex(),
        'files':{n:hashlib.sha256((dest/n).read_bytes()).hexdigest() for n in ('application.bin','partitions.bin')}}
  (dest/'manifest.json').write_text(json.dumps(info,indent=2)+'\n')
 print('Built module:',module['name'],a.family,flush=True)
