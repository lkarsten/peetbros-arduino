from operator import xor

import sys
#payload = sys.stdin.read()
payload = "GPAAM,A,A,0.10,N,WPTNME"
payload = "GPRMC,163722.000,A,5946.7479,N,01034.4009,E,0005.38,358.66,060713,,"
print  "%02x" % reduce(xor, (ord(s) for s in payload))
