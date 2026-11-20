"""Actual TLS client against a local pinned TLS server; no real credentials."""
from pathlib import Path
import binascii,json,os,socket,ssl,sys,tempfile,threading,time,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
import wifi_setup,wifi_transport,display_transport

class WifiTests(unittest.TestCase):
 def test_setup_limits_and_checksum(self):
  cert,key=wifi_setup.certificates('xiao-display-123456.local')
  packet=wifi_setup.packet('Test network','test-password','a'*64,cert,key)
  self.assertLess(len(packet),2400);self.assertNotIn(b'test-password',packet)
  body,crc=packet[1:].strip().rsplit(b'|',1);self.assertEqual(int(crc,16),binascii.crc_hqx(body,0xffff))
  for name,password in [('', 'abcdefgh'),('a'*33,'abcdefgh'),('hello','short'),('hello','x'*64),('a\0b','abcdefgh')]:
   with self.assertRaises(ValueError):wifi_setup.credentials(name,password)
  wifi_setup.credentials('Unicode ☀','a'*63);wifi_setup.credentials('network','a'*64)
 def test_actual_firmware_provision_parser(self):
  import subprocess
  source=(ROOT/'XiaoPlayground/WifiLink.h').read_text()
  def method(text,start):
   a=text.index(start);b=text.index('{',a);depth=1;i=b+1
   while depth:
    depth+=(text[i]=='{')-(text[i]=='}');i+=1
   return text[a:i]
  config=source[source.index(' struct Config'):source.index(' portMUX_TYPE')]
  parser=method(source,' bool provision(')
  unhex=method(source,' static int nibble(')+'\n'+method(source,' static int unhex(')
  code=('#include <cstdint>\n#include <cstring>\n#include <cstdlib>\n#include <string>\n#include <iostream>\n'
        'struct Preferences {bool begin(const char*,bool){return true;}size_t putBytes(const char*,void*,size_t n){return n;}void end(){}};\n'
        'class Parser { public:\n'+config+unhex+parser+'\n};\n'
        'int main(){std::string s;while(std::getline(std::cin,s)){Parser p;std::cout<<p.provision(s.data())<<"\\n";}}')
  cert,key=wifi_setup.certificates('xiao-display-123456.local')
  packet=wifi_setup.packet('Test WiFi','test-password','a'*64,cert,key)[1:].strip()
  bad=packet[:-1]+(b'0' if packet[-1:]!=b'0' else b'1')
  with tempfile.TemporaryDirectory() as tmp:
   path=Path(tmp);(path/'check.cpp').write_text(code)
   subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(path/'check.cpp'),'-o',str(path/'check')],check=True,capture_output=True)
   r=subprocess.run([str(path/'check')],input=packet+b'\n'+bad+b'\nN|broken\n',capture_output=True,check=True)
   self.assertEqual(r.stdout,b'1\n0\n0\n')
 def test_setup_survives_finite_usb_receive_ring(self):
  # A serial receiver drains during the pacing intervals. Overflow discards
  # incoming bytes, just as on the physical native-USB target.
  class Port:
   def __init__(self):self.pending=bytearray();self.received=bytearray();self.dropped=0
   def write(self,data):
    room=256-len(self.pending);self.pending.extend(data[:room]);self.dropped+=max(0,len(data)-room)
   def flush(self):pass
   def readline(self):self.tick();return b''
   def tick(self,*args):self.received.extend(self.pending);self.pending.clear()
  data=b'@N|'+b'a'*1500+b'\n'
  burst=Port();burst.write(data);burst.tick();self.assertGreater(burst.dropped,0)
  paced=Port()
  with patch.object(wifi_setup.time,'sleep',side_effect=paced.tick):wifi_setup.send_setup(paced,data)
  paced.tick();self.assertEqual(paced.dropped,0);self.assertEqual(paced.received,b'\n'+data)
 def test_selection_keeps_usb_and_requires_saved_host(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);(root/'display-connection.json').write_text(json.dumps({'mode':'wifi','wifi_host':'xiao-display-123456.local','usb_serial':'aabbccddeeff'}))
   self.assertEqual(display_transport.endpoint(root,'/dev/cu.other'),'wifi://xiao-display-123456.local')
  for name in ('../../private','192.0.2.1','xiao-display-123456.local/../key','xiao-display-123456.local:9999'):
   with self.assertRaises(ValueError):wifi_transport.host_from_device(name)
 def exercise_tls(self,pin_correct=True,token_correct=True,overflow=False):
  host='xiao-display-123456.local';cert,key=wifi_setup.certificates(host)
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);private=root/'private/wifi';private.mkdir(parents=True)
   pem=ssl.DER_cert_to_PEM_cert(cert);(root/'cert.pem').write_text(pem)
   # openssl ec DER format is supported by the server context.
   (root/'key.der').write_bytes(key)
   import subprocess
   subprocess.run(['/usr/bin/openssl','ec','-inform','DER','-in',str(root/'key.der'),'-out',str(root/'key.pem')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
   ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(root/'cert.pem',root/'key.pem')
   listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(4);port=listener.getsockname()[1]
   server_errors=[]
   def serve():
    try:
     conn,_=listener.accept()
     with ctx.wrap_socket(conn,server_side=True) as secure:
      auth=b''
      while not auth.endswith(b'\n'):auth+=secure.recv(128)
      if auth!=b'AUTH '+b'a'*64+b'\n':secure.sendall(b'NO\n');return
      secure.sendall(b'OK\n');request=secure.recv(1024)
      self.assertEqual(request,b'TEST\n')
      if overflow:secure.sendall(b'x'*33000);time.sleep(.1);return
      secure.sendall(b'ACK ');time.sleep(.04);secure.sendall(b'W\nACK C\n');time.sleep(.1)
    except (ssl.SSLError,ConnectionError) as error:server_errors.append(type(error).__name__)
    finally:listener.close()
   thread=threading.Thread(target=serve,daemon=True);thread.start()
   if not pin_correct:pem=ssl.DER_cert_to_PEM_cert(wifi_setup.certificates(host)[0])
   (private/(host+'.json')).write_text(json.dumps({'certificate':pem,'token':('a' if token_correct else 'b')*64}))
   connect=socket.create_connection
   with patch.dict(os.environ,{'XIAO_DISPLAY_DATA_DIR':str(root)}),patch.object(wifi_transport.socket,'create_connection',side_effect=lambda address,timeout:connect(('127.0.0.1',port),timeout)):
    if not pin_correct or not token_correct:
     with self.assertRaises(OSError):wifi_transport.WifiPort('wifi://'+host)
    else:
     with wifi_transport.WifiPort('wifi://'+host,timeout=.01) as client:
      client.write(b'TEST\n')
      if overflow:
       with self.assertRaises(OSError):client.session.readline(1)
      else:
       self.assertEqual(client.readline(),b'');time.sleep(.07)
       self.assertEqual(client.readline(),b'ACK W\n');self.assertEqual(client.readline(),b'ACK C\n')
       self.assertTrue(wifi_transport.is_connected('wifi://'+host))
    wifi_transport.close_wifi()
   thread.join(timeout=5);self.assertFalse(thread.is_alive())
 def test_tls_pinning_auth_and_fragmented_replies(self):self.exercise_tls()
 def test_untrusted_certificate_is_rejected(self):self.exercise_tls(pin_correct=False)
 def test_wrong_controller_key_is_rejected(self):self.exercise_tls(token_correct=False)
 def test_receive_limit(self):self.exercise_tls(overflow=True)
if __name__=='__main__':unittest.main()
