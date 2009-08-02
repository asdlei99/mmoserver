/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_COMMON_MESSAGEDISPATCH_H
#define ANH_COMMON_MESSAGEDISPATCH_H

//#include <winsock2.h>
#include "Utils/typedefs.h"
#include "NetworkManager/NetworkCallback.h"
#include <map>
#include "zthread/ZThread.h"


//======================================================================================================================

class Service;
class DispatchClient;
class MessageDispatchCallback;
class Message;

typedef std::map<uint32, MessageDispatchCallback*>   MessageCallbackMap;
typedef std::map<uint32, DispatchClient*>            AccountClientMap;

//======================================================================================================================

class MessageDispatch : public NetworkCallback
{
	public:

		MessageDispatch(void);
		~MessageDispatch(void);

		void						Startup(Service* service);
		void						Shutdown(void);
		void						Process(void);

		void						RegisterMessageCallback(uint32 opcode, MessageDispatchCallback* callback);
		void						UnregisterMessageCallback(uint32 opcode);
		AccountClientMap*			getClientMap(){return(&mAccountClientMap);}

		// Inherited NetworkCallback
		virtual NetworkClient*		handleSessionConnect(Session* session, Service* service);
		virtual void				handleSessionDisconnect(NetworkClient* client);
		virtual void				handleSessionMessage(NetworkClient* client, Message* message);

	private:

		Service*					mRouterService;
		MessageCallbackMap			mMessageCallbackMap;
		AccountClientMap			mAccountClientMap;
		ZThread::RecursiveMutex		mSessionMutex;
};

//======================================================================================================================

#endif //MMOSERVER_COMMON_MESSAGEDISPATCH_H




