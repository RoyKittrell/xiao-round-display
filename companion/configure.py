"""Explicit local setup; no account credentials are accepted on the command line."""
from pathlib import Path
import argparse,json
from runtime_config import DATA_ROOT,SOURCES,load,save,firmware_header

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,default=DATA_ROOT)
    parser.add_argument('--list-devices',action='store_true')
    parser.add_argument('--device')
    parser.add_argument('--enable',choices=SOURCES,action='append',default=[])
    parser.add_argument('--disable',choices=SOURCES,action='append',default=[])
    parser.add_argument('--utc-offset-minutes',type=int)
    parser.add_argument('--location-label')
    parser.add_argument('--latitude',type=float);parser.add_argument('--longitude',type=float)
    parser.add_argument('--write-firmware-settings',action='store_true')
    args=parser.parse_args()
    if args.list_devices:
        from serial.tools import list_ports
        for port in list_ports.comports():print(port.device,port.description)
        return
    v=load(args.data)
    for key in ('device','utc_offset_minutes','location_label','latitude','longitude'):
        value=getattr(args,key)
        if value is not None:v[key]=value
    enabled=set(v['enabled_sources']);enabled.update(args.enable);enabled.difference_update(args.disable)
    v['enabled_sources']=[s for s in SOURCES if s in enabled]
    # Validate before writing either file.
    header=firmware_header(v)
    if args.write_firmware_settings:
        path=Path(__file__).resolve().parents[1]/'XiaoPlayground/UserSettings.local.h'
        path.write_text(header)
    save(v,args.data)
    print('Saved local settings. Restart the helper after changing settings.')
    if args.write_firmware_settings:print('Local firmware settings written. Recompile and flash to apply.')

if __name__=='__main__':main()
