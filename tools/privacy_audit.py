"""Conservative audit of publishable files; report paths, never matching values."""
from pathlib import Path
import re,subprocess,sys,hashlib
ROOT=Path(__file__).resolve().parents[1]
EXCLUDED={'.git','.venv','__pycache__','build','dist'}
# Reviewed media: synthetic showcase and the owner's explicitly authorized
# hardware-demo GIF (no audio/location metadata). Replacements require renewed
# visual/privacy review before updating their exact fingerprints.
REVIEWED_MEDIA={
 'docs/media/xiao-display-showcase.gif':'dd8205b091c175a74f15963fa1499611f00e75f29931e44443b7374c58e8451f',
 'docs/media/gym-xiao-demo.gif':'74e331c8031b7455441043044c079e735e8c7b9bff91f7ba7d78df99774e19a4',
}
SYNTHETIC_MACS={'00:11:22:33:44:55','aa:bb:cc:dd:ee:ff'}

def audit(paths, read_bytes=None):
    errors=[]
    forbidden_names={'settings.json','display-profiles.json','display-preferences.json','control-status.json','helper-status.json','display-connection.json','bluetooth-status.json','hardware-status.json','weather-location.json','wifi-setup-status.json','display-pairings.json','device-catalog.json','device-connections.json','device-names.json','gym-actions.json','gym-status.json','device-profile.json','firmware-maintenance.json','pedals.csv'}
    patterns=[r'/Users/[A-Za-z0-9_.-]+/',r'/home/[A-Za-z0-9_.-]+/',
              r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
              r'gh[pousr]_[A-Za-z0-9]{30,}',r'github_pat_[A-Za-z0-9_]{30,}',
              r'\bsk-(?:proj-)?[A-Za-z0-9_-]{30,}',
              r'/dev/(?:cu|tty)\.usbmodem(?!YOUR_DEVICE\b|EXAMPLE\b)[A-Za-z0-9]{6,}',
              r'\b(?:192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3})\b',
              r'(?i)com\.roy\.', r"(?i)roy['’]s (?:laboratory|bike)"]
    for path in paths:
        rel=path.relative_to(ROOT)
        if any(p in EXCLUDED for p in rel.parts):
            if read_bytes is not None:errors.append((str(rel),'generated directory in publication index'))
            continue
        if path.name in forbidden_names or path.name.endswith('-live-check.json') or (path.suffix==".tmp" and path.with_suffix(".json").name in forbidden_names) or any(p in ('private','devices','logs','captures') for p in rel.parts) or any(part.startswith('python-runtime') for part in rel.parts) or path.suffix.lower() in ('.pem','.key','.p12','.elf','.hex','.bin','.uf2','.pyc','.zip','.partial','.chunk','.log') or any(part.endswith('.app') for part in rel.parts):
            errors.append((str(rel),'local/generated/credential file'));continue
        data=read_bytes(path) if read_bytes is not None else path.read_bytes()
        if str(rel) in REVIEWED_MEDIA:
            if len(data)>4*1024*1024 or data[:6] not in (b'GIF87a',b'GIF89a') or hashlib.sha256(data).hexdigest()!=REVIEWED_MEDIA[str(rel)]:
                errors.append((str(rel),'media changed; repeat visual/privacy review'))
            continue
        try:text=data.decode('utf-8')
        except UnicodeDecodeError:errors.append((str(rel),'unreviewed binary'));continue
        if any(re.search(pattern,text) for pattern in patterns):errors.append((str(rel),'possible personal path/address/secret'))
        addresses=re.findall(r'\b(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}\b',text)
        if any(address.lower() not in SYNTHETIC_MACS for address in addresses):
            errors.append((str(rel),'non-synthetic hardware address'))
    return errors

def main():
    # Git index is authoritative once initialized, so ignored local app builds
    # and venvs are not mistaken for intended publication content.
    indexed=subprocess.run(['git','-C',str(ROOT),'ls-files','-z'],capture_output=True)
    paths=[ROOT/p.decode() for p in indexed.stdout.split(b'\0') if p] if indexed.returncode==0 and indexed.stdout else [p for p in ROOT.rglob('*') if p.is_file() and not any(x in EXCLUDED for x in p.relative_to(ROOT).parts) and p.suffix not in ('.plist','.zip')]
    # Inspect staged blobs too: an unstaged cleanup cannot hide a secret that
    # would still be published by committing the index.
    errors=audit(paths)
    if indexed.returncode==0 and indexed.stdout:
        def staged(path):
            return subprocess.check_output(['git','-C',str(ROOT),'show',':'+str(path.relative_to(ROOT))])
        errors+=audit(paths,read_bytes=staged)
    for path,reason in errors:print(path+': '+reason)
    if errors:raise SystemExit(1)
    print(f'PASS privacy audit: {len(paths)} indexed/source files; no matching private artifacts')

if __name__=='__main__':main()
