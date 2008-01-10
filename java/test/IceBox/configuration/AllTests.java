// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

import Test.*;

public class AllTests
{
    private static void
    test(boolean b)
    {
        if (!b)
        {
            throw new RuntimeException();
        }
    }

    public static void
    allTests(Ice.Communicator communicator)
    {
        TestIntfPrx service1 = TestIntfPrxHelper.uncheckedCast(communicator.stringToProxy("test:tcp -p 12010"));
        TestIntfPrx service2 = TestIntfPrxHelper.uncheckedCast(communicator.stringToProxy("test:tcp -p 12011"));
        TestIntfPrx service3 = TestIntfPrxHelper.uncheckedCast(communicator.stringToProxy("test:tcp -p 12012"));
        TestIntfPrx service4 = TestIntfPrxHelper.uncheckedCast(communicator.stringToProxy("test:tcp -p 12013"));

        if(service1.getProperty("IceBox.InheritProperties").equals(""))
        {
            System.out.print("testing service properties... ");
            System.out.flush();

            test(service1.getProperty("Ice.ProgramName").equals("IceBox-Service1"));
            test(service1.getProperty("Service").equals("1"));
            test(service1.getProperty("Service1.Ovrd").equals("2"));
            test(service1.getProperty("Service1.Unset").equals(""));
            test(service1.getProperty("Arg").equals("1"));

            String[] args1 = {"-a", "--Arg=2"};
            test(java.util.Arrays.equals(service1.getArgs(), args1));

            test(service2.getProperty("Ice.ProgramName").equals("Test"));
            test(service2.getProperty("Service").equals("2"));
            test(service2.getProperty("Service1.ArgProp").equals(""));
            test(service2.getProperty("IceBox.InheritProperties").equals("1"));

            String[] args2 = {"--Service1.ArgProp=1"};
            test(java.util.Arrays.equals(service2.getArgs(), args2));

            System.out.println("ok");

            System.out.print("testing with shared communicator... ");
            System.out.flush();

            test(service3.getProperty("Ice.ProgramName").equals("IceBox-SharedCommunicator"));
            test(service3.getProperty("Service").equals("4"));
            test(service3.getProperty("Prop").equals(""));
            test(service3.getProperty("Service3.Prop").equals("1"));
            test(service3.getProperty("Ice.Trace.Network").equals("3"));

            test(service4.getProperty("Ice.ProgramName").equals("IceBox-SharedCommunicator"));
            test(service4.getProperty("Service").equals("4"));
            test(service4.getProperty("Prop").equals(""));
            test(service4.getProperty("Service3.Prop").equals("1"));
            test(service4.getProperty("Ice.Trace.Network").equals("3"));

            String[] args4 = {"--Service3.Prop=2"};
            test(java.util.Arrays.equals(service4.getArgs(), args4));

            System.out.println("ok");
        }
        else
        {
            System.out.print("testing property inheritance... ");
            System.out.flush();

            test(service1.getProperty("Ice.ProgramName").equals("IceBox2-Service1"));
            test(service1.getProperty("ServerProp").equals("1"));
            test(service1.getProperty("OverrideMe").equals("2"));
            test(service1.getProperty("UnsetMe").equals(""));
            test(service1.getProperty("Service1.Prop").equals("1"));
            test(service1.getProperty("Service1.ArgProp").equals("2"));

            test(service2.getProperty("Ice.ProgramName").equals("IceBox2-SharedCommunicator"));
            test(service2.getProperty("ServerProp").equals("1"));
            test(service2.getProperty("OverrideMe").equals("3"));
            test(service2.getProperty("UnsetMe").equals(""));
            test(service2.getProperty("Service2.Prop").equals("1"));

            System.out.println("ok");
        }
    }
}