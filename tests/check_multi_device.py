"""Actual bindings, process reconciliation and state isolation across selections."""
from pathlib import Path
import json,os,sys,tempfile,time,unittest,subprocess
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'companion'))
from multi_device import Coordinator,all_settings,device_root,prepare_preferences
from device_registry import Selection,SelectionChanged,atomic
from display_control import Control,DEFAULTS
import display_bridge as bridge
DESK='usb:aabbccddeeff';GYM='usb:001122334455'

def profile(identifier,epoch='one'):
 return {'device_id':identifier,'usb_serial':identifier[4:],'usb_device':'/dev/cu.'+identifier[4:],'mode':'usb','selection_epoch':epoch}
def seed(root):
 d,g=profile(DESK),profile(GYM)
 atomic(root/'device-connections.json',{DESK:d,GYM:g});atomic(root/'display-connection.json',g)
 atomic(root/'device-catalog.json',{'devices':{DESK:{'id':DESK,'role':'desktop',**d},GYM:{'id':GYM,'role':'gym',**g}}})
 return d,g

class MultiDevice(unittest.TestCase):
 def test_menu_selection_does_not_abort_either_binding(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);d,g=seed(root);desktop=Selection(root,DESK);gym=Selection(root,GYM)
   atomic(root/'display-connection.json',{**d,'selection_epoch':'two'})
   desktop.check();gym.check();self.assertEqual(desktop.epoch,'two');self.assertEqual(gym.epoch,'one')
   # The menu can move back without closing either device's in-flight transfer.
   atomic(root/'display-connection.json',{**g,'selection_epoch':'three'})
   desktop.check();gym.check();self.assertEqual(gym.epoch,'three')
 def test_connection_edit_invalidates_only_its_device(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);d,g=seed(root);desktop=Selection(root,DESK);gym=Selection(root,GYM)
   atomic(root/'device-connections.json',{DESK:{**d,'mode':'auto'},GYM:g})
   with self.assertRaises(SelectionChanged):desktop.check()
   gym.check()
 def test_legacy_preferences_migrate_without_cross_device_writes(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);seed(root);old={**DEFAULTS,'enabled_mask':384,'interval_seconds':300};atomic(root/'display-preferences.json',old)
   before=(root/'display-preferences.json').read_bytes();desk=prepare_preferences(root,DESK,'desktop');gym=prepare_preferences(root,GYM,'gym')
   self.assertEqual(json.loads((desk/'display-preferences.json').read_text())['enabled_mask'],384)
   self.assertFalse((gym/'display-preferences.json').exists())
   control=Control(desk);control.value['enabled_mask']=128
   atomic(desk/'display-preferences.json',control.value)
   self.assertEqual((root/'display-preferences.json').read_bytes(),before)
   self.assertNotEqual(device_root(root,DESK),device_root(root,GYM))
 def test_unsafe_identity_and_saved_identity_mismatch_are_rejected(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);seed(root);r=json.loads((root/'device-connections.json').read_text());r[DESK]['usb_serial']='001122334455';atomic(root/'device-connections.json',r)
   self.assertNotIn(DESK,all_settings(root))
   self.assertEqual(device_root(root,'../bad').parent,root/'devices')
 def test_locks_on_one_board_do_not_block_another(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp)
   with patch.dict(os.environ,{'XIAO_DEVICE_STATE_DIR':str(device_root(root,DESK))}):
    with bridge.usb_lock():
     with patch.dict(os.environ,{'XIAO_DEVICE_STATE_DIR':str(device_root(root,GYM))}):
      with bridge.usb_lock():pass
 def test_workers_survive_selection_changes_and_one_worker_failure(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);d,g=seed(root);script=root/'fake_device.py'
   script.write_text("import os,time,pathlib\np=pathlib.Path(os.environ['XIAO_DEVICE_STATE_DIR'])/'ticks'\nwhile True:\n p.write_text(str(time.time()))\n time.sleep(.03)\n")
   coordinator=Coordinator(root,script)
   try:
    coordinator.reconcile(all_settings(root));pids={k:v.pid for k,v in coordinator.children.items()}
    end=time.monotonic()+2
    while time.monotonic()<end and not all((device_root(root,k)/'ticks').exists() for k in pids):time.sleep(.02)
    self.assertTrue(all((device_root(root,k)/'ticks').exists() for k in pids))
    atomic(root/'display-connection.json',d);coordinator.reconcile(all_settings(root))
    self.assertEqual(pids,{k:v.pid for k,v in coordinator.children.items()})
    old=float((device_root(root,DESK)/'ticks').read_text())
    coordinator.children[GYM].terminate();coordinator.children[GYM].wait(timeout=2)
    time.sleep(.1);coordinator.reconcile(all_settings(root))
    self.assertGreater(float((device_root(root,DESK)/'ticks').read_text()),old)
    self.assertEqual(coordinator.children[DESK].pid,pids[DESK])
    coordinator.next_start[GYM]=0;coordinator.reconcile(all_settings(root))
    self.assertNotEqual(coordinator.children[GYM].pid,pids[GYM])
   finally:coordinator.close()
   self.assertFalse(coordinator.children)
if __name__=='__main__':unittest.main()
