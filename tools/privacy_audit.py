"""Conservative audit of publishable files; report paths, never matching values."""
from pathlib import Path
import re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
EXCLUDED={'.git','.venv','__pycache__','build','dist'}

def audit(paths):
    errors=[]
    forbidden_names={'settings.json','display-profiles.json','display-preferences.json','control-status.json','helper-status.json','display-connection.json','bluetooth-status.json'}
    patterns=[r'/Users/[A-Za-z0-9_.-]+/',r'/home/[A-Za-z0-9_.-]+/',
              r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
              r'gh[pousr]_[A-Za-z0-9]{30,}',r'github_pat_[A-Za-z0-9_]{30,}',
              r'\bsk-(?:proj-)?[A-Za-z0-9_-]{30,}',
              r'/dev/(?:cu|tty)\.usbmodem(?!YOUR_DEVICE\b|EXAMPLE\b)[A-Za-z0-9]{6,}',
              r'\b(?:192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3})\b']
    for path in paths:
        rel=path.relative_to(ROOT)
        if any(p in EXCLUDED for p in rel.parts):continue
        if path.name in forbidden_names or 'private' in rel.parts or any(part.startswith('python-runtime') for part in rel.parts) or path.suffix.lower() in ('.pem','.key','.p12','.elf','.hex','.bin','.uf2','.pyc','.zip'):
            errors.append((str(rel),'local/generated/credential file'));continue
        try:text=path.read_text()
        except UnicodeDecodeError:errors.append((str(rel),'unreviewed binary'));continue
        if any(re.search(pattern,text) for pattern in patterns):errors.append((str(rel),'possible personal path/address/secret'))
    return errors

def main():
    # Git index is authoritative once initialized, so ignored local app builds
    # and venvs are not mistaken for intended publication content.
    indexed=subprocess.run(['git','-C',str(ROOT),'ls-files','-z'],capture_output=True)
    paths=[ROOT/p.decode() for p in indexed.stdout.split(b'\0') if p] if indexed.returncode==0 and indexed.stdout else [p for p in ROOT.rglob('*') if p.is_file() and not any(x in EXCLUDED for x in p.relative_to(ROOT).parts) and p.suffix not in ('.plist','.zip')]
    errors=audit(paths)
    for path,reason in errors:print(path+': '+reason)
    if errors:raise SystemExit(1)
    print(f'PASS privacy audit: {len(paths)} indexed/source files; no matching private artifacts')

if __name__=='__main__':main()
