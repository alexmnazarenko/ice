// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

namespace IceInternal
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.Net;
    using System.Threading;

    public class EndpointHostResolver
    {
        internal EndpointHostResolver(Instance instance)
        {
            _instance = instance;

            _thread = new HelperThread(this);
            _thread.Start();
        }

        public void resolve(string host, int port, EndpointI endpoint, EndpointI_connectors callback)
        {
            //
            // Try to get the addresses without DNS lookup. If this doesn't work, we queue a resolve
            // entry and the thread will take care of getting the endpoint addresses.
            //
            try
            {
                List<IPEndPoint> addrs = Network.getAddresses(host, port, _instance.protocolSupport(), false);
                if(addrs.Count > 0)
                {
                    callback.connectors(endpoint.connectors(addrs));
                    return;
                }
            }
            catch(Ice.LocalException ex)
            {
                callback.exception(ex);
                return;
            }

            lock(this)
            {
                Debug.Assert(!_destroyed);

                ResolveEntry entry = new ResolveEntry();
                entry.host = host;
                entry.port = port;
                entry.endpoint = endpoint;
                entry.callback = callback;
                _queue.AddLast(entry);
                Monitor.Pulse(this);
            }
        }

        public void destroy()
        {
            lock(this)
            {
                Debug.Assert(!_destroyed);
                _destroyed = true;
                Monitor.Pulse(this);
            }
        }

        public void joinWithThread()
        {
            if(_thread != null)
            {
                _thread.Join();
            }
        }

        public void run()
        {
            while(true)
            {
                ResolveEntry resolve;
                lock(this)
                {
                    while(!_destroyed && _queue.Count == 0)
                    {
                        Monitor.Wait(this);
                    }

                    if(_destroyed)
                    {
                        break;
                    }

                    resolve = _queue.First.Value;
                    _queue.RemoveFirst();
                }

                resolve.callback.connectors(resolve.endpoint.connectors(
                    Network.getAddresses(resolve.host, resolve.port, _instance.protocolSupport())));
            }

            foreach(ResolveEntry entry in _queue)
            {
                entry.callback.exception(new Ice.CommunicatorDestroyedException());
            }
            _queue.Clear();
        }

        private class ResolveEntry
        {
            internal string host;
            internal int port;
            internal EndpointI endpoint;
            internal EndpointI_connectors callback;
        }

        private Instance _instance;
        private bool _destroyed;
        private LinkedList<ResolveEntry> _queue = new LinkedList<ResolveEntry>();

        private sealed class HelperThread
        {
            internal HelperThread(EndpointHostResolver resolver)
            {
                _resolver = resolver;
                _name = _resolver._instance.initializationData().properties.getProperty("Ice.ProgramName");
                if(_name.Length > 0)
                {
                    _name += "-";
                }
                _name += "Ice.EndpointHostResolverThread";
            }

            public void Join()
            {
                _thread.Join();
            }

            public void Start()
            {
                _thread = new Thread(new ThreadStart(Run));
                _thread.IsBackground = true;
                _thread.Name = _name;
                _thread.Start();
            }

            public void Run()
            {
                if(_resolver._instance.initializationData().threadHook != null)
                {
                    _resolver._instance.initializationData().threadHook.start();
                }

                try
                {
                    _resolver.run();
                }
                catch(Ice.LocalException ex)
                {
                    string s = "exception in endpoint host resolver thread " + _name + ":\n" + ex;
                    _resolver._instance.initializationData().logger.error(s);
                }
                catch(System.Exception ex)
                {
                    string s = "unknown exception in endpoint host resolver thread " + _name + ":\n" + ex;
                    _resolver._instance.initializationData().logger.error(s);
                }

                if(_resolver._instance.initializationData().threadHook != null)
                {
                    _resolver._instance.initializationData().threadHook.stop();
                }
            }

            private EndpointHostResolver _resolver;
            private string _name;
            private Thread _thread;
        }

        private HelperThread _thread;
    }
}