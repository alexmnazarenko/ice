// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package IceInternal;

class PropertiesAdminI extends Ice._PropertiesAdminDisp
{
    PropertiesAdminI(Ice.Properties properties)
    {
        _properties = properties;
    }
    
    public String
    getProperty(String name, Ice.Current current)
    {
        return _properties.getProperty(name);
    }
    
    public String
    getPropertyWithDefault(String name, String dflt, Ice.Current current)
    {
        return _properties.getPropertyWithDefault(name, dflt);
    }

    public int
    getPropertyAsInt(String name, Ice.Current current)
    {
        return _properties.getPropertyAsInt(name);
    }

    public int
    getPropertyAsIntWithDefault(String name, int dflt, Ice.Current current)
    {
        return _properties.getPropertyAsIntWithDefault(name, dflt);
    }

    public java.util.Map
    getPropertiesForPrefix(String name, Ice.Current current)
    {
        return _properties.getPropertiesForPrefix(name);
    }
    
    private final Ice.Properties _properties;
}