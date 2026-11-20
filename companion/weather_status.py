"""Open-Meteo hourly weather, acquired by the existing independent worker.

Forecast probabilities describe the preceding hour at each provider timestamp.
Convert these to explicit interval starts before sending them to the display.
Personal location is configured locally, never obtained from printer/account data.
"""
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import Request, urlopen
import binascii,hashlib,json,math,time,unicodedata

COUNT=30
DEFAULT_LOCATION=None

def location(root):
    path=Path(root)/'weather-location.json'
    if not path.exists():raise ValueError('Select a weather location before enabling weather')
    v=json.loads(path.read_text())
    if not finite(v.get('latitude'),-90,90) or not finite(v.get('longitude'),-180,180):raise ValueError('Invalid weather location')
    if type(v.get('utc_offset_seconds',0)) is not int:raise ValueError('Invalid weather timezone offset')
    return v

def location_key(v):
    fields={k:v.get(k) for k in ('latitude','longitude','city','timezone')}
    return hashlib.sha256(json.dumps(fields,sort_keys=True).encode()).hexdigest()

def display_city(v):
    return ''.join(c for c in unicodedata.normalize('NFKD',str(v.get('city','Local weather'))) if 32<=ord(c)<=126).strip()[:24] or 'Local weather'

def pending(root):
    v=location(root);now=int(time.time());offset=v.get('utc_offset_seconds',0)
    return {'observed_at':now,'base':(now+offset)//3600*3600-offset,'offset':offset,'city':display_city(v),
            'temperature10':-9999,'uv10':-1,'probabilities':[255]*COUNT,'codes':[255]*COUNT,'days':[255]*COUNT,
            'location_key':location_key(v),'pending':True}

def load_cache(root):
    path=Path(root)/'assets/weather/latest.json'
    if not path.exists():return None
    s=json.loads(path.read_text())
    return s if s.get('location_key')==location_key(location(root)) else None

def finite(value,lo,hi):
    return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value) and lo<=value<=hi

def normalize(raw,location,now=None):
    now=int(time.time() if now is None else now)
    hourly=raw['hourly'];timestamps=hourly['time']
    columns=['precipitation_probability','weather_code','is_day','uv_index']
    if not isinstance(timestamps,list) or not timestamps or len(timestamps)>200:raise ValueError('Invalid hourly weather timestamps')
    if any(len(hourly[k])!=len(timestamps) for k in columns):raise ValueError('Weather arrays disagree')
    if any(type(t) is not int for t in timestamps):raise ValueError('Expected Unix hourly timestamps')
    if any(b-a!=3600 for a,b in zip(timestamps,timestamps[1:])):raise ValueError('Weather hours are not consecutive')
    lookup={t-3600:i for i,t in enumerate(timestamps)}
    starts=[t for t in lookup if t<=now]
    if not starts:raise ValueError('Current forecast hour missing')
    base=max(starts)
    if now-base>=3600:raise ValueError('Current forecast hour missing')
    probabilities=[];codes=[];days=[]
    for k in range(COUNT):
        i=lookup.get(base+k*3600)
        def item(name,lo,hi):
            v=None if i is None else hourly[name][i]
            return round(v) if finite(v,lo,hi) else 255
        probabilities.append(item('precipitation_probability',0,100))
        codes.append(item('weather_code',0,99))
        days.append(item('is_day',0,1))
    if all(p==255 for p in probabilities[:25]):raise ValueError('No usable rain forecast')
    current=raw.get('current',{});temperature=current.get('temperature_2m')
    current_time=current.get('time')
    if type(current_time) is not int or abs(now-current_time)>3600:temperature=None
    i=lookup.get(base);uv=None if i is None else hourly['uv_index'][i]
    result={'observed_at':now,'base':base,'offset':raw.get('utc_offset_seconds',location.get('utc_offset_seconds',0)),
            'city':display_city(location),'temperature10':round(temperature*10) if finite(temperature,-90,65) else -9999,
            'uv10':round(uv*10) if finite(uv,0,50) else -1,
            'probabilities':probabilities,'codes':codes,'days':days,'location_key':location_key(location),'pending':False}
    packet(result) # Validate once before publishing/caching.
    return result

def packet(s):
    def integer(k,lo,hi):
        v=s[k]
        if type(v) is not int or not lo<=v<=hi:raise ValueError('Invalid weather '+k)
        return str(v)
    city=s['city']
    if not isinstance(city,str) or not 1<=len(city)<=24 or any(ord(c)<32 or ord(c)>126 for c in city):raise ValueError('Invalid weather city')
    encoded=[]
    for key,limit in [('probabilities',100),('codes',99),('days',1)]:
        a=s[key]
        if not isinstance(a,list) or len(a)!=COUNT or any(type(v) is not int or not (0<=v<=limit or v==255) for v in a):raise ValueError('Invalid weather '+key)
        encoded.append(bytes(a).hex())
    if (s['base']+s['offset'])%3600:raise ValueError('Unaligned local weather interval')
    body='|'.join(['W','1',integer('observed_at',946684800,4102444800),integer('base',946684800,4102444800),
                   integer('offset',-43200,50400),integer('temperature10',-9999,650),integer('uv10',-1,500),
                   city.encode('ascii').hex(),*encoded]).encode()
    return b'@'+body+b'|'+f'{binascii.crc_hqx(body,0xffff):04x}'.encode()+b'\n'

def fetch(root):
    loc=location(root)
    params={'latitude':loc['latitude'],'longitude':loc['longitude'],'timezone':loc.get('timezone') or 'auto','timeformat':'unixtime',
            'past_hours':3,'forecast_hours':36,'current':'temperature_2m',
            'hourly':'precipitation_probability,weather_code,is_day,uv_index'}
    request=Request('https://api.open-meteo.com/v1/forecast?'+urlencode(params),headers={'User-Agent':'XIAO-Display-Helper/2.3 Weather (+https://open-meteo.com/)'})
    with urlopen(request,timeout=20) as response:
        data=response.read(2_000_001)
    if len(data)>2_000_000:raise ValueError('Weather response too large')
    if location_key(location(root))!=location_key(loc):raise ValueError('Weather location changed during refresh; retrying the new place')
    return normalize(json.loads(data),loc)
