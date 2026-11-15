"""Pair a user's own printer interactively. Never import application credentials."""
from pathlib import Path
import argparse,getpass,hashlib,json,os,socket,ssl,tempfile
from runtime_config import DATA_ROOT

def pair(host,root):
    serial=input('Printer serial from its settings: ').strip()
    access=getpass.getpass('Printer LAN access code (hidden): ').strip()
    if not serial or len(serial)>64 or not serial.isalnum():raise ValueError('Invalid serial')
    if len(access)!=8:raise ValueError('Expected an eight-character LAN access code')
    # Certificate inspection is unauthenticated. Require an explicit trust
    # decision before any access code is sent or a pairing is persisted.
    context=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT);context.check_hostname=False;context.verify_mode=ssl.CERT_NONE
    with socket.create_connection((host,8883),timeout=5) as raw:
        with context.wrap_socket(raw,server_hostname=host) as peer:cert=peer.getpeercert(binary_form=True)
    pem=ssl.DER_cert_to_PEM_cert(cert)
    with tempfile.NamedTemporaryFile(mode='w',suffix='.pem') as f:
        f.write(pem);f.flush();subject=ssl._ssl._test_decode_cert(f.name)['subject']
    cn=[value for rdn in subject for key,value in rdn if key=='commonName']
    if cn!=[serial]:raise ValueError('Certificate does not match the serial you entered')
    fingerprint=hashlib.sha256(cert).hexdigest()
    print('Certificate SHA-256:',fingerprint)
    print('Confirm this is your printer on your trusted network. Verify the fingerprint independently where possible.')
    if input('Trust and pin this certificate? Type yes: ').strip()!='yes':raise SystemExit('Pairing cancelled; nothing saved')
    private=Path(root)/'private';private.mkdir(parents=True,exist_ok=True,mode=0o700);private.chmod(0o700)
    values={'host':host,'serial':serial,'access_code':access,'certificate_sha256':fingerprint}
    for name,text in (('bambu.pem',pem),('bambu.json',json.dumps(values,indent=2)+'\n')):
        tmp=private/(name+'.tmp');fd=os.open(tmp,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
        with os.fdopen(fd,'w') as f:f.write(text)
        tmp.chmod(0o600);tmp.replace(private/name)
    print('Local pairing saved. Enable the bambu source explicitly and restart the helper.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',required=True);parser.add_argument('--data',type=Path,default=DATA_ROOT)
    args=parser.parse_args();pair(args.host,args.data)
