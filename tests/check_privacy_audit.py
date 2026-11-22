"""Audit runtime artifacts and staged blobs using an isolated synthetic repo."""
from pathlib import Path
from unittest.mock import patch
import contextlib,importlib.util,io,subprocess,tempfile,unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('privacy_audit',ROOT/'tools/privacy_audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)

class Privacy(unittest.TestCase):
    def test_hardware_addresses_must_use_synthetic_fixtures(self):
        fixture=ROOT/'example.txt'
        address=':'.join(['02','23','45','67','89','ab'])
        self.assertTrue(audit.audit([fixture],read_bytes=lambda _:address.encode()))
        self.assertFalse(audit.audit([fixture],read_bytes=lambda _:b'00:11:22:33:44:55'))

    def test_saved_device_state_and_generated_directories_are_rejected(self):
        for name in ('device-names.json','device-catalog.json','device-connections.json',
                     'gym-status.json','firmware-maintenance.json','device-profile.json',
                     'devices/example/settings.json','build/application.bin','Example.app/Contents/file'):
            errors=audit.audit([ROOT/name],read_bytes=lambda _:b'{}')
            self.assertTrue(errors,name)

    def test_staged_secret_is_found_even_if_worktree_was_cleaned(self):
        with tempfile.TemporaryDirectory(prefix='xiao-audit-',dir='/tmp') as tmp:
            root=Path(tmp)
            def git(*args):
                return subprocess.run(['git','-C',str(root),*args],check=True,capture_output=True)
            git('init','--quiet')
            file=root/'example.txt'
            file.write_text('sk-'+('x'*40))  # Synthetic detector fixture.
            git('add','example.txt')
            file.write_text('cleaned worktree\n')
            with patch.object(audit,'ROOT',root),contextlib.redirect_stdout(io.StringIO()) as output:
                with self.assertRaises(SystemExit):audit.main()
            self.assertIn('example.txt',output.getvalue())
            self.assertNotIn('x'*40,output.getvalue())
            git('add','example.txt')
            with patch.object(audit,'ROOT',root),contextlib.redirect_stdout(io.StringIO()):audit.main()

if __name__=='__main__':unittest.main()
