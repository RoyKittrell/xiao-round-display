"""Run offline checks only. Does not install, flash or connect to live feeds."""
from pathlib import Path
import ast,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[1]
test_data=tempfile.TemporaryDirectory(prefix='xiao-offline-data-',dir='/tmp')
env={**os.environ,'PYTHONPATH':str(ROOT/'companion'),'PYTHONDONTWRITEBYTECODE':'1','XIAO_DISPLAY_DATA_DIR':test_data.name}
def run(*args):subprocess.run(args,cwd=ROOT,env=env,check=True)
for folder in ('companion','tests','tools','assets'):
    for p in (ROOT/folder).rglob('*.py'):ast.parse(p.read_text(),filename=str(p))
for name in ('test_public_template.py','check_display_control.py','check_clock_recovery.py','check_display_schedule.py',
             'check_helper_runtime.py','check_transport.py','check_service_install.py','check_process_owner.py','check_bambu.py','check_codex_usage.py','check_sun_bridge.py'):
    run(sys.executable,'-B',str(ROOT/'tests'/name))
with tempfile.TemporaryDirectory(prefix='xiao-check-',dir='/tmp') as tmp:
    for name in ('check_clock','check_sun_information','check_frame_tiles','check_host_link'):
        exe=str(Path(tmp)/name)
        run('clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I',str(ROOT/'tests/host_link_stubs') if name=='check_host_link' else str(ROOT/'tests'),str(ROOT/'tests'/(name+'.cpp')),'-o',exe)
        run(exe)
    # Astronomy is C; compile it separately before linking a C++ caller.
    obj=str(Path(tmp)/'astronomy.o')
    run('clang','-O1','-c',str(ROOT/'XiaoPlayground/src/astronomy/astronomy.c'),'-o',obj)
    exe=str(Path(tmp)/'celestial')
    run('clang++','-std=c++17','-O1',str(ROOT/'tests/check_celestial.cpp'),obj,'-lm','-o',exe)
    run(exe)
    if sys.platform=='darwin':
        exe=str(Path(tmp)/'profiles')
        run('swiftc','-swift-version','5','-module-cache-path','/tmp/xiao-swift-cache',str(ROOT/'companion/menu_bar/Profiles.swift'),str(ROOT/'tests/check_profiles.swift'),'-o',exe)
        run(exe)
        run(sys.executable,'-B',str(ROOT/'tests/benchmark_menu.py'))
        run(sys.executable,'-B',str(ROOT/'tests/check_bluetooth_search.py'))
run(sys.executable,'-B',str(ROOT/'tools/privacy_audit.py'))
print('PASS offline suite; no live account/printer/hardware actions performed')
test_data.cleanup()
