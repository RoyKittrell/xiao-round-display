"""Signed-in Codex allowance. Fetch once/minute; replay latest over USB every5s."""
from helper_runtime import Source
import codex_usage

def create(root):
 return Source('codex','Codex usage',codex_usage.snapshot,60,encode=codex_usage.packet,
               cache=root/'assets/codex_usage/latest.json')
