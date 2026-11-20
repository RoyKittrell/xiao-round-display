"""Private USB provisioning. Password arrives on stdin, never argv/logs/files.

Writes a per-device certificate pin and random controller key under private/.
The Wi-Fi password and TLS private key are kept only on the display.
"""
from pathlib import Path
import binascii,fcntl,json,os,re,secrets,ssl,subprocess,sys,tempfile,time
from serial.tools import list_ports
from display_transport import open_port
from hardware_profile import identify
ROOT=Path(os.environ.get('XIAO_DISPLAY_DATA_DIR',Path.home()/'Library/Application Support/XIAO Display Helper'))

def identity(serial):
    value=''.join(c.lower() for c in str(serial) if c.isalnum())
    if not re.fullmatch('[0-9a-f]{12}',value):raise ValueError('Choose an ESP32 S3 or C6 USB display first')
    return value

def credentials(ssid,password):
    if not isinstance(ssid,str) or not 1<=len(ssid.encode())<=32 or '\0' in ssid:raise ValueError('Wi-Fi name must contain 1–32 UTF-8 bytes')
    if not isinstance(password,str) or '\0' in password or not 8<=len(password.encode())<=64:raise ValueError('Enter a WPA2/WPA3 Personal password (8–63 bytes, or 64 hex digits)')
    if len(password.encode())==64 and not re.fullmatch('[0-9a-fA-F]{64}',password):raise ValueError('A 64-byte Wi-Fi key must be hexadecimal')

def certificates(host):
    with tempfile.TemporaryDirectory(prefix='xiao-wifi-') as tmp:
        root=Path(tmp);key=root/'key.pem';cert=root/'cert.pem'
        subprocess.run(['/usr/bin/openssl','ecparam','-name','prime256v1','-genkey','-noout','-out',str(key)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        subprocess.run(['/usr/bin/openssl','req','-new','-x509','-sha256','-key',str(key),'-out',str(cert),'-days','3650','-subj','/CN='+host,'-addext','subjectAltName=DNS:'+host],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        der=ssl.PEM_cert_to_DER_cert(cert.read_text())
        private=subprocess.check_output(['/usr/bin/openssl','ec','-in',str(key),'-outform','DER'],stderr=subprocess.DEVNULL)
        return der,private

def packet(ssid,password,token,cert,key):
    credentials(ssid,password)
    if len(cert)>640 or len(key)>256 or len(token)!=64:raise ValueError('Invalid device TLS material')
    body='|'.join(['N','1',ssid.encode().hex(),password.encode().hex(),token,cert.hex(),key.hex()]).encode()
    value=b'@'+body+b'|'+f'{binascii.crc_hqx(body,0xffff):04x}'.encode()+b'\n'
    if len(value)>2400:raise ValueError('Setup packet exceeds device limit')
    return value

def send_setup(port,data):
    # Native USB CDC has a small RX ring. A certificate-sized single write can
    # overflow it while the LCD is sending pixels. CRC rejects truncated data,
    # but a missing newline leaves the host waiting forever. Give rendering
    # time to drain the ring, and finish any prior incomplete line first.
    port.write(b'\n');port.flush()
    end=time.monotonic()+2
    while time.monotonic()<end:
        if not port.readline():break
    for offset in range(0,len(data),32):
        port.write(data[offset:offset+32]);port.flush();time.sleep(.015)


def provision(value):
    serial=identity(value.get('serial',''));ssid=value.get('ssid');password=value.get('password');credentials(ssid,password)
    boards=[p for p in list_ports.comports() if p.vid==0x303a and identity(p.serial_number or '')==serial]
    if len(boards)!=1:raise ValueError('The selected ESP32 must be connected over USB for Wi-Fi setup')
    # hostname mirrors the ESP efuse MAC's final three octets.
    host='xiao-display-'+serial[-6:]+'.local'
    cert,key=certificates(host);token=secrets.token_hex(32);data=packet(ssid,password,token,cert,key)
    target=ROOT/'private/wifi'/f'{host}.json';target.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
    record={'host':host,'port':7443,'serial':serial,'token':token,'certificate':ssl.DER_cert_to_PEM_cert(cert)}
    # Retain the pin before provisioning, so even a post-write USB disconnect
    # cannot orphan the new key. Previous record remains recoverable privately.
    if target.exists():target.with_suffix('.previous.json').write_text(target.read_text());target.with_suffix('.previous.json').chmod(0o600)
    tmp=target.with_suffix('.tmp');tmp.write_text(json.dumps(record,indent=2)+'\n');tmp.chmod(0o600);tmp.replace(target)
    lockpath=ROOT/'companion/usb.lock';lockpath.parent.mkdir(parents=True,exist_ok=True)
    with lockpath.open('a') as lock:
        deadline=time.monotonic()+35
        while True:
            try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);break
            except BlockingIOError:
                if time.monotonic()>deadline:raise TimeoutError('Display is busy with an image transfer. Try Wi-Fi setup again shortly.')
                time.sleep(.1)
        with open_port(boards[0].device,115200,timeout=.2,write_timeout=5) as port:
            port.reset_input_buffer();port.write(b'?');port.flush();end=time.monotonic()+4;diagnostics=[]
            while time.monotonic()<end:
                line=port.readline()
                decoded=line.decode('ascii',errors='replace').strip()
                if decoded.startswith(('FIRMWARE ','WIFI ')):diagnostics.append(decoded)
                if decoded.startswith('SUN rows='):break
            profile=identify(diagnostics,boards[0].device)
            if profile['family'] not in ('esp32s3','esp32c6') or not profile['wifi_firmware']:
                raise ValueError('The connected display needs supported ESP32 Wi-Fi firmware before setup')
            send_setup(port,data);end=time.monotonic()+8;saved=False
            while time.monotonic()<end:
                line=port.readline()
                if line.startswith(b'ACK N saved'):saved=True;break
                if line.startswith(b'ERR '):raise ValueError('Display rejected Wi-Fi setup')
            if not saved:raise TimeoutError('Display did not confirm Wi-Fi setup')
        # Startup can take several seconds. Poll only the same serial identity.
        end=time.monotonic()+45
        while time.monotonic()<end:
            try:
                boards=[p for p in list_ports.comports() if p.vid==0x303a and ''.join(c.lower() for c in (p.serial_number or '') if c.isalnum())==serial]
                if boards:
                    with open_port(boards[0].device,115200,timeout=.2,write_timeout=2) as port:
                        port.write(b'?');port.flush();until=time.monotonic()+1
                        while time.monotonic()<until:
                            line=port.readline().decode(errors='replace').strip()
                            if line.startswith('WIFI ') and 'connected=1' in line:
                                fields=dict(item.split('=',1) for item in line.split()[1:]);record['ip']=fields.get('ip','');tmp.write_text(json.dumps(record,indent=2)+'\n');tmp.chmod(0o600);tmp.replace(target)
                                return {'host':host,'ip':record['ip'],'connected':True}
            except OSError:pass
            time.sleep(.5)
    return {'host':host,'connected':False,'message':'Settings saved, but the network did not connect yet. Check the password, 2.4 GHz network and attached antenna.'}

if __name__=='__main__':
    failed=False
    try:
        value=json.loads(sys.stdin.buffer.read(4096));result=provision(value)
    except Exception as error:
        # Never include input, serial packet or subprocess stdout in errors.
        result={'error':str(error) if isinstance(error,(ValueError,TimeoutError)) else 'Wi-Fi setup failed: '+type(error).__name__};failed=True
    try:
        path=ROOT/'wifi-setup-status.json';tmp=path.with_suffix('.tmp')
        tmp.write_text(json.dumps({**result,'updated_at':int(time.time())}));tmp.chmod(0o600);tmp.replace(path)
    except OSError:pass
    print(json.dumps(result));sys.exit(int(failed))
