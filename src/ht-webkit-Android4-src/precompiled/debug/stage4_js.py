#!/usr/bin/env python

import string
import sys
import os

# MAGIC: -213173581276 (= -1 * 0x44 * 0xbadadd07)
# Magic checksum
MAGIC = -213173581276

KEY = '\xa2\xb0\xc8\xa5\xef\x11\xf4\x12\xc2g$L\xb1\xbe\xc6\x15f\x1b\xda\xde\xae\x906\xc1uh\xb0\xa9d\xc5C\x0e'
FAKE_KEY = '\xd6[\x88\x1e!\xc3s\xac\xb4=\x13\xbc7\xc5\x0cu:C\xa1?\x1f\xc0\xaef\xe7\xd0\x91 \x83H\x82x'

def stage4_js(magic):
    if str(magic) == str(MAGIC):
        key = KEY
    else:
        key = FAKE_KEY

    with open("stage4.js") as fp:
        content = fp.read()

    tpl = string.Template(content)

    key_js  = "["
    key_js += ", ".join(["0x{:02x}".format(ord(x)) for x in key])
    key_js += "]"

    return tpl.safe_substitute({"R_KEY": key_js})
    
def main():
    env = os.environ
    magic = env.get('_REQUEST__trk')

    data = stage4_js(magic)
    sys.stdout.write(data)
    sys.stdout.write("\n")

if __name__ == "__main__":
    main()
