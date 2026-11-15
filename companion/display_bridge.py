#!/usr/bin/env python3
"""One extensible Mac helper for Codex, Bambu, Sun and future display sources.

Independent fetch workers -> latest-value registry -> one serial dispatcher.
Reconnection replays cached small packets immediately; no source can monopolise
network acquisition. Sun image transfers interleave all ready small packets.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
from datetime import datetime,timezone,timedelta
import fcntl,json,os,sys,time
from pathlib import Path
import codex_usage,bambu_status,sun_bridge
from helper_runtime import Registry
from display_control import Control
from process_owner import check_owner,stop_orphan
from display_transport import open_port,endpoint,is_connected,close_bluetooth
from sources import build as build_sources
from runtime_config import DATA_ROOT,load,civil_zone

ROOT=DATA_ROOT
sys.path.insert(0,str(Path(__file__).resolve().parent/'vendor'))
import serial

@contextmanager
def usb_lock():
    with (ROOT/'companion/usb.lock').open('a') as lock:
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
    def __init__(self,registry,device,select=False,control=None):self.registry=registry;self.device=device;self.select=select;self.control=control
    def small(self,port=None,force=False):
        due=[]
        for state in self.registry.states:
            data,version=state.read()
            if state.source.encode and data is not None and (force or time.monotonic()>=state.next_send):due.append((state,data,version))
        control_due=self.control is not None and self.control.due(force)
        if not due and not control_due:return
        if port is None:
            with usb_lock(),open_port(self.device,115200,timeout=.2,write_timeout=5) as opened:
                opened.reset_input_buffer();sync_clock(opened)
                if control_due:self.control.send(opened)
                self._packets(opened,due)
        else:
            if control_due:self.control.send(port)
            self._packets(port,due)
    def _packets(self,port,due):
        for state,data,version in due:
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
        for state in self.registry.states:
            data,version=state.read()
            if not state.source.transfer or data is None or version==state.last_sent_version:continue
            try:
                def auxiliary(port):
                    check_owner();self.small(port);health(self.registry,self.device)
                with usb_lock():state.source.transfer(*data,self.device,on_progress=auxiliary)
                state.last_sent_version=version
                with state.lock:state.delivered_at=time.time();state.usb_error=None
            except Exception as error:
                with state.lock:state.usb_error=type(error).__name__+': '+str(error)
                # Back off image retries, while small packets keep flowing.
                state.last_sent_version=version
                print(state.source.label+' transfer unavailable: '+type(error).__name__,flush=True)

def health(registry,device):
    path=ROOT/'helper-status.json';temporary=path.with_suffix('.tmp')
    temporary.write_text(json.dumps({'updated_at':int(time.time()),'usb_connected':bool(device) and not str(device).startswith('ble://') and Path(device).exists(),'display_connected':bool(device) and is_connected(device),'transport':'bluetooth' if str(device).startswith('ble://') else 'usb','sources':registry.status()},indent=2)+'\n');temporary.replace(path)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device',default=load(ROOT)['device'])
    parser.add_argument('--once',action='store_true',help='One Codex refresh; stop service first')
    parser.add_argument('--select',action='store_true',help='Select Codex on first successful delivery')
    parser.add_argument('--status',action='store_true',help='Show source/USB health without opening USB')
    args=parser.parse_args()
    if args.status:
        path=ROOT/'helper-status.json'
        if not path.exists():raise SystemExit('No helper status recorded yet')
        print(path.read_text());return
    (ROOT/'companion').mkdir(parents=True,exist_ok=True)
    with (ROOT/'companion/sun_bridge.lock').open('a+') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:
            if not stop_orphan(ROOT,Path(__file__)):
                raise SystemExit('XIAO Display Helper is already running')
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        lock.seek(0);lock.truncate();lock.write(str(os.getpid()));lock.flush()
        if args.once and 'codex' not in load(ROOT)['enabled_sources']:raise SystemExit('Enable Codex explicitly before querying account usage')
        if args.once:send_usage(codex_usage.snapshot(),args.device,args.select);return
        registry=Registry(build_sources(ROOT));control=Control(ROOT);dispatcher=Dispatcher(registry,args.device,args.select,control)
        print('Unified XIAO helper active: '+', '.join(s.source.label for s in registry.states),flush=True)
        registry.start();connected=False;health_due=0
        try:
            while True:
                check_owner()
                selected=endpoint(ROOT,args.device)
                if dispatcher.device!=selected:
                    close_bluetooth();dispatcher.device=selected;connected=False;control.boot_id=0
                present=str(selected).startswith('ble://') or bool(selected) and Path(selected).exists()
                if not present:
                    registry.connected.clear();connected=False;health(registry,dispatcher.device);time.sleep(1);continue
                registry.connected.set()
                if not connected:registry.reconnect();control.reconnect();connected=True
                try:
                    dispatcher.small();dispatcher.images()
                except (OSError,TimeoutError) as error:
                    print('Display delivery unavailable: '+type(error).__name__+'; replaying on reconnect',flush=True)
                    connected=False;time.sleep(1)
                if time.monotonic()>=health_due:
                    health(registry,dispatcher.device);health_due=time.monotonic()+2
                time.sleep(.25)
        finally:registry.close();close_bluetooth()
if __name__=='__main__':main()
