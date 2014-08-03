#!/usr/bin/env python
#Based on Duck's script from http://forum.i2p2.de/viewtopic.php?t=4367
import base64, hashlib, sys

if len(sys.argv) != 2:
   print 'Usage: convertkey.py <base64key>'
   sys.exit(1)

key = sys.argv[1]
raw_key = base64.b64decode(key, '-~')
hash = hashlib.sha256(raw_key)
base32_hash = base64.b32encode(hash.digest())
print base32_hash.lower().replace('=', '')+'.b32.i2p'
