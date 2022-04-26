#! /usr/bin/env python

import subprocess
import sys
import os

try:
    server = subprocess.Popen([sys.argv[1],"-a","1000","-r","2","-m","250"])
except Exception as e:
    print( "mqserver start failed", e )
    sys.exit( 1 )

try:
    client = subprocess.run([sys.argv[2]])
except Exception as e:
    print( "mqclient start failed", e )
    sys.exit( 1 )
finally:
    server.terminate()

print( client )

exit( client.returncode )
