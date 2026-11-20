"""Privacy/setup checks; use synthetic fixtures, temp directories and no network."""
from pathlib import Path
import json,sys,tempfile,unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'companion'))
import runtime_config as config
from sources import build

class PublicTemplate(unittest.TestCase):
    def test_default_is_unconfigured_and_never_starts_sources(self):
        with tempfile.TemporaryDirectory() as tmp:
            v=config.load(tmp)
            self.assertEqual(v['device'],'');self.assertEqual(v['enabled_sources'],[])
            self.assertIsNone(v['latitude']);self.assertIsNone(v['longitude'])
            v['enabled_sources'].append('sun')
            self.assertEqual(config.load(tmp)['enabled_sources'],[])
            with patch('codex_usage.snapshot') as quota,patch('bambu_status.Monitor') as printer,patch('sun_bridge.download') as sun:
                self.assertEqual(build(Path(tmp)),[])
                quota.assert_not_called();printer.assert_not_called();sun.assert_not_called()
    def test_weather_has_no_default_city(self):
        import weather_status
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):weather_status.location(Path(tmp))
            self.assertIn('weather',config.SOURCES)

    def test_explicit_enable_roundtrip_and_private_permissions(self):
        with tempfile.TemporaryDirectory() as tmp:
            v={**config.DEFAULTS,'enabled_sources':['sun'],'device':'/dev/cu.usbmodemEXAMPLE'}
            config.save(v,tmp);self.assertEqual(config.load(tmp),v)
            self.assertEqual((Path(tmp)/'settings.json').stat().st_mode & 0o777,0o600)
            self.assertEqual([s.id for s in build(Path(tmp))],['sun'])
    def test_bad_config_and_arbitrary_source_names_rejected(self):
        for value in ({'enabled_sources':['other_module']},{'utc_offset_minutes':9999},
                      {'latitude':12},{'latitude':91,'longitude':0},
                      {'location_label':'bad\nlocation'},{'device':'https://example.invalid'},
                      {'access_token':'not a supported field'}):
            with self.assertRaises(ValueError):config.validate(value)
    def test_fixed_offset_and_optional_observer_header_match(self):
        for offset in (0,330,-210,840,-720):
            v={**config.DEFAULTS,'utc_offset_minutes':offset}
            self.assertIn(f'UtcOffsetMinutes={offset};',config.firmware_header(v))
            self.assertIn('HasObserver=false;',config.firmware_header(v))
            with tempfile.TemporaryDirectory() as tmp:
                config.save(v,tmp);self.assertEqual(config.civil_zone(tmp).utcoffset(None).total_seconds(),offset*60)
        v={**config.DEFAULTS,'latitude':1.0,'longitude':2.0,'location_label':'Example'}
        self.assertIn('HasObserver=true;',config.firmware_header(v))

if __name__=='__main__':unittest.main()
