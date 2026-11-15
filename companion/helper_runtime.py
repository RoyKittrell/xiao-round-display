"""Independent source workers and immutable latest-value mailboxes.

A source owns data acquisition/encoding. The coordinator alone owns USB.
Slow/failed sources cannot hold another source's fetch worker or erase its cache.
"""
from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
import json,os,threading,time
from typing import Callable

@dataclass
class Source:
    id: str
    label: str
    fetch: Callable
    interval: float
    encode: Callable | None = None
    transfer: Callable | None = None
    cache: Path | None = None
    close: Callable | None = None
    send_interval: float = 5
    load: Callable | None = None

class SourceState:
    def __init__(self,source):
        self.source=source;self.lock=threading.Lock();self.latest=None;self.version=0
        self.fetched_at=0;self.delivered_at=0;self.last_sent_version=-1;self.next_send=0
        self.error=None;self.usb_error=None;self.thread=None
        try:
            cached=source.load() if source.load else json.loads(source.cache.read_text()) if source.cache and source.cache.exists() else None
            if cached is not None:
                if source.encode:source.encode(cached) # Shape check before replay.
                self.latest=cached;self.version=1
        except Exception:self.error='Cached data unavailable; awaiting a fresh reading'
    def read(self):
        with self.lock:return self.latest,self.version
    def publish(self,data):
        if data is None:return False
        if self.source.encode:self.source.encode(data)
        with self.lock:
            self.latest=data;self.version+=1;self.fetched_at=time.time();self.error=None
        if self.source.cache:
            path=self.source.cache;path.parent.mkdir(parents=True,exist_ok=True)
            temporary=path.with_name(path.name+'.tmp')
            temporary.write_text(json.dumps(data,indent=2)+'\n');temporary.replace(path)
        return True
    def status(self):
        with self.lock:
            observed=self.latest.get('observed_at') if isinstance(self.latest,dict) else None
            return {'label':self.source.label,'available':self.latest is not None,'last_fetched_at':self.fetched_at,
                    'source_observed_at':observed,'last_delivered_at':self.delivered_at,'fetch_error':self.error,'usb_error':self.usb_error}
    def start(self,stop,connected):
        def work():
            due=0
            while not stop.is_set():
                if not connected.wait(.25):continue
                if time.monotonic()<due:
                    stop.wait(min(.25,due-time.monotonic()));continue
                try:self.publish(self.source.fetch())
                except Exception as error:
                    message=type(error).__name__+': '+str(error)
                    with self.lock:
                        if message!=self.error:print(self.source.label+' refresh unavailable: '+message,flush=True)
                        self.error=message
                due=time.monotonic()+self.source.interval
        self.thread=threading.Thread(target=work,name='XIAO '+self.source.id,daemon=True);self.thread.start()

class Registry:
    def __init__(self,sources):
        ids=[s.id for s in sources]
        if len(set(ids))!=len(ids):raise ValueError('Source IDs must be unique')
        self.states=[SourceState(s) for s in sources];self.stop_event=threading.Event();self.connected=threading.Event()
    def start(self):
        for state in self.states:state.start(self.stop_event,self.connected)
    def reconnect(self):
        # Replay cached small packets immediately, independent of fetch deadlines.
        for state in self.states:state.next_send=0;state.last_sent_version=-1
    def status(self):return {state.source.id:state.status() for state in self.states}
    def close(self):
        self.stop_event.set();self.connected.set()
        for state in self.states:
            if state.source.close:state.source.close()
