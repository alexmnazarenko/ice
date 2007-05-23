#!/usr/bin/env ruby
# **********************************************************************
#
# Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

require 'pathname'

rubyDir = nil
for toplevel in [".", "..", "../..", "../../..", "../../../.."]
    path = Pathname.new(toplevel).join("ruby", "Ice.rb")
    if path.file?
        rubyDir = Pathname.new(toplevel).join("ruby")
        break
    end
end
if not rubyDir
    fail "can't find toplevel directory!"
end
$:.unshift(rubyDir)

#
# Find Slice directory.
#
slice_dir = ''
if ENV.has_key?('ICERB_HOME') and Pathname.new(ENV['ICERB_HOME']).join("slice").directory?
    slice_dir = ENV['ICERB_HOME'] + '/slice'
elsif ENV.has_key?('ICE_HOME') and Pathname.new(ENV['ICE_HOME']).join("slice").directory?
    slice_dir = ENV['ICE_HOME'] + '/slice'
else
    puts $0 + ': Slice directory not found. Define ICERB_HOME or ICE_HOME.'
    exit(1)
end

require 'Ice'
Ice::loadSlice('-I' + slice_dir + ' Test.ice')
require 'AllTests'

def test(b)
    if !b
        raise RuntimeError, 'test assertion failed'
    end
end

def run(args, communicator)
    myClass = allTests(communicator)

    myClass.shutdown()
    return true
end

begin
    initData = Ice::InitializationData.new
    initData.properties = Ice.createProperties(ARGV)
    communicator = Ice.initialize(ARGV, initData)
    status = run(ARGV, communicator)
rescue => ex
    puts $!
    print ex.backtrace.join("\n")
    status = false
end

if communicator
    begin
        communicator.destroy()
    rescue => ex
        puts $!
        print ex.backtrace.join("\n")
        status = false
    end
end

exit(status ? 0 : 1)