"""Check installed runtime placement/rollback without touching real app data."""
from pathlib import Path
from unittest.mock import patch
import importlib.util,subprocess,sys,tempfile,unittest,os
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'companion'))
spec=importlib.util.spec_from_file_location('service_manage',ROOT/'companion/service/manage.py');manage=importlib.util.module_from_spec(spec);spec.loader.exec_module(manage)
class Install(unittest.TestCase):
 def test_gui_scheduling(self):
  self.assertEqual(manage.configuration()['ProcessType'],'Interactive')
 def test_runtime_is_copied_outside_source_and_validated(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);source=root/'source-env';(source/'bin').mkdir(parents=True);(source/'bin/python').write_text('fake interpreter');(source/'pyvenv.cfg').write_text('test')
   data=root/'app-support'
   with patch.object(manage,'DATA',data),patch.object(manage.sys,'prefix',str(source)),patch.object(manage.sys,'base_prefix',str(root/'base')),patch.object(manage.subprocess,'run') as run:
    exe=manage.prepare_runtime();self.assertEqual(exe,data/'python-runtime/bin/python');self.assertTrue(exe.exists());self.assertNotEqual(exe.parent.parent,source)
    run.assert_called_once_with([str(exe),'-B','-c','import PIL,bleak'],check=True,timeout=30)
 def test_failed_validation_restores_previous_runtime(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);source=root/'new-env';source.mkdir();(source/'marker').write_text('new');data=root/'app-support';old=data/'python-runtime';old.mkdir(parents=True);(old/'marker').write_text('old')
   with patch.object(manage,'DATA',data),patch.object(manage.sys,'prefix',str(source)),patch.object(manage.sys,'base_prefix',str(root/'base')),patch.object(manage.subprocess,'run',side_effect=RuntimeError('test failure')):
    with self.assertRaises(RuntimeError):manage.prepare_runtime()
   self.assertEqual((old/'marker').read_text(),'old');self.assertFalse((data/'python-runtime.previous').exists())
 def test_stop_waits_until_old_job_is_unloaded(self):
  with patch.object(manage.sys,'argv',['manage.py','stop']),patch.object(manage,'run') as bootout,patch.object(manage.subprocess,'run',side_effect=[SimpleNamespace(returncode=0),SimpleNamespace(returncode=113)]) as status,patch.object(manage.time,'sleep') as sleep:
   manage.main();bootout.assert_called_once_with('bootout',f'{manage.DOMAIN}/{manage.LABEL}');self.assertEqual(status.call_count,2);sleep.assert_called_once()
 @unittest.skipIf(sys.prefix==sys.base_prefix,'Relocation smoke check requires the documented project venv')
 def test_actual_virtualenv_relocation_and_blank_config(self):
  with tempfile.TemporaryDirectory(prefix='xiao-installed-runtime-',dir='/tmp') as tmp:
   data=Path(tmp)
   with patch.object(manage,'DATA',data):exe=manage.prepare_runtime()
   code="import sys,PIL,bleak;from pathlib import Path;import runtime_config;root=Path(sys.prefix);assert Path(PIL.__file__).is_relative_to(root);assert Path(bleak.__file__).is_relative_to(root);v=runtime_config.load();assert v['device']=='' and v['enabled_sources']==[];print('PASS relocated interpreter/dependencies and blank user configuration')"
   subprocess.run([str(exe),'-B','-c',code],check=True,timeout=30,env={**os.environ,'PYTHONPATH':str(ROOT/'companion'),'XIAO_DISPLAY_DATA_DIR':str(data)})
if __name__=='__main__':unittest.main()
