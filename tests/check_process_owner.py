from pathlib import Path
from unittest.mock import patch
from types import SimpleNamespace
import os,sys,tempfile,signal,unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
import process_owner as owner
class Ownership(unittest.TestCase):
 def test_only_orphaned_matching_worker_is_stopped(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);(root/'companion').mkdir();(root/'companion/sun_bridge.lock').write_text('1234')
   script=root/'display_bridge.py'
   for text in ('77 python '+str(script),'1 python /some/other/script.py'):
    with patch.object(owner.subprocess,'run',return_value=SimpleNamespace(stdout=text)),patch.object(owner.os,'killpg') as kill:
     self.assertFalse(owner.stop_orphan(root,script));kill.assert_not_called()
   with patch.object(owner.subprocess,'run',return_value=SimpleNamespace(stdout='1 python '+str(script.resolve()))),patch.object(owner.os,'getpgid',return_value=1234),patch.object(owner.os,'getpgrp',return_value=9999),patch.object(owner.os,'killpg') as kill,patch.object(owner.os,'kill',side_effect=ProcessLookupError):
    self.assertTrue(owner.stop_orphan(root,script));kill.assert_called_once_with(1234,signal.SIGTERM)
 def test_owner_exit_stops_only_own_group(self):
  with patch.dict(os.environ,{'XIAO_MENU_OWNER_PID':'1234'}),patch.object(owner.os,'getppid',return_value=1234),patch.object(owner.os,'killpg') as kill:
   owner.check_owner();kill.assert_not_called()
  with patch.dict(os.environ,{'XIAO_MENU_OWNER_PID':'1234'}),patch.object(owner.os,'getppid',return_value=1),patch.object(owner.os,'getpgrp',return_value=8888),patch.object(owner.os,'getpid',return_value=8888),patch.object(owner.os,'killpg') as kill:
   with self.assertRaises(SystemExit):owner.check_owner()
   kill.assert_called_once_with(8888,signal.SIGTERM)
if __name__=='__main__':unittest.main()
