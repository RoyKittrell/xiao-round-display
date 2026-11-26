#!/usr/bin/env python3
"""One extensible Mac helper for Codex, Bambu, Sun and future display sources.

Independent per-device processes each own their source registry and transport.
The coordinator keeps remembered displays running; menu selection is view-only.
Reconnection replays cached small packets immediately; no source can monopolise
network acquisition. Sun image transfers interleave all ready small packets.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
from datetime import datetime,timezone,timedelta
import fcntl,json,os,sys,time
import signal
from pathlib import Path
import codex_usage,bambu_status,sun_bridge
from helper_runtime import Registry
from hardware_profile import HardwareProfile
from device_registry import DeviceRegistry,Selection,SelectionChanged
from gym_control import GymControl
from display_control import Control
from screen_orientation import Orientation
from process_owner import check_owner,stop_orphan
from display_transport import open_port,endpoint,is_connected,close_bluetooth,set_selection_guard
from sources import build as build_sources
from runtime_config import DATA_ROOT,load,civil_zone

ROOT=DATA_ROOT
sys.path.insert(0,str(Path(__file__).resolve().parent/'vendor'))
import serial

def state_root():return Path(os.environ.get('XIAO_DEVICE_STATE_DIR',ROOT))

@contextmanager
def usb_lock():
    path=state_root()/'companion/usb.lock';path.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
    with path.open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);yield

def sync_clock(port):
    now=datetime.now(civil_zone(ROOT))
    port.write(('T'+now.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]+'\n').encode());port.flush()
    sun_bridge.wait_for(port,'CLOCK ')

def send_usage(snapshot,device,select=False):
    """Compatibility utility; stop the login service before standalone diagnostics."""
    with usb_lock(),open_port(device,115200,timeout=.2,write_timeout=5) as port:
        port.reset_input_buffer();sync_clock(port)
        port.write(codex_usage.packet(snapshot));port.flush();sun_bridge.wait_for(port,'ACK U',8)
        if select:port.write(b'0');port.flush()
    return snapshot

def send_printer(value,device=None,port=None):
    if value is None:return
    if port is not None:
        port.write(bambu_status.packet(value));port.flush();sun_bridge.wait_for(port,'ACK M',3)
    else:
        with usb_lock(),open_port(device,115200,timeout=.2,write_timeout=5) as opened:
            opened.reset_input_buffer();sync_clock(opened);send_printer(value,port=opened)

class Dispatcher:
    def __init__(self,registry,device,select=False,control=None):self.registry=registry;self.device=device;self.select=select;self.control=control;self.hardware=HardwareProfile(state_root());self.binding=None;self.orientation=Orientation(state_root())
    def identify(self,binding):
        with usb_lock(),open_port(self.device,115200,timeout=.15,write_timeout=5) as port:
            port.reset_input_buffer();profile=self.hardware.ensure(port,self.device)
        if profile.get('role')=='gym' and binding.settings.get('usb_serial'):
            from display_transport import serial_key
            if profile.get('chip_serial')!=serial_key(binding.settings['usb_serial']):raise SelectionChanged('Gym identity differs from selected USB serial')
        return profile
    def small(self,port=None,force=False):
        if self.hardware.value.get('role')!='desktop':return
        due=[]
        for state in self.registry.states:
            data,version=state.read()
            if state.source.encode and data is not None and (force or time.monotonic()>=state.next_send):due.append((state,data,version))
        control_due=self.control is not None and self.control.due(force)
        orientation_due=self.binding is not None and self.hardware.value.get("rotation_supported",False) and self.orientation.due()
        if not due and not control_due and not orientation_due:return
        if port is None:
            with usb_lock(),open_port(self.device,115200,timeout=.2,write_timeout=5) as opened:
                opened.reset_input_buffer();profile=self.hardware.ensure(opened,self.device)
                if profile.get("role")!="desktop":return
                if self.control:self.control.protocol_version=profile.get("control_version",2)
                sync_clock(opened)
                if control_due:
                    result=self.control.send(opened)
                    if result:self.hardware.observe_boot(result.get("boot_id"))
                if orientation_due:self.orientation.service(opened,self.binding)
                self._packets(opened,due)
        else:
            if control_due:
                result=self.control.send(port)
                if result:self.hardware.observe_boot(result.get("boot_id"))
            if orientation_due:self.orientation.service(port,self.binding)
            self._packets(port,due)
    def _packets(self,port,due):
        if self.hardware.value.get("role")!="desktop":return
        for state,data,version in due:
            if state.source.id=='weather' and self.hardware.value.get('scene_count',10)<10:
                with state.lock:state.usb_error='Weather needs a firmware update'
                state.next_send=time.monotonic()+60
                continue
            try:
                packet=state.source.encode(data);port.write(packet);port.flush()
                ack='ACK '+packet[1:2].decode();sun_bridge.wait_for(port,ack,5)
                if self.select and state.source.id=='codex':port.write(b'0');port.flush();self.select=False
                with state.lock:state.delivered_at=time.time();state.usb_error=None
                state.last_sent_version=version;state.next_send=time.monotonic()+state.source.send_interval
            except Exception as error:
                with state.lock:state.usb_error=type(error).__name__+': '+str(error)
                raise
    def images(self):
        if self.hardware.value.get("role")!="desktop":return
        for state in self.registry.states:
            data,version=state.read()
            if not state.source.transfer or data is None or version==state.last_sent_version:continue
            try:
                def auxiliary(port):
                    check_owner();self.small(port);health(self.registry,self.device,self.hardware.value,self.binding)
                with usb_lock():state.source.transfer(*data,self.device,on_progress=auxiliary)
                state.last_sent_version=version
                with state.lock:state.delivered_at=time.time();state.usb_error=None
            except Exception as error:
                with state.lock:state.usb_error=type(error).__name__+': '+str(error)
                # Back off image retries, while small packets keep flowing.
                state.last_sent_version=version
                print(state.source.label+' transfer unavailable: '+type(error).__name__,flush=True)

def health(registry,device,hardware=None,binding=None,online=True):
    path=state_root()/'helper-status.json';temporary=path.with_suffix('.tmp')
    temporary.write_text(json.dumps({'updated_at':int(time.time()),'worker_pid':os.getpid(),'usb_connected':bool(device) and not str(device).startswith(('ble://','wifi://')) and Path(device).exists(),'display_connected':online and is_connected(device),'transport':'wifi' if str(device).startswith('wifi://') else 'bluetooth' if str(device).startswith('ble://') else 'usb','sources':registry.status() if (hardware or {}).get('role')=='desktop' else {},'hardware':hardware or {},'selected_id':binding.id if binding else '', 'selection':binding.epoch if binding else ''},indent=2)+'\n');temporary.replace(path)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device',default=load(ROOT)['device'])
    parser.add_argument('--once',action='store_true',help='One Codex refresh; stop service first')
    parser.add_argument('--select',action='store_true',help='Select Codex on first successful delivery')
    parser.add_argument('--status',action='store_true',help='Show source/USB health without opening USB')
    parser.add_argument('--serve-device',help=argparse.SUPPRESS)
    args=parser.parse_args()
    if args.status:
        path=ROOT/'helper-status.json'
        if not path.exists():raise SystemExit('No helper status recorded yet')
        print(path.read_text());return
    workroot=state_root();(workroot/'companion').mkdir(mode=0o700,parents=True,exist_ok=True)
    with (workroot/'companion/sun_bridge.lock').open('a+') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:
            if not stop_orphan(workroot,Path(__file__)):
                raise SystemExit('XIAO Display Helper is already running')
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        lock.seek(0);lock.truncate();lock.write(str(os.getpid()));lock.flush()
        if args.once and 'codex' not in load(ROOT)['enabled_sources']:raise SystemExit('Enable Codex explicitly before querying account usage')
        if args.once:send_usage(codex_usage.snapshot(),args.device,args.select);return
        if not args.serve_device:
            from multi_device import run
            run(ROOT,Path(__file__));return
        def stop(*_):raise SystemExit()
        for s in (signal.SIGTERM,signal.SIGINT):signal.signal(s,stop)
        registry=Registry([]);control=None;dispatcher=Dispatcher(registry,args.device,args.select)
        print('Independent XIAO worker active: '+args.serve_device,flush=True)
        connected=False;health_due=0;binding=None;gym=GymControl(workroot,ROOT);sources_started=False;profile_stamp=None
        try:
            while True:
                from multi_device import maintenance_active
                if maintenance_active(workroot):
                    from wifi_transport import close_wifi
                    close_wifi();close_bluetooth();registry.connected.clear();connected=False
                    time.sleep(.15);continue
                check_owner();current=Selection(ROOT,args.serve_device);selected=endpoint(ROOT,args.device,settings=current.settings)
                if binding is None or binding.key!=current.key or dispatcher.device!=selected:
                    close_bluetooth();binding=current;set_selection_guard(binding)
                    dispatcher.device=selected;connected=False
                    if control:control.boot_id=0
                    dispatcher.hardware.reset(selected);gym.reset();registry.connected.clear()
                marker=workroot/'device-profile.json'
                stamp=marker.stat().st_mtime_ns if marker.exists() else None
                if stamp!=profile_stamp:
                    profile_stamp=stamp;connected=False;dispatcher.hardware.reset(selected)
                    registry.connected.clear()
                dispatcher.binding=binding
                present=str(selected).startswith(('ble://','wifi://')) or bool(selected) and Path(selected).exists()
                if not present:
                    registry.connected.clear();connected=False
                    health(registry,selected,dispatcher.hardware.value,binding);time.sleep(.5);continue
                try:
                    binding.check()
                    if not connected:
                        profile=dispatcher.identify(binding);binding.check()
                        if profile.get('role') not in ('desktop','gym'):
                            registry.connected.clear();health(registry,selected,profile,binding);time.sleep(.5);continue
                        if profile.get('role')=='gym' and sources_started:
                            registry.close();registry=Registry([]);dispatcher.registry=registry
                            control=None;dispatcher.control=None;sources_started=False
                        if profile.get('role')=='desktop' and not sources_started:
                            from multi_device import prepare_preferences
                            prepare_preferences(ROOT,binding.id,'desktop')
                            registry=Registry(build_sources(ROOT));registry.start();sources_started=True
                            control=Control(workroot);dispatcher.registry=registry;dispatcher.control=control
                        connected=True;dispatcher.orientation.reconnect()
                        if control:control.reconnect()
                        registry.reconnect()
                        if profile.get('role')=='gym':gym.begin_session(binding)
                    role=dispatcher.hardware.value.get('role')
                    if role=='desktop':
                        registry.connected.set();dispatcher.small();dispatcher.images()
                    elif role=='gym':
                        registry.connected.clear()
                        rotation_due=dispatcher.hardware.value.get('rotation_supported',False) and dispatcher.orientation.due()
                        if gym.due(binding) or rotation_due:
                            with usb_lock(),open_port(selected,115200,timeout=.15,write_timeout=3) as port:
                                if gym.due(binding):gym.service(port,binding)
                                if rotation_due:dispatcher.orientation.service(port,binding)
                    if time.monotonic()>=health_due:
                        health(registry,selected,dispatcher.hardware.value,binding);health_due=time.monotonic()+1
                except (OSError,TimeoutError,ValueError) as error:
                    registry.connected.clear();connected=False;dispatcher.hardware.reset(selected)
                    print(args.serve_device+' unavailable: '+type(error).__name__,flush=True)
                    health(registry,selected,dispatcher.hardware.value,binding,online=False);time.sleep(.25)
                time.sleep(.15)
        finally:registry.close();close_bluetooth();set_selection_guard(None)
if __name__=='__main__':main()
