"""Read-only weather updates, independent of menu UI and other sources."""
from helper_runtime import Source
import weather_status

def create(root):
    return Source('weather','Weather / Open-Meteo',lambda:weather_status.fetch(root),900,
                  encode=weather_status.packet,cache=root/'assets/weather/latest.json',send_interval=30,
                  load=lambda:weather_status.load_cache(root),
                  trigger=lambda:(root/'weather-location.json').stat().st_mtime_ns if (root/'weather-location.json').exists() else 0,
                  invalidated=lambda:weather_status.pending(root),send_on_publish=True)
