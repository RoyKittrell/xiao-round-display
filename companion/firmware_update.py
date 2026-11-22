"""Explicit paired-TLS firmware update; ordinary device polling never calls this."""
from pathlib import Path
from contextlib import contextmanager
import argparse,hashlib,json,os,re,secrets,time
from device_profiles import ROOT,PACKAGE,module
from device_registry import atomic
from multi_device import device_root,settings_for
from wifi_transport import WifiSession,host_from_device

def fields(line):
    return dict(re.findall(r'([a-z_]+)=([^\s]+)',line.decode('ascii')))

def image_metadata(folder,profile):
    manifest=json.loads((folder/'manifest.json').read_text());data=(folder/'application.bin').read_bytes()
    if data[0]!=0xe9 or int.from_bytes(data[12:14],'little')!=9 or data[32:36]!=bytes.fromhex('3254cdab'):raise ValueError('Not an ESP32-S3 application image')
    marker=('XIAO-PROFILE:'+profile+':v1').encode()
    elf=data[176:208].hex()
    if marker not in data or manifest.get('ota')!=1 or manifest.get('elf_sha256')!=elf:raise ValueError('This firmware package does not support verified Wi-Fi updates. Rebuild the module first.')
    return data,elf

@contextmanager
def maintenance(root,identifier):
    target=device_root(root,identifier);path=target/'firmware-maintenance.json'
    value={'pid':os.getpid(),'until':time.time()+300,'token':secrets.token_hex(16)}
    atomic(path,value)
    try:
        # The device worker observes the lease and releases its cached TLS link.
        time.sleep(.5);yield target
    finally:
        try:
            if json.loads(path.read_text()).get('token')==value['token']:path.unlink()
        except (OSError,ValueError):pass

def paired_host(root,identifier):
    if not re.fullmatch('usb:[0-9a-f]{12}',identifier):raise ValueError('Choose a verified XIAO first')
    saved=settings_for(root,identifier);host=host_from_device(saved.get('wifi_host',''))
    record=json.loads((Path(root)/'private/wifi'/f'{host}.json').read_text())
    if record.get('serial')!=identifier[4:]:raise ValueError('Wi-Fi pairing belongs to another display')
    return host

def hello(session,identifier,profile,partition_hash):
    line=session.readline(10)
    if not line.startswith(b'OTA '):raise ValueError('This display needs the initial USB firmware update to enable Wi-Fi updates')
    value=fields(line)
    if value.get('version')!='1' or value.get('serial')!=identifier[4:] or value.get('family')!='esp32s3' or value.get('profile')!=profile:raise ValueError('Wi-Fi display identity/profile does not match this update')
    if value.get('slot') not in ('app0','app1','ota_0','ota_1'):raise ValueError('Unknown running OTA slot')
    if value.get('partitions')!=partition_hash:raise ValueError('Partition layout differs; use a reviewed USB update')
    return value

def update(identifier,profile,root=ROOT,package=PACKAGE,force=False,progress=lambda n:None,session_factory=WifiSession):
    folder=module(profile,'esp32s3',package);data,elf=image_metadata(folder,profile)
    partition_hash=hashlib.sha256((folder/'partitions.bin').read_bytes()).hexdigest()
    host=paired_host(root,identifier)
    def session():return session_factory(host,port=7444,root=root)
    with maintenance(root,identifier) as target:
        s=session()
        try:
            s.connect();current=hello(s,identifier,profile,partition_hash)
            if current.get('elf')==elf and not force:return {'verified':True,'changed':False,'profile':profile,'transport':'wifi'}
            if len(data)>int(current['capacity']):raise ValueError('Firmware does not fit the inactive slot')
            request=f'UPDATE 1 {len(data)} {hashlib.sha256(data).hexdigest()} {partition_hash} {profile} {elf}\n'.encode()
            s.write(request,10)
            if s.readline(30)!=b'READY\n':raise OSError('Display refused the firmware update')
            for offset in range(0,len(data),8192):
                chunk=data[offset:offset+8192];s.write(chunk,20);progress(min(100,(offset+len(chunk))*100//len(data)))
            if s.readline(40)!=b'COMMITTED\n':raise OSError('The display did not confirm the commit. Keep power connected and check it before retrying')
        finally:s.close()
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            time.sleep(1);s=session()
            try:
                s.connect();value=hello(s,identifier,profile,partition_hash)
                if value.get('elf')==elf and value.get('slot')!=current.get('slot'):
                    result={'verified':True,'changed':True,'profile':profile,'transport':'wifi','elf':elf}
                    atomic(target/'device-profile.json',result);return result
            except (OSError,ValueError):pass
            finally:s.close()
        raise OSError('Firmware was committed but the new build has not reconnected yet. Keep power connected and check the display before retrying.')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--device',required=True);p.add_argument('--profile',choices=['desktop','gym'],required=True);p.add_argument('--force',action='store_true');a=p.parse_args()
    try:result=update(a.device,a.profile,force=a.force)
    except Exception as error:result={'error':str(error)}
    print(json.dumps(result))
if __name__=='__main__':main()
