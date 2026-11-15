"""NASA/NOAA acquisition runs independently; only the main loop transfers rows."""
from helper_runtime import Source
import sun_bridge

def create(root):
 return Source('sun','Sun / NOAA',sun_bridge.download,900,transfer=sun_bridge.send)
