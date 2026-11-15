"""Recover only an orphaned packaged helper, verified by PID/parent/argv."""
import os,signal,subprocess,time
from pathlib import Path

def stop_orphan(root,script):
    lock=Path(root)/'companion/sun_bridge.lock'
    try:pid=int(lock.read_text().strip())
    except (OSError,ValueError):return False
    if pid<=1 or pid==os.getpid():return False
    result=subprocess.run(['/bin/ps','-p',str(pid),'-o','ppid=,command='],capture_output=True,text=True)
    parts=result.stdout.strip().split(None,1)
    if len(parts)!=2 or parts[0]!='1' or str(Path(script).resolve()) not in parts[1]:return False
    try:
        group=os.getpgid(pid)
        grouped=group==pid and group!=os.getpgrp()
        if grouped:os.killpg(group,signal.SIGTERM)
        else:os.kill(pid,signal.SIGTERM)
        until=time.monotonic()+2
        while time.monotonic()<until:
            try:os.kill(pid,0)
            except ProcessLookupError:return True
            time.sleep(.05)
        if grouped:os.killpg(group,signal.SIGKILL)
        else:os.kill(pid,signal.SIGKILL)
    except ProcessLookupError:return True
    return True

def check_owner():
    expected=int(os.environ.get('XIAO_MENU_OWNER_PID','0'))
    if expected and os.getppid()!=expected:
        # NSTask owns a dedicated process group, including its read-only RPC child.
        if os.getpgrp()==os.getpid():os.killpg(os.getpgrp(),signal.SIGTERM)
        raise SystemExit('Menu owner exited')
