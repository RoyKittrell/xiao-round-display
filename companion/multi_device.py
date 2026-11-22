"""Menu selection is a view; remembered targets keep independent workers."""
from pathlib import Path
import json,os,signal,subprocess,sys,time

def read(path,default=None):
    try:
        value=json.loads(Path(path).read_text());return value if isinstance(value,dict) else ({} if default is None else default)
    except (OSError,ValueError):return {} if default is None else default

def device_root(base,identifier):
    if not identifier or len(identifier)>100:raise ValueError('Invalid device identity')
    return Path(base)/'devices'/identifier.encode().hex()

def all_settings(base):
    from device_registry import device_id
    from display_transport import connection,serial_key
    base=Path(base);records=read(base/'device-connections.json');catalog=read(base/'device-catalog.json').get('devices',{})
    result={}
    for identifier,item in catalog.items():
        if isinstance(item,dict):result[identifier]={k:item[k] for k in ('usb_serial','usb_device','wifi_host','bluetooth_id','bluetooth_usb_serial') if k in item}
    for identifier,item in records.items():
        if isinstance(item,dict):result[identifier]={**result.get(identifier,{}),**item}
    current=connection(base);identifier=device_id(current)
    if identifier:result[identifier]=current
    clean={}
    for identifier,item in result.items():
        if not isinstance(identifier,str) or not identifier or len(identifier)>100:continue
        value={'mode':'usb','selection_epoch':'background',**item,'device_id':identifier}
        if value['mode'] not in ('usb','wifi','bluetooth','auto'):continue
        if identifier.startswith('usb:') and serial_key(value.get('usb_serial',''))!=identifier[4:]:continue
        clean[identifier]=value
    return clean

def settings_for(base,identifier):
    value=all_settings(base).get(identifier)
    if value is None:raise OSError('Remembered device was removed')
    return value

def route_key(settings):
    # A new menu selection/epoch does not interrupt an in-flight device feed.
    from device_registry import selection_key
    return selection_key({k:v for k,v in settings.items() if k!='selection_epoch'})

def prepare_preferences(base,identifier,role):
    from device_registry import atomic
    base=Path(base);target=device_root(base,identifier);target.mkdir(parents=True,exist_ok=True)
    prefs=target/'display-preferences.json'
    if role=='desktop' and not prefs.exists():
        from display_control import DEFAULTS,validate
        catalog=read(base/'device-catalog.json').get('devices',{})
        desktops=sorted(k for k,v in catalog.items() if v.get('role')=='desktop')
        legacy=read(base/'display-preferences.json') if not desktops or identifier==desktops[0] else {}
        try:value=validate(legacy) if legacy else dict(DEFAULTS)
        except ValueError:value=dict(DEFAULTS)
        atomic(prefs,value)
    return target

class Coordinator:
    def __init__(self,base,script,popen=subprocess.Popen):
        self.base=Path(base);self.script=Path(script);self.popen=popen;self.children={};self.next_start={}
    def reconcile(self,profiles):
        for identifier in list(self.children):
            if identifier not in profiles:self.stop(identifier)
        for identifier in profiles:
            child=self.children.get(identifier)
            if child and child.poll() is None:continue
            if time.monotonic()<self.next_start.get(identifier,0):continue
            target=device_root(self.base,identifier);(target/'companion').mkdir(mode=0o700,parents=True,exist_ok=True)
            env={**os.environ,'XIAO_DEVICE_STATE_DIR':str(target),'XIAO_MENU_OWNER_PID':str(os.getpid())}
            self.children[identifier]=self.popen([sys.executable,'-B','-u',str(self.script),'--serve-device',identifier],env=env)
            self.next_start[identifier]=time.monotonic()+5
    def stop(self,identifier):
        child=self.children.pop(identifier,None)
        if child and child.poll() is None:
            child.terminate()
            try:child.wait(timeout=2)
            except subprocess.TimeoutExpired:child.kill();child.wait(timeout=2)
    def close(self):
        for identifier in list(self.children):self.stop(identifier)

def run(base,script):
    from device_registry import DeviceRegistry,device_id,atomic
    from display_transport import connection
    from process_owner import check_owner
    base=Path(base);catalog=DeviceRegistry(base);coordinator=Coordinator(base,script)
    def stop(*_):raise SystemExit()
    for s in (signal.SIGTERM,signal.SIGINT):signal.signal(s,stop)
    try:
        while True:
            check_owner();selected=connection(base);catalog.refresh(selected)
            profiles=all_settings(base);coordinator.reconcile(profiles);states={};now=time.time()
            for identifier,settings in profiles.items():
                target=device_root(base,identifier);state=read(target/'helper-status.json')
                child=coordinator.children.get(identifier)
                live=child is not None and child.poll() is None and state.get('worker_pid')==child.pid
                online=live and bool(state.get('display_connected')) and now-state.get('updated_at',0)<20
                state={**state,'display_connected':online}
                item=catalog.devices.setdefault(identifier,{'id':identifier});item['online']=online
                hardware=state.get('hardware',{})
                if hardware.get('role') in ('gym','desktop'):
                    item.update({k:hardware[k] for k in ('role','model','family','firmware','wifi_hardware','wifi_firmware','bluetooth','watch_csc') if k in hardware})
                if online:item['last_seen']=int(now)
                states[identifier]=state
            catalog.save()
            selected_id=device_id(selected);state=dict(states.get(selected_id,{}))
            state.update(selected_id=selected_id,selection=selected.get('selection_epoch','legacy'))
            state['devices']={i:{'display_connected':bool(s.get('display_connected')) and now-s.get('updated_at',0)<20,'transport':s.get('transport'),'worker_pid':s.get('worker_pid'),'sources':s.get('sources',{})} for i,s in states.items()}
            atomic(base/'helper-status.json',state)
            # Compatibility snapshots; the native menu reads namespaced files.
            if selected_id in profiles:
                for name in ('gym-status.json','control-status.json'):
                    value=read(device_root(base,selected_id)/name)
                    if value:atomic(base/name,value)
            time.sleep(.5)
    finally:coordinator.close()


def maintenance_active(target):
    value=read(Path(target)/'firmware-maintenance.json')
    try:
        until=float(value['until']);pid=int(value['pid'])
        if pid<=0 or not time.time()<until<time.time()+360:return False
        os.kill(pid,0);return True
    except (OSError,ValueError,TypeError,KeyError):return False
