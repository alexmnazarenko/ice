#!/usr/bin/env python
# **********************************************************************
#
# Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

import os, sys, imp

sys.path.append(os.path.join(os.path.dirname(__file__), "config"))
import TestUtil

testGroups = []

for d in [ "cpp", "java", "cs", "py", "rb", "php" ]:
    
    filename = os.path.abspath(os.path.join(os.path.dirname(__file__), d, "allTests.py"))
    f = file(filename, "r")
    current_mod = imp.load_module("allTests", f, filename, (".py", "r", imp.PY_SOURCE)) 
    f.close()

    tests = TestUtil.getTestSet([ os.path.join(d, "test", x) for x in current_mod.tests ])
    if len(tests) > 0:
        testGroups.extend(tests)

TestUtil.rootRun(testGroups)