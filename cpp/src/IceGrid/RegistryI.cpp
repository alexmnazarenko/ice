// **********************************************************************
//
// Copyright (c) 2003-2006 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceUtil/UUID.h>
#include <Ice/Ice.h>

#include <IceGrid/RegistryI.h>
#include <IceGrid/LocatorI.h>
#include <IceGrid/LocatorRegistryI.h>
#include <IceGrid/AdminI.h>
#include <IceGrid/QueryI.h>
#include <IceGrid/TraceLevels.h>
#include <IceGrid/Database.h>
#include <IceGrid/NodeSessionI.h>
#include <IceGrid/ReapThread.h>
#include <IceGrid/AdminSessionI.h>
#include <IceGrid/Topics.h>

#include <IceStorm/Service.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#   include <direct.h>
#   define S_ISDIR(mode) ((mode) & _S_IFDIR)
#   define S_ISREG(mode) ((mode) & _S_IFREG)
#else
#   include <unistd.h>
#endif

using namespace std;
using namespace Ice;
using namespace IceGrid;

namespace IceGrid
{

class NodeSessionReapable : public Reapable
{
public:

    NodeSessionReapable(const NodeSessionIPtr& session, const NodeSessionPrx& proxy) : 
	_session(session),
	_proxy(proxy)
    {
    }

    virtual ~NodeSessionReapable()
    {
    }
	
    virtual IceUtil::Time
    timestamp() const
    {
	return _session->timestamp();
    }

    virtual void
    destroy()
    {
	_proxy->destroy();
    }

private:

    const NodeSessionIPtr _session;
    const NodeSessionPrx _proxy;
};

};

RegistryI::RegistryI(const CommunicatorPtr& communicator) : _communicator(communicator)
{
}

RegistryI::~RegistryI()
{
}

bool
RegistryI::start(bool nowarn)
{
    assert(_communicator);
    PropertiesPtr properties = _communicator->getProperties();

    //
    // Initialize the database environment.
    //
    string dbPath = properties->getProperty("IceGrid.Registry.Data");
    if(dbPath.empty())
    {
	Error out(_communicator->getLogger());
	out << "property `IceGrid.Registry.Data' is not set";
	return false;
    }
    else
    {
	struct stat filestat;
	if(stat(dbPath.c_str(), &filestat) != 0 || !S_ISDIR(filestat.st_mode))
	{
	    Error out(_communicator->getLogger());
	    SyscallException ex(__FILE__, __LINE__);
	    ex.error = getSystemErrno();
	    out << "property `IceGrid.Registry.Data' is set to an invalid path:\n" << ex;
	    return false;
	}
    }

    //
    // Check that required properties are set and valid.
    //
    if(properties->getProperty("IceGrid.Registry.Client.Endpoints").empty())
    {
	Error out(_communicator->getLogger());
	out << "property `IceGrid.Registry.Client.Endpoints' is not set";
	return false;
    }

    if(properties->getProperty("IceGrid.Registry.Server.Endpoints").empty())
    {
	Error out(_communicator->getLogger());
	out << "property `IceGrid.Registry.Server.Endpoints' is not set";
	return false;
    }

    if(properties->getProperty("IceGrid.Registry.Internal.Endpoints").empty())
    {
	Error out(_communicator->getLogger());
	out << "property `IceGrid.Registry.Internal.Endpoints' is not set";
	return false;
    }

    if(!properties->getProperty("IceGrid.Registry.Admin.Endpoints").empty())
    {
	if(!nowarn)
	{
	    Warning out(_communicator->getLogger());
	    out << "administrative endpoints `IceGrid.Registry.Admin.Endpoints' enabled";
	}
    }

    properties->setProperty("Ice.PrintProcessId", "0");
    properties->setProperty("Ice.ServerIdleTime", "0");
    properties->setProperty("IceGrid.Registry.Client.AdapterId", "");
    properties->setProperty("IceGrid.Registry.Server.AdapterId", "");
    properties->setProperty("IceGrid.Registry.Admin.AdapterId", "");
    properties->setProperty("IceGrid.Registry.Internal.AdapterId", "");

    setupThreadPool(properties, "IceGrid.Registry.Client.ThreadPool", 1, 10);
    setupThreadPool(properties, "IceGrid.Registry.Server.ThreadPool", 1, 10);
    setupThreadPool(properties, "IceGrid.Registry.Admin.ThreadPool", 1, 10);
    setupThreadPool(properties, "IceGrid.Registry.Internal.ThreadPool", 1, 100);

    TraceLevelsPtr traceLevels = new TraceLevels(properties, _communicator->getLogger(), false);

    //
    // Create the object adapters.
    //
    ObjectAdapterPtr serverAdapter = _communicator->createObjectAdapter("IceGrid.Registry.Server");
    ObjectAdapterPtr clientAdapter = _communicator->createObjectAdapter("IceGrid.Registry.Client");
    ObjectAdapterPtr adminAdapter = _communicator->createObjectAdapter("IceGrid.Registry.Admin");
    ObjectAdapterPtr registryAdapter = _communicator->createObjectAdapter("IceGrid.Registry.Internal");

    //
    // Start the reaper thread.
    //
    _nodeSessionTimeout = properties->getPropertyAsIntWithDefault("IceGrid.Registry.NodeSessionTimeout", 10);
    _reaper = new ReapThread(_nodeSessionTimeout);
    _reaper->start();

    int adminSessionTimeout = properties->getPropertyAsIntWithDefault("IceGrid.Registry.AdminSessionTimeout", 10);
    if(adminSessionTimeout != _nodeSessionTimeout)
    {
	_adminReaper = new ReapThread(adminSessionTimeout);
	_adminReaper->start();
    }

    //
    // Setup the wait queue (used for allocation request timeouts).
    //
    _waitQueue = new WaitQueue();
    _waitQueue->start();

    //
    // Get the instance name
    //
    const string instanceNameProperty = "IceGrid.InstanceName";
    string instanceName = properties->getPropertyWithDefault(instanceNameProperty, "IceGrid");

    //
    // Create the internal registries (node, server, adapter, object).
    //
    const string envName = "Registry";
    properties->setProperty("Freeze.DbEnv.Registry.DbHome", dbPath);
    _database = new Database(registryAdapter, envName, instanceName, _nodeSessionTimeout, traceLevels);

    //
    // Create the locator registry and locator interfaces.
    //
    bool dynamicReg = properties->getPropertyAsInt("IceGrid.Registry.DynamicRegistration") > 0;
    ObjectPtr locatorRegistry = new LocatorRegistryI(_database, dynamicReg);
    ObjectPrx obj = serverAdapter->add(locatorRegistry, 
				       stringToIdentity(instanceName + "/" + IceUtil::generateUUID()));
    LocatorRegistryPrx locatorRegistryPrx = LocatorRegistryPrx::uncheckedCast(obj);
    ObjectPtr locator = new LocatorI(_communicator, _database, locatorRegistryPrx, 0); 
    Identity locatorId = stringToIdentity(instanceName + "/Locator");
    clientAdapter->add(locator, locatorId);


    Ice::Identity registryId = stringToIdentity(instanceName + "/Registry");
    registryAdapter->add(this, registryId);
    registryAdapter->activate();

    //
    // Create the internal IceStorm service and the registry and node topics.
    //
    _iceStorm = IceStorm::Service::create(_communicator, 
					  registryAdapter, 
					  registryAdapter, 
					  "IceGrid.Registry", 
 					  stringToIdentity(instanceName + "/TopicManager"),
					  "Registry");

    NodeObserverTopic* nodeTopic = new NodeObserverTopic(_iceStorm->getTopicManager());
    _nodeObserver = NodeObserverPrx::uncheckedCast(registryAdapter->addWithUUID(nodeTopic));

    RegistryObserverTopic* regTopic = new RegistryObserverTopic(_iceStorm->getTopicManager());
    _registryObserver = RegistryObserverPrx::uncheckedCast(registryAdapter->addWithUUID(regTopic));

    _database->setObservers(_registryObserver, _nodeObserver);

    //
    // Create the query, admin, session manager interfaces
    //
    Identity queryId = stringToIdentity(instanceName + "/Query");
    clientAdapter->add(new QueryI(_communicator, _database, 0), queryId);

    ReapThreadPtr reaper = _adminReaper ? _adminReaper : _reaper; // TODO: XXX

    Identity sessionMgrId = stringToIdentity(instanceName + "/SessionManager");
    ObjectPtr sessionMgr = new ClientSessionManagerI(_database, reaper, _waitQueue, locatorRegistryPrx, 
						     adminSessionTimeout); // TODO: XXX
    clientAdapter->add(sessionMgr, sessionMgrId);

    Identity adminId = stringToIdentity(instanceName + "/Admin");
    adminAdapter->add(new AdminI(_database, this, traceLevels), adminId);

    Identity admSessionMgrId = stringToIdentity(instanceName + "/AdminSessionManager");
    ObjectPtr admSessionMgr = new AdminSessionManagerI(*regTopic, *nodeTopic, _database, reaper, _waitQueue, 
						       locatorRegistryPrx, adminSessionTimeout);
    adminAdapter->add(admSessionMgr, admSessionMgrId);

    //
    // Register well known objects with the object registry.
    //
    addWellKnownObject(registryAdapter->createProxy(registryId), Registry::ice_staticId());
    addWellKnownObject(clientAdapter->createProxy(queryId), Query::ice_staticId());
    addWellKnownObject(clientAdapter->createProxy(sessionMgrId), SessionManager::ice_staticId());
    addWellKnownObject(adminAdapter->createProxy(adminId), Admin::ice_staticId());
    addWellKnownObject(adminAdapter->createProxy(admSessionMgrId), SessionManager::ice_staticId());

    //
    // We are ready to go!
    //
    serverAdapter->activate();
    clientAdapter->activate();
    adminAdapter->activate();
    
    return true;
}

void
RegistryI::stop()
{
    _reaper->terminate();
    _reaper->getThreadControl().join();
    _reaper = 0;

    if(_adminReaper)
    {
	_adminReaper->terminate();
	_adminReaper->getThreadControl().join();
	_adminReaper = 0;
    }

    _waitQueue->destroy();
    _waitQueue = 0;

    _iceStorm->stop();
    _iceStorm = 0;

    _database->destroy();
    _database = 0;
}

NodeSessionPrx
RegistryI::registerNode(const std::string& name, const NodePrx& node, const NodeInfo& info, NodeObserverPrx& obs, 
			const Ice::Current& c)
{
    NodePrx n = NodePrx::uncheckedCast(node->ice_timeout(_nodeSessionTimeout * 1000));
    NodeSessionIPtr session = new NodeSessionI(_database, name, n, info);
    NodeSessionPrx proxy = NodeSessionPrx::uncheckedCast(c.adapter->addWithUUID(session));
    _reaper->add(new NodeSessionReapable(session, proxy));
    obs = _nodeObserver;
    return proxy;
}

int
RegistryI::getTimeout(const Ice::Current& current) const
{
    return _nodeSessionTimeout;
}

void
RegistryI::shutdown(const Ice::Current& current)
{
    _communicator->shutdown();
}

IceStorm::TopicManagerPrx
RegistryI::getTopicManager()
{
    return _iceStorm->getTopicManager();
}

void
RegistryI::addWellKnownObject(const Ice::ObjectPrx& proxy, const string& type)
{
    assert(_database);
    try
    {
	_database->removeObject(proxy->ice_getIdentity());
    }
    catch(const IceGrid::ObjectNotRegisteredException&)
    {
    }
    ObjectInfo info;
    info.proxy = proxy;
    info.type = type;
    _database->addObject(info);
}

void
RegistryI::setupThreadPool(const Ice::PropertiesPtr& properties, const string& name, int size, int sizeMax)
{
    if(properties->getPropertyAsIntWithDefault(name + ".Size", 0) < size)
    {
	ostringstream os;
	os << size;
	properties->setProperty(name + ".Size", os.str());
    }
    else
    {
	size = properties->getPropertyAsInt(name + ".Size");
    }

    if(sizeMax > 0 && properties->getPropertyAsIntWithDefault(name + ".SizeMax", 0) < sizeMax)
    {
	if(size >= sizeMax)
	{
	    sizeMax = size * 10;
	}
	
	ostringstream os;
	os << sizeMax;
	properties->setProperty(name + ".SizeMax", os.str());
    }
}
