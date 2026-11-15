#!/usr/bin/env python3
"""Read signed-in Codex quota through its documented app-server RPC.

No model calls, credential-file reads, or reset-credit consumption. Returned
snapshots contain only display labels, percentages, durations and reset times.
"""
from __future__ import annotations
import binascii
import json
import math
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import time
import unicodedata

MAX_WINDOWS = 8

def codex_binary():
    candidates = [os.environ.get('XIAO_CODEX_BIN'), shutil.which('codex'),
        '/Applications/ChatGPT.app/Contents/Resources/codex-cli/CodexCLI.app/Contents/MacOS/codex',
        '/Applications/Codex.app/Contents/Resources/codex']
    for path in candidates:
        if path and Path(path).is_file():
            return str(path)
    raise RuntimeError('Codex executable unavailable; set XIAO_CODEX_BIN')

def read_rate_limits(timeout=25):
    # Start an isolated stdio connection. No TCP listener or agent thread is used.
    with subprocess.Popen([codex_binary(), 'app-server'], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                          bufsize=0) as process:
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        buffered = bytearray()
        def send(message):
            process.stdin.write((json.dumps(message)+'\n').encode())
            process.stdin.flush()
        def call(identifier, method, params=None):
            message = {'id': identifier, 'method': method}
            if params is not None:
                message['params'] = params
            send(message)
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if b'\n' not in buffered:
                    if not selector.select(max(0, deadline-time.monotonic())):
                        break
                    chunk = os.read(process.stdout.fileno(), 65536)
                    if not chunk:
                        raise RuntimeError('Codex app server closed its connection')
                    buffered.extend(chunk)
                while b'\n' in buffered:
                    line, _, rest = buffered.partition(b'\n')
                    buffered[:] = rest
                    result = json.loads(line)
                    if result.get('id') != identifier:
                        continue
                    if 'error' in result:
                        # Do not log upstream payloads (may contain account details).
                        raise RuntimeError('Codex usage RPC failed (code %s)' % result['error'].get('code', 'unknown'))
                    return result.get('result', {})
            raise TimeoutError('Codex usage request timed out')
        try:
            call(0, 'initialize', {'clientInfo': {'name': 'xiao_display_helper',
                 'title': "XIAO Display Display", 'version': '1.0'}})
            send({'method': 'initialized', 'params': {}})
            return call(1, 'account/rateLimits/read')
        finally:
            selector.close()
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

def label(text):
    ascii_text = unicodedata.normalize('NFKD', str(text)).encode('ascii', 'ignore').decode()
    clean = ''.join(c if c.isalnum() or c in ' -./' else ' ' for c in ascii_text)
    return ' '.join(clean.split())[:22] or 'Codex'

def window_name(minutes):
    if minutes == 10080:
        return 'Weekly'
    if minutes == 1440:
        return 'Daily'
    if minutes % 60 == 0:
        return f'{minutes//60} hour'
    return f'{minutes} min'

def normalise(response, observed=None):
    buckets = response.get('rateLimitsByLimitId')
    if not buckets:
        legacy = response.get('rateLimits')
        buckets = {'codex': legacy} if legacy else {}
    windows = []
    # Always keep the standard Codex bucket first, with any model-specific
    # buckets following. Never count legacy+map twice or invent a missing limit.
    for key in sorted(buckets, key=lambda k: (k != 'codex', k)):
        bucket = buckets[key]
        if not isinstance(bucket, dict):
            continue
        for field in ('primary', 'secondary'):
            window = bucket.get(field)
            if not isinstance(window, dict):
                continue
            used, minutes, reset = (window.get(k) for k in ('usedPercent', 'windowDurationMins', 'resetsAt'))
            if isinstance(used, bool) or not isinstance(used, (int, float)) or not math.isfinite(used):
                continue
            if isinstance(minutes, bool) or not isinstance(minutes, int) or not 0 < minutes <= 5256000:
                continue
            if isinstance(reset, bool) or not isinstance(reset, int) or not 946684800 <= reset <= 4102444800:
                continue
            title = window_name(minutes)
            if key != 'codex':
                title = label(bucket.get('limitName') or key)[:10] + ' ' + title
            windows.append({'label': label(title), 'remaining_tenths': round(max(0, min(100, 100-used))*10),
                            'duration_mins': minutes, 'resets_at': reset})
    if not windows:
        raise RuntimeError('No usable signed-in Codex quota windows returned')
    if len(windows) > MAX_WINDOWS:
        raise RuntimeError('More quota windows than this display supports; refusing to omit limits')
    return {'observed_at': int(time.time() if observed is None else observed), 'windows': windows}

def packet(snapshot):
    fields = ['U', str(snapshot['observed_at']), str(len(snapshot['windows']))]
    for item in snapshot['windows']:
        fields += [item['label'], str(item['remaining_tenths']), str(item['duration_mins']), str(item['resets_at'])]
    body = '|'.join(fields).encode('ascii')
    return b'@'+body+b'|'+f'{binascii.crc_hqx(body, 0xffff):04x}'.encode()+b'\n'

def snapshot():
    return normalise(read_rate_limits())

if __name__ == '__main__':
    print(json.dumps(snapshot(), indent=2))
