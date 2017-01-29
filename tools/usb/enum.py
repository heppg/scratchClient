import usb.core
import usb.util

devices = usb.core.find(find_all=True)

for p in devices:
    print( p)
