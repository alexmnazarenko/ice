// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package IceSSL;

final class AcceptorI implements IceInternal.Acceptor
{
    public java.nio.channels.ServerSocketChannel
    fd()
    {
        return _fd;
    }

    public void
    close()
    {
        if(_instance.networkTraceLevel() >= 1)
        {
            String s = "stopping to accept ssl connections at " + toString();
            _logger.trace(_instance.networkTraceCategory(), s);
        }

        java.nio.channels.ServerSocketChannel fd;
        java.nio.channels.Selector selector;
        synchronized(this)
        {
            fd = _fd;
            selector = _selector;
            _fd = null;
            _selector = null;
        }
        if(fd != null)
        {
            IceInternal.Network.closeSocketNoThrow(fd);
        }
        if(selector != null)
        {
            try
            {
                selector.close();
            }
            catch(java.io.IOException ex)
            {
                // Ignore.
            }
        }
    }

    public void
    listen()
    {
        // Nothing to do.

        if(_instance.networkTraceLevel() >= 1)
        {
            String s = "accepting ssl connections at " + toString();
            _logger.trace(_instance.networkTraceCategory(), s);
        }
    }

    public IceInternal.Transceiver
    accept(int timeout)
    {
        //
        // The plugin may not be fully initialized.
        //
        if(!_instance.initialized())
        {
            Ice.PluginInitializationException ex = new Ice.PluginInitializationException();
            ex.reason = "IceSSL: plugin is not initialized";
            throw ex;
        }

        java.nio.channels.SocketChannel fd = null;
        while(fd == null)
        {
            try
            {
                fd = _fd.accept();
                if(fd == null)
                {
                    if(_selector == null)
                    {
                        _selector = java.nio.channels.Selector.open();
                    }

                    while(true)
                    {
                        try
                        {
                            java.nio.channels.SelectionKey key =
                                _fd.register(_selector, java.nio.channels.SelectionKey.OP_ACCEPT);
                            if(timeout > 0)
                            {
                                if(_selector.select(timeout) == 0)
                                {
                                    throw new Ice.TimeoutException();
                                }
                            }
                            else if(timeout == 0)
                            {
                                if(_selector.selectNow() == 0)
                                {
                                    throw new Ice.TimeoutException();
                                }
                            }
                            else
                            {
                                _selector.select();
                            }

                            break;
                        }
                        catch(java.io.IOException ex)
                        {
                            if(IceInternal.Network.interrupted(ex))
                            {
                                continue;
                            }
                            Ice.SocketException se = new Ice.SocketException();
                            se.initCause(ex);
                            throw se;
                        }
                    }
                }
            }
            catch(java.io.IOException ex)
            {
                if(IceInternal.Network.interrupted(ex))
                {
                    continue;
                }
                Ice.SocketException se = new Ice.SocketException();
                se.initCause(ex);
                throw se;
            }
        }

        javax.net.ssl.SSLEngine engine = null;
        try
        {
            try
            {
                java.net.Socket socket = fd.socket();
                socket.setTcpNoDelay(true);
                socket.setKeepAlive(true);
            }
            catch(java.io.IOException ex)
            {
                Ice.SocketException se = new Ice.SocketException();
                se.initCause(ex);
                throw se;
            }

            IceInternal.Network.setBlock(fd, false);
            IceInternal.Network.setTcpBufSize(fd, _instance.communicator().getProperties(), _logger);

            engine = _instance.createSSLEngine(true);
        }
        catch(RuntimeException ex)
        {
            IceInternal.Network.closeSocketNoThrow(fd);
            throw ex;
        }

        if(_instance.networkTraceLevel() >= 1)
        {
            _logger.trace(_instance.networkTraceCategory(), "accepting ssl connection\n" +
                          IceInternal.Network.fdToString(fd));
        }

        return new TransceiverI(_instance, engine, fd, "", true, true, _adapterName);
    }

    public void
    connectToSelf()
    {
        java.nio.channels.SocketChannel fd = IceInternal.Network.createTcpSocket();
        IceInternal.Network.setBlock(fd, false);
        IceInternal.Network.doConnect(fd, _addr, -1);
        IceInternal.Network.closeSocketNoThrow(fd);
    }

    public String
    toString()
    {
        return IceInternal.Network.addrToString(_addr);
    }

    int
    effectivePort()
    {
        return _addr.getPort();
    }

    AcceptorI(Instance instance, String adapterName, String host, int port)
    {
        _instance = instance;
        _adapterName = adapterName;
        _logger = instance.communicator().getLogger();
        _backlog = 0;

        if(_backlog <= 0)
        {
            _backlog = 5;
        }

        try
        {
            _fd = IceInternal.Network.createTcpServerSocket();
            IceInternal.Network.setBlock(_fd, false);
            IceInternal.Network.setTcpBufSize(_fd, _instance.communicator().getProperties(), _logger);
            if(!System.getProperty("os.name").startsWith("Windows"))
            {
                //
                // Enable SO_REUSEADDR on Unix platforms to allow
                // re-using the socket even if it's in the TIME_WAIT
                // state. On Windows, this doesn't appear to be
                // necessary and enabling SO_REUSEADDR would actually
                // not be a good thing since it allows a second
                // process to bind to an address even it's already
                // bound by another process.
                //
                // TODO: using SO_EXCLUSIVEADDRUSE on Windows would
                // probably be better but it's only supported by recent
                // Windows versions (XP SP2, Windows Server 2003).
                //
                IceInternal.Network.setReuseAddress(_fd, true);
            }
            _addr = IceInternal.Network.getAddressForServer(host, port, _instance.protocolSupport());
            if(_instance.networkTraceLevel() >= 2)
            {
                String s = "attempting to bind to ssl socket " + toString();
                _logger.trace(_instance.networkTraceCategory(), s);
            }
            _addr = IceInternal.Network.doBind(_fd, _addr);
        }
        catch(RuntimeException ex)
        {
            _fd = null;
            throw ex;
        }
    }

    protected synchronized void
    finalize()
        throws Throwable
    {
        IceUtilInternal.Assert.FinalizerAssert(_fd == null);

        super.finalize();
    }

    private Instance _instance;
    private String _adapterName;
    private Ice.Logger _logger;
    private java.nio.channels.ServerSocketChannel _fd;
    private int _backlog;
    private java.net.InetSocketAddress _addr;
    private java.nio.channels.Selector _selector;
}