"""User-owned settings. A fresh checkout has no device or enabled data feeds."""
from pathlib import Path
from datetime import timezone,timedelta
import json,os

APP_NAME='XIAO Display Helper'
DATA_ROOT=Path(os.environ.get('XIAO_DISPLAY_DATA_DIR',Path.home()/'Library/Application Support'/APP_NAME))
DEFAULTS={'schema_version':1,'device':'','enabled_sources':[],'utc_offset_minutes':0,
          'location_label':'Location not set','latitude':None,'longitude':None}
SOURCES=('codex','bambu','sun')

def validate(value):
    if not isinstance(value,dict):raise ValueError('Settings must be an object')
    if set(value)-set(DEFAULTS):raise ValueError('Unknown setting')
    v={**DEFAULTS,**value}
    if type(v['schema_version']) is not int or v['schema_version']!=1:raise ValueError('Unsupported settings version')
    if not isinstance(v['device'],str) or (v['device'] and not v['device'].startswith('/dev/')):raise ValueError('Choose a /dev/ serial device')
    enabled=v['enabled_sources']
    if not isinstance(enabled,list) or any(s not in SOURCES for s in enabled) or len(set(enabled))!=len(enabled):raise ValueError('Invalid enabled sources')
    v['enabled_sources']=list(enabled)
    offset=v['utc_offset_minutes']
    if type(offset) is not int or not -720<=offset<=840:raise ValueError('Invalid UTC offset')
    label=v['location_label']
    if not isinstance(label,str) or not 1<=len(label)<=28 or any(ord(c)<32 or ord(c)>126 for c in label):raise ValueError('Use a short ASCII location label')
    for key,limit in (('latitude',90),('longitude',180)):
        n=v[key]
        if n is not None and (type(n) not in (int,float) or not -limit<=n<=limit):raise ValueError('Invalid '+key)
    if (v['latitude'] is None)!=(v['longitude'] is None):raise ValueError('Set both latitude and longitude, or neither')
    return v

def load(root=DATA_ROOT):
    path=Path(root)/'settings.json'
    return validate(json.loads(path.read_text())) if path.exists() else validate({})

def civil_zone(root=DATA_ROOT):return timezone(timedelta(minutes=load(root)['utc_offset_minutes']))

def save(value,root=DATA_ROOT):
    value=validate(value);root=Path(root);root.mkdir(parents=True,exist_ok=True,mode=0o700)
    path=root/'settings.json';tmp=path.with_suffix('.tmp')
    tmp.write_text(json.dumps(value,indent=2)+'\n');tmp.chmod(0o600);tmp.replace(path)

def firmware_header(value):
    v=validate(value);offset=v['utc_offset_minutes'];sign='+' if offset>=0 else '-';hours,minutes=divmod(abs(offset),60)
    label='UTC' if not offset else f'UTC{sign}{hours:02d}:{minutes:02d}'
    return ('#pragma once\nnamespace UserSettings {\n'
            f'constexpr int UtcOffsetMinutes={offset};\n'
            f'constexpr const char* TimeZoneLabel={json.dumps(label)};\n'
            f'constexpr const char* LocationLabel={json.dumps(v["location_label"])};\n'
            f'constexpr bool HasObserver={str(v["latitude"] is not None).lower()};\n'
            f'constexpr double Latitude={v["latitude"] or 0.0},Longitude={v["longitude"] or 0.0};\n'
            'constexpr const char* Branding="";\n}\n')
