#!/usr/bin/env python
# **********************************************************************
#
# Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

import pexpect, sys, os

try:
    import demoscript
except ImportError:
    for toplevel in [".", "..", "../..", "../../..", "../../../.."]:
        toplevel = os.path.normpath(toplevel)
        if os.path.exists(os.path.join(toplevel, "demoscript")):
            break
    else:
        raise "can't find toplevel directory!"
    sys.path.append(os.path.join(toplevel))
    import demoscript

import demoscript.Util
import demoscript.Freeze.library

print "cleaning databases...",
sys.stdout.flush()
demoscript.Util.cleanDbDir("db")
print "ok"

server = demoscript.Util.spawn('./server --Ice.PrintAdapterReady --Freeze.Trace.Evictor=0 --Freeze.Trace.DbEnv=0')
server.expect('.* ready')
client = demoscript.Util.spawn('./client')
client.expect('>>> ')

demoscript.Freeze.library.run(client, server)

client.sendline('shutdown')
server.expect(pexpect.EOF)
assert server.wait() == 0

client.sendline('exit')
client.expect(pexpect.EOF)
assert server.wait() == 0

print "running with collocated server"

print "cleaning databases...",
sys.stdout.flush()
demoscript.Util.cleanDbDir("db")
print "ok"

server = demoscript.Util.spawn('./collocated --Freeze.Trace.Evictor=0 --Freeze.Trace.DbEnv=0')
server.expect('>>> ')

demoscript.Freeze.library.run(server, server)

server.sendline('exit')
server.expect(pexpect.EOF)
assert server.wait() == 0