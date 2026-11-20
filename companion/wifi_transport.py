"""Certificate-pinned TLS LAN transport for USB-provisioned ESP32 displays."""
import json,os,re,socket,ssl,time
from pathlib import Path

def host_from_device(device):
    host=str(device).removeprefix('wifi://')
    if not re.fullmatch(r'xiao-display-[0-9a-f]{6}\.local',host):raise ValueError('Invalid saved Wi-Fi display')
    return host

class WifiSession:
    def __init__(self,host):self.host=host;self.socket=None;self.buffer=bytearray()
    def connect(self):
        if self.socket:return
        root=Path(os.environ.get('XIAO_DISPLAY_DATA_DIR',Path.home()/'Library/Application Support/XIAO Display Helper'))
        record=json.loads((root/'private/wifi'/f'{self.host}.json').read_text())
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT);context.minimum_version=ssl.TLSVersion.TLSv1_2
        context.load_verify_locations(cadata=record['certificate']);context.set_ciphers('ECDHE-ECDSA-AES128-GCM-SHA256')
        addresses=[self.host]
        if record.get('ip'):addresses.append(record['ip'])
        for address in addresses:
            raw=None
            try:
                raw=socket.create_connection((address,7443),timeout=5)
                raw.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
                self.socket=context.wrap_socket(raw,server_hostname=self.host)
                self.socket.sendall(('AUTH '+record['token']+'\n').encode())
                if self.readline(5)!=b'OK\n':raise OSError('Wi-Fi display authentication failed')
                return
            except OSError:
                if raw:raw.close()
                self.close()
        raise OSError('Selected Wi-Fi display is unavailable or could not be authenticated')
    def close(self):
        if self.socket:
            try:self.socket.close()
            except OSError:pass
        self.socket=None;self.buffer.clear()
    def write(self,data,timeout):
        try:self.socket.settimeout(timeout);self.socket.sendall(data)
        except OSError:self.close();raise
        return len(data)
    def readline(self,timeout):
        end=time.monotonic()+timeout
        while True:
            pos=self.buffer.find(b'\n')
            if pos>=0:value=bytes(self.buffer[:pos+1]);del self.buffer[:pos+1];return value
            remaining=end-time.monotonic()
            if remaining<=0:return b''
            try:
                self.socket.settimeout(remaining);value=self.socket.recv(4096)
                if not value:raise OSError('Wi-Fi display disconnected')
                if len(self.buffer)+len(value)>32768:raise OSError('Wi-Fi reply buffer exceeded')
                self.buffer.extend(value)
            except (socket.timeout,TimeoutError):return b''
            except OSError:self.close();raise

sessions={}
def close_wifi():
    for session in sessions.values():session.close()
    sessions.clear()

class WifiPort:
    def __init__(self,device,baudrate=115200,timeout=.2,write_timeout=5):
        host=host_from_device(device);self.timeout=timeout;self.write_timeout=write_timeout
        self.session=sessions.setdefault(host,WifiSession(host));self.session.connect()
    def __enter__(self):return self
    def __exit__(self,kind,*args):
        if kind:self.session.close()
        return False
    def write(self,data):return self.session.write(bytes(data),self.write_timeout)
    def flush(self):pass
    def readline(self):return self.session.readline(self.timeout)
    def reset_input_buffer(self):self.session.buffer.clear()

def is_connected(device):
    session=sessions.get(host_from_device(device));return bool(session and session.socket)
