"""Device-purpose catalog and explicit USB-only installation of built modules.

Name edits never reach firmware. Only an explicit native Apply action calls
install(). Menu selection and ordinary polling never install firmware.
"""
from pathlib import Path
import argparse,fcntl,hashlib,json,os,re,struct,subprocess,time
from device_registry import atomic
from multi_device import device_root

HERE=Path(__file__).resolve().parent
ROOT=Path(os.environ.get('XIAO_DISPLAY_DATA_DIR',Path.home()/'Library/Application Support/XIAO Display Helper'))
PACKAGE=HERE/'device_profiles'

def catalog(package=PACKAGE):
    value=json.loads((Path(package)/'catalog.json').read_text())
    assert value['schema_version']==1
    return value

def module(profile,family,package=PACKAGE):
    package=Path(package);items=catalog(package)['profiles']
    item=next((v for v in items if v['id']==profile),None)
    if item is None or family not in item['installer_families']:raise ValueError('This profile installer does not support this controller yet.')
    folder=package/'build'/family/profile
    manifest=json.loads((folder/'manifest.json').read_text())
    if manifest.get('schema_version')!=1 or manifest.get('profile')!=profile or manifest.get('family')!=family or manifest.get('chip')!='esp32s3' or manifest.get('offset')!=0x10000:raise ValueError('Invalid profile firmware manifest')
    if set(manifest.get('files',{}))!={'application.bin','partitions.bin'}:raise ValueError('Incomplete profile package')
    for name,digest in manifest['files'].items():
        if hashlib.sha256((folder/name).read_bytes()).hexdigest()!=digest:raise ValueError('Profile firmware checksum mismatch')
    if not 1024<=(folder/'application.bin').stat().st_size<=0x330000:raise ValueError('Invalid firmware size')
    return folder

def port_for(serial):
    from serial.tools import list_ports
    matches=[p.device for p in list_ports.comports() if (p.serial_number or '').lower().replace(':','').replace('-','')==serial and p.vid==0x303a]
    if len(matches)!=1:raise ValueError('Connect this XIAO with a USB data cable before applying a profile.')
    return matches[0]

def probe(port):
    import serial
    from hardware_profile import identify
    with serial.Serial(port,115200,timeout=.15,write_timeout=3) as s:
        time.sleep(.25);s.reset_input_buffer();s.write(b'?\n');lines=[];end=time.monotonic()+4
        while time.monotonic()<end:
            line=s.readline().decode('ascii',errors='replace').strip()
            if line:lines.append(line[:500])
            if line.startswith(('SUN rows=','UI ')) and identify(lines,port)['identified']:break
        return identify(lines,port)

def check_partitions(expected,actual):
    # Preserve NVS, filesystem, OTA layout and bootloader: never repartition.
    if len(expected)<32 or len(actual)<len(expected) or actual[:len(expected)]!=expected:raise ValueError('The current partition layout differs. No firmware was written; a reviewed USB migration is needed.')

def run_tool(tool,serial,args,reset='no-reset'):
    deadline=time.monotonic()+8
    while True:
        try:port=port_for(serial);break # Recheck USB identity before every ROM command.
        except ValueError:
            if time.monotonic()>deadline:raise
            time.sleep(.1)
    command=[str(tool),'--chip','esp32s3','--port',port,'--baud','921600','--after',reset,*args]
    result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
    if result.returncode and args[0]=='read-flash':
        # Some native-USB/stub reads fail at particular image byte patterns.
        # The ROM slow reader is independent of that streaming stub path.
        command=[str(tool),'--chip','esp32s3','--port',port_for(serial),'--baud','115200','--no-stub','--after',reset,*args]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
    if result.returncode:raise RuntimeError('USB firmware operation failed: '+result.stdout[-1200:])
    return result.stdout

def read_application(tool,serial,offset,destination):
    # Native USB can drop long streaming reads. Bound each ROM transfer and
    # assemble an exact-size recovery file before any firmware write.
    destination=Path(destination);partial=destination.with_suffix('.partial')
    with partial.open('wb') as out:
        for relative in range(0,0x330000,0x20000):
            length=min(0x20000,0x330000-relative);chunk=destination.with_suffix('.chunk')
            run_tool(tool,serial,['read-flash',hex(int(offset,0)+relative),hex(length),str(chunk)],reset='hard-reset')
            time.sleep(.5)
            data=chunk.read_bytes()
            if len(data)!=length:raise OSError('Incomplete USB recovery chunk; firmware was not written')
            out.write(data);chunk.unlink()
    partial.replace(destination)

def install(identifier,profile,root=ROOT,package=PACKAGE,force=False):
    if not re.fullmatch(r'usb:[0-9a-f]{12}',identifier):raise ValueError('Choose a positively identified USB XIAO first.')
    serial=identifier[4:];root=Path(root);target=device_root(root,identifier);folder=module(profile,'esp32s3',package)
    tool=Path(package)/'tools/esptool'
    if not tool.is_file() or not os.access(tool,os.X_OK):raise ValueError('USB installer is not bundled. Build and reinstall the helper with the ESP32 toolchain available.')
    lockpath=target/'companion/usb.lock';lockpath.parent.mkdir(parents=True,exist_ok=True)
    with lockpath.open('a') as lock:
        deadline=time.monotonic()+35
        while True:
            try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);break
            except BlockingIOError:
                if time.monotonic()>deadline:raise TimeoutError('This display is busy. Try again after its current transfer finishes.')
                time.sleep(.1)
        current=probe(port_for(serial))
        if not current['identified'] or current['family']!='esp32s3':raise ValueError('The running firmware must positively identify a supported ESP32-S3.')
        if current['role']==profile and not force:return {'profile':profile,'changed':False,'verified':True}
        offset=hex(current.get('ota_address',65536))
        if offset not in ('0x10000','0x340000'):raise ValueError('Unsupported running application slot')
        backup=target/'private/profile-recovery'/str(time.time_ns());backup.mkdir(parents=True,mode=0o700)
        atomic(backup/'before.json',{'device':identifier,'hardware':current,'requested_profile':profile})
        try:
            output=run_tool(tool,serial,['read-mac'])
            macs=re.findall(r'(?:MAC|Base MAC):\s*([0-9a-fA-F:]{17})',output)
            if serial not in [m.lower().replace(':','') for m in macs]:raise ValueError('ROM MAC does not match the selected XIAO. No firmware was written.')
            run_tool(tool,serial,['read-flash','0x8000','0x1000',str(backup/'partitions.bin')])
            check_partitions((folder/'partitions.bin').read_bytes(),(backup/'partitions.bin').read_bytes())
            read_application(tool,serial,offset,backup/'application.bin')
            # Application-only write. NVS, logs, preferences and partition table remain.
            output=run_tool(tool,serial,['write-flash',offset,str(folder/'application.bin')],reset='hard-reset')
            (backup/'upload.log').write_text(output)
            end=time.monotonic()+18;observed={}
            while time.monotonic()<end:
                try:
                    observed=probe(port_for(serial))
                    if observed.get('role')==profile:break
                except (OSError,ValueError):pass
                time.sleep(.5)
            if observed.get('role')!=profile:raise RuntimeError('Firmware was written but the new profile did not confirm its identity. Recovery firmware was saved locally.')
        except Exception:
            # Restore normal boot after a refused layout/MAC or failed ROM job.
            try:run_tool(tool,serial,['read-mac'],reset='hard-reset')
            except Exception:pass
            raise
        result={'profile':profile,'changed':True,'verified':True,'hardware':observed}
        atomic(target/'device-profile.json',result);return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--device',required=True);parser.add_argument('--profile',required=True,choices=['desktop','gym'])
    parser.add_argument('--force',action='store_true');args=parser.parse_args()
    try:result=install(args.device,args.profile,force=args.force)
    except Exception as error:result={'error':str(error)}
    print(json.dumps(result))
if __name__=='__main__':main()
