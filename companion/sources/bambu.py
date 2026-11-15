"""Read-only local P2S subscription, sampled independently of Codex and NASA."""
from helper_runtime import Source
import bambu_status

def create(root):
 try:monitor=bambu_status.Monitor(root)
 except Exception as error:
  # Report through this source's worker; other sources still start normally.
  message=type(error).__name__
  def unavailable():raise RuntimeError('P2S monitor could not start: '+message)
  return Source('bambu','Bambu Printer',unavailable,30,encode=bambu_status.packet)
 return Source('bambu','Bambu Printer',monitor.snapshot,1,encode=bambu_status.packet,
               cache=root/'assets/bambu/latest.json',close=monitor.close)
