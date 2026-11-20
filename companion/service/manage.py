#!/usr/bin/env python3
"""Install/manage XIAO Display Helper as a named per-user macOS app.

No privileged daemon, firmware changes or credential copying. Run with the
project's virtual environment after installing requirements.txt.
"""
import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from process_owner import stop_orphan

ROOT=Path(__file__).resolve().parents[2]
SERVICE=Path(__file__).resolve().parent
NAME='XIAO Display Helper'
LABEL='org.xiaodisplay.helper'
OLD_LABEL='org.xiaodisplay.updater'
DOMAIN=f'gui/{os.getuid()}'
TARGET=Path.home()/'Library/LaunchAgents'/f'{LABEL}.plist'
APP=Path.home()/'Applications'/f'{NAME}.app'
DATA=Path(os.environ.get('XIAO_DISPLAY_DATA_DIR',Path.home()/'Library/Application Support'/NAME))
LOGS=DATA/'logs'
EXECUTABLE=APP/'Contents/MacOS'/NAME
LSREGISTER='/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'

def configuration():
 # This launches the native menu UI, not just its data worker. Background
 # classification caps UI scheduling even when the user opens the menu.
 return {'Label':LABEL,'ProgramArguments':[str(EXECUTABLE)],
         'AssociatedBundleIdentifiers':[LABEL],'Comment':NAME,
         'WorkingDirectory':str(DATA),'RunAtLoad':True,'KeepAlive':True,'ThrottleInterval':30,
         'ProcessType':'Interactive','Umask':0o077,
         'EnvironmentVariables':{'PATH':'/usr/bin:/bin:/usr/sbin:/sbin','PYTHONUNBUFFERED':'1','PYTHONDONTWRITEBYTECODE':'1','XIAO_DISPLAY_DATA_DIR':str(DATA)},
         'StandardOutPath':str(LOGS/'updater.log'),'StandardErrorPath':str(LOGS/'updater-error.log')}

def prepare_runtime():
 # Login services must not depend on a virtualenv inside protected Documents.
 # Install the chosen environment beside local application state instead.
 source=Path(sys.prefix);target=DATA/'python-runtime'
 if source==Path(sys.base_prefix):raise RuntimeError('Install using the project virtual environment; see Bluetooth setup')
 if source.resolve()==target.resolve():return target/'bin/python'
 if target.resolve().is_relative_to(source.resolve()):raise RuntimeError('Application data must be outside the source virtual environment')
 DATA.mkdir(mode=0o700,parents=True,exist_ok=True)
 pending=DATA/'python-runtime.installing';previous=DATA/'python-runtime.previous'
 if pending.exists():shutil.rmtree(pending)
 shutil.copytree(source,pending,symlinks=True,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
 if previous.exists():shutil.rmtree(previous)
 if target.exists():target.rename(previous)
 pending.rename(target)
 executable=target/'bin/python'
 try:subprocess.run([str(executable),'-B','-c','import PIL,bleak'],check=True,timeout=30)
 except Exception:
  shutil.rmtree(target)
  if previous.exists():previous.rename(target)
  raise
 if previous.exists():shutil.rmtree(previous)
 return executable

def build(python_path=None):
 staged=Path(tempfile.mkdtemp(prefix='xiao-helper-build-',dir='/tmp'))/(NAME+'.app');contents=staged/'Contents';macos=contents/'MacOS';macos.mkdir(parents=True,exist_ok=True)
 info={'CFBundleName':NAME,'CFBundleDisplayName':NAME,'CFBundleIdentifier':LABEL,
       'CFBundleExecutable':NAME,'CFBundlePackageType':'APPL','CFBundleVersion':'20',
       'CFBundleShortVersionString':'2.7.0','LSUIElement':True,
       'XIAOPythonPath':str(python_path or Path(sys.executable).absolute()),
       'NSBluetoothAlwaysUsageDescription':'Connects to your XIAO Round Display to send scene controls and live information without USB.',
        'NSLocationUsageDescription':'Uses your location once to save a weather forecast location for your display.',
       'NSLocationWhenInUseUsageDescription':'Uses your location once for local weather.',
       'NSLocationTemporaryUsageDescriptionDictionary':{'WeatherLocation':'Saves your weather location once without continuous tracking.'},
       'NSLocalNetworkUsageDescription':'Connects to your XIAO display over local Wi-Fi and reads printer status when you enable that source.',
       'NSHumanReadableCopyright':"XIAO Display — USB display updater"}
 (contents/'Info.plist').write_bytes(plistlib.dumps(info))
 runtime=contents/'Resources/companion';runtime.mkdir(parents=True,exist_ok=True)
 for filename in ('display_bridge.py','codex_usage.py','sun_bridge.py','bambu_status.py','helper_runtime.py','display_control.py','scenes.json','process_owner.py','runtime_config.py','display_transport.py','hardware_profile.py','weather_status.py','wifi_setup.py','wifi_transport.py','clock-faces.json'):
  shutil.copyfile(ROOT/'companion'/filename,runtime/filename)
 shutil.copytree(ROOT/'companion/sources',runtime/'sources',ignore=shutil.ignore_patterns('__pycache__','*.pyc'),copy_function=shutil.copyfile)
 subprocess.run(['/usr/bin/swiftc','-swift-version','5','-module-cache-path','/tmp/xiao-swift-cache','-O',str(ROOT/'companion/menu_bar/MenuBar.swift'),str(ROOT/'companion/menu_bar/Profiles.swift'),str(ROOT/'companion/menu_bar/BluetoothSearch.swift'),str(ROOT/'companion/menu_bar/WeatherLocation.swift'),str(ROOT/'companion/menu_bar/WifiSetup.swift'),'-o',str(macos/NAME),'-framework','AppKit','-framework','CoreLocation','-framework','CoreWLAN'],check=True)
 subprocess.run(['/usr/bin/codesign','--force','--sign','-','--identifier',LABEL,str(staged)],check=True)
 subprocess.run([str(macos/NAME),'--version'],check=True)
 with zipfile.ZipFile(SERVICE/(NAME+'.app.zip'),'w',zipfile.ZIP_DEFLATED) as archive:
  for path in staged.rglob('*'):
   if path.is_file():archive.write(path,Path(staged.name)/path.relative_to(staged))
 return staged

def run(*args,check=True):return subprocess.run(['launchctl',*args],check=check)

def bootstrap():
 # bootout can return before launchd has finished removing the old job. A
 # short bounded retry handles its transient EIO without privilege escalation.
 for attempt in range(21):
  result=subprocess.run(['launchctl','bootstrap',DOMAIN,str(TARGET)],capture_output=True,text=True)
  if result.returncode==0:return
  if result.returncode!=5 or attempt==20:
   raise RuntimeError('Could not start '+NAME+': '+result.stderr.strip())
  time.sleep(.25)

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('action',choices=['stage','install','status','start','stop','uninstall'])
 args=parser.parse_args()
 if args.action=='stage':
  build();staged=SERVICE/f'{LABEL}.plist';staged.write_bytes(plistlib.dumps(configuration()));print(staged);return
 if args.action=='install':
  runtime_python=DATA/'python-runtime/bin/python'
  staged=build(runtime_python) # Finish and validate the replacement before stopping old job.
  prepare_runtime()
  LOGS.mkdir(mode=0o700,parents=True,exist_ok=True);TARGET.parent.mkdir(parents=True,exist_ok=True)
  for folder in ('companion','assets/codex_usage','assets/sun','assets/bambu'):(DATA/folder).mkdir(parents=True,exist_ok=True)
  for label in (OLD_LABEL,LABEL):
   old=TARGET.parent/f'{label}.plist'
   if old.exists():
    run('bootout',f'{DOMAIN}/{label}',check=False)
    if label==OLD_LABEL:old.unlink()
  stop_orphan(DATA,APP/'Contents/Resources/companion/display_bridge.py')
  APP.parent.mkdir(parents=True,exist_ok=True)
  # Replace the complete bundle: merging retains old Python bytecode/files
  # that are outside the new signature's sealed resource manifest.
  pending=APP.with_name(NAME+'.installing.app');previous=APP.with_name(NAME+'.previous')
  if pending.exists():shutil.rmtree(pending)
  shutil.copytree(staged,pending,copy_function=shutil.copyfile)
  (pending/'Contents/MacOS'/NAME).chmod(0o755)
  subprocess.run(['/usr/bin/codesign','--verify','--deep','--strict',str(pending)],check=True)
  if previous.exists():shutil.rmtree(previous)
  if APP.exists():APP.rename(previous)
  pending.rename(APP)
  if previous.exists():shutil.rmtree(previous)
  subprocess.run([LSREGISTER,'-f',str(APP)],check=True)
  TARGET.write_bytes(plistlib.dumps(configuration()));TARGET.chmod(0o600)
  run('enable',f'{DOMAIN}/{LABEL}');bootstrap();print('Installed:',NAME,'at',APP)
 elif args.action=='status':run('print',f'{DOMAIN}/{LABEL}')
 elif args.action=='start':
  status=subprocess.run(['launchctl','print',f'{DOMAIN}/{LABEL}'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  if status.returncode:
   if not TARGET.exists():raise SystemExit('Helper is not installed; run manage.py install first')
   run('enable',f'{DOMAIN}/{LABEL}');bootstrap()
  else:
   result=subprocess.run(['launchctl','kickstart',f'{DOMAIN}/{LABEL}'],capture_output=True,text=True)
   if result.returncode==113:
    # A just-booted-out job can remain visible to print briefly, then vanish.
    run('enable',f'{DOMAIN}/{LABEL}');bootstrap()
   elif result.returncode:raise RuntimeError('Could not start '+NAME+': '+result.stderr.strip())
 elif args.action=='stop':
  run('bootout',f'{DOMAIN}/{LABEL}')
  # bootout is asynchronous; a subsequent start must not kick the dying job.
  for attempt in range(25):
   status=subprocess.run(['launchctl','print',f'{DOMAIN}/{LABEL}'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
   if status.returncode:break
   if attempt==24:raise RuntimeError('Helper is still stopping; try start again in a moment')
   time.sleep(.2)
 elif args.action=='uninstall':
  run('bootout',f'{DOMAIN}/{LABEL}',check=False)
  if TARGET.exists():TARGET.unlink()
  if APP.exists():
   subprocess.run([LSREGISTER,'-u',str(APP)],check=False);shutil.rmtree(APP)
  print('Removed:',NAME)
if __name__=='__main__':main()
