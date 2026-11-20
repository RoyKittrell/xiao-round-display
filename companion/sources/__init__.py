"""One allowlisted registry for the display's independent data sources.

Future scenes add an adapter here plus their bounded firmware packet handler.
No external configuration can execute/import arbitrary modules.
"""
from .codex import create as codex
from .bambu import create as bambu
from .sun import create as sun
from .weather import create as weather
FACTORIES={'codex':codex,'bambu':bambu,'sun':sun,'weather':weather}
def build(root):
 from runtime_config import load
 return [FACTORIES[name](root) for name in load(root)['enabled_sources']]
