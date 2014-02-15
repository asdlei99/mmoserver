/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/

#include "ShuttleFactory.h"
#include "ZoneServer/PlayerEnums.h"
#include "Zoneserver/Objects/Inventory.h"
#include "ZoneServer/Objects/Object/ObjectFactoryCallback.h"
#include "ZoneServer\Objects\Object\ObjectManager.h"

#include "Shuttle.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"
#include "anh/Utils/rand.h"
#include "Utils/utils.h"

//=============================================================================

bool			ShuttleFactory::mInsFlag    = false;
ShuttleFactory*	ShuttleFactory::mSingleton  = NULL;

//======================================================================================================================

ShuttleFactory*	ShuttleFactory::Init(swganh::app::SwganhKernel*	kernel)
{
    if(!mInsFlag)
    {
        mSingleton = new ShuttleFactory(kernel);
        mInsFlag = true;
        return mSingleton;
    }
    else
        return mSingleton;
}

//=============================================================================

ShuttleFactory::ShuttleFactory(swganh::app::SwganhKernel*	kernel) : FactoryBase(kernel)
{
    _setupDatabindings();
}

//=============================================================================

ShuttleFactory::~ShuttleFactory()
{
    _destroyDatabindings();

    mInsFlag = false;
    delete(mSingleton);
}

//=============================================================================

void ShuttleFactory::handleDatabaseJobComplete(void* ref,swganh::database::DatabaseResult* result)
{
    QueryContainerBase* asyncContainer = reinterpret_cast<QueryContainerBase*>(ref);

    switch(asyncContainer->mQueryType)
    {
    case SHFQuery_MainData:
    {
        Shuttle* shuttle = _createShuttle(result);

        if(shuttle->getLoadState() == LoadState_Loaded && asyncContainer->mOfCallback)
        {
            asyncContainer->mOfCallback->handleObjectReady(shuttle,asyncContainer->mClient);
        }
        else
        {

        }
    }
    break;

    default:
        break;
    }

    mQueryContainerPool.free(asyncContainer);
}

//=============================================================================

void ShuttleFactory::requestObject(ObjectFactoryCallback* ofCallback, uint64 id, uint16 subGroup, uint16 subType, DispatchClient* client)
{
	std::stringstream sql;

	sql <<"SELECT shuttles.id,shuttles.parentId,shuttles.firstName,shuttles.lastName,shuttles.oX,shuttles.oY,shuttles.oZ,shuttles.oW,shuttles.x,shuttles.y,shuttles.z,"
		<< "shuttle_types.object_string,shuttle_types.name,shuttle_types.file,shuttles.awayTime,shuttles.inPortTime,shuttles.collectorId "
		<< "FROM " << mDatabase->galaxy() << ".shuttles INNER JOIN " << mDatabase->galaxy() << ".shuttle_types ON (shuttles.shuttle_type = shuttle_types.id) "
		<< "WHERE (shuttles.id = " << id << ")";

    mDatabase->executeSqlAsync(this,new(mQueryContainerPool.ordered_malloc()) QueryContainerBase(ofCallback,SHFQuery_MainData,client), sql.str());
                                                              
    
}

//=============================================================================

Shuttle* ShuttleFactory::_createShuttle(swganh::database::DatabaseResult* result)
{
    if (!result->getRowCount()) {
    	return nullptr;
    }

    Shuttle*	shuttle				= new Shuttle();
    Inventory*	shuttleInventory	= new Inventory();
    shuttleInventory->setParent(shuttle);

    result->getNextRow(mShuttleBinding,(void*)shuttle);

	gObjectManager->LoadSlotsForObject(shuttle);

	shuttle->SetBattleFatigue(0);

	for(int8 i = 0; i<9;i++)	{
		shuttle->InitStatBase(500);
		shuttle->InitStatCurrent(500);
		shuttle->InitStatMax(500);
		shuttle->InitStatWound(0);
	}
    
    // inventory
    shuttleInventory->setId(shuttle->mId + INVENTORY_OFFSET);
    shuttleInventory->setParentId(shuttle->mId);
    shuttleInventory->SetTemplate("object/tangible/inventory/shared_creature_inventory.iff");
    shuttleInventory->setName("inventory");
    shuttleInventory->setNameFile("item_n");
    shuttleInventory->setTangibleGroup(TanGroup_Inventory);
    shuttleInventory->setTangibleType(TanType_CreatureInventory);
    
	gObjectManager->LoadSlotsForObject(shuttleInventory);
	shuttle->InitializeObject(shuttleInventory);
    
	shuttle->setLoadState(LoadState_Loaded);

    shuttle->mPosture = 0;
    shuttle->mScale = 1.0;
    shuttle->setFaction("neutral");
    shuttle->mTypeOptions = 0x100;

    // Here we can handle the initializing of shuttle states

    // First, a dirty test for the shuttles in Theed Spaceport.
    // No need to randomize departure times, since we can always travel from there.
    // We wan't them to go in sync, so one of them always are in the spaceport.
    // if (shuttle->mParentId == 1692104)
#if defined(_MSC_VER)
    if (shuttle->mId == 47781511212)
#else
    if (shuttle->mId == 47781511212LLU)
#endif
    {
        shuttle->setShuttleState(ShuttleState_InPort);
        shuttle->setInPortTime(0);
    }
#if defined (_MSC_VER)
    else if (shuttle->mId == 47781511214)	// This is the "extra" shuttle.
#else
    else if (shuttle->mId == 47781511214LLU)	// This is the "extra" shuttle.
#endif
    {
        shuttle->setShuttleState(ShuttleState_Away);
        shuttle->setAwayTime(0);
    }
    else
    {
        // Get a randowm value in the range [0 <-> InPortInterval + AwayInterval] in ticks.
        // The rand value will land in either the InPort or in the Away part of the values.
        // Use that state as initial state and set the value as time that have already expired.
        uint32 maxInPortAndAwayIntervalTime = shuttle->getInPortInterval() + shuttle->getAwayInterval();
        uint32 shuttleTimeExpired = static_cast<uint32>(gRandom->getRand() / RAND_MAX) * (maxInPortAndAwayIntervalTime);

        if (shuttleTimeExpired <= shuttle->getInPortInterval())
        {
            // gLogger->log(LogManager::DEBUG,"Shuttle start InPort, time expired %u", shuttleTimeExpired);
            shuttle->setShuttleState(ShuttleState_InPort);
            shuttle->setInPortTime(shuttleTimeExpired);
        }
        else
        {
            // gLogger->log(LogManager::DEBUG,"Shuttle start Away, time expired %u", shuttleTimeExpired - shuttle->getInPortInterval());
            shuttle->setShuttleState(ShuttleState_Away);
            shuttle->setAwayTime(shuttleTimeExpired - shuttle->getInPortInterval()); // Set the part corresponding to this state only.
        }
    }
    return shuttle;
}

//=============================================================================

void ShuttleFactory::_setupDatabindings()
{
    mShuttleBinding = mDatabase->createDataBinding(17);
    mShuttleBinding->addField(swganh::database::DFT_uint64,offsetof(Shuttle,mId),8,0);
    mShuttleBinding->addField(swganh::database::DFT_uint64,offsetof(Shuttle,mParentId),8,1);
	mShuttleBinding->addField(swganh::database::DFT_stdstring,offsetof(Shuttle,first_name),64,2);
    mShuttleBinding->addField(swganh::database::DFT_stdstring,offsetof(Shuttle,last_name),64,3);
    mShuttleBinding->addField(swganh::database::DFT_float,offsetof(Shuttle,mDirection.x),4,4);
    mShuttleBinding->addField(swganh::database::DFT_float,offsetof(Shuttle,mDirection.y),4,5);
    mShuttleBinding->addField(swganh::database::DFT_float,offsetof(Shuttle,mDirection.z),4,6);
    mShuttleBinding->addField(swganh::database::DFT_float,offsetof(Shuttle,mDirection.w),4,7);
    mShuttleBinding->addField(swganh::database::DFT_float,offsetof(Shuttle,mPosition.x),4,8);
    mShuttleBinding->addField(swganh::database::DFT_float,offsetof(Shuttle,mPosition.y),4,9);
    mShuttleBinding->addField(swganh::database::DFT_float,offsetof(Shuttle,mPosition.z),4,10);
	mShuttleBinding->addField(swganh::database::DFT_stdstring,offsetof(Shuttle,template_string_),256,11);
    mShuttleBinding->addField(swganh::database::DFT_bstring,offsetof(Shuttle,mSpecies),64,12);
    mShuttleBinding->addField(swganh::database::DFT_bstring,offsetof(Shuttle,mSpeciesGroup),64,13);
    mShuttleBinding->addField(swganh::database::DFT_uint32,offsetof(Shuttle,mAwayInterval),4,14);
    mShuttleBinding->addField(swganh::database::DFT_uint32,offsetof(Shuttle,mInPortInterval),4,15);
    mShuttleBinding->addField(swganh::database::DFT_uint64,offsetof(Shuttle,mTicketCollectorId),8,16);
}

//=============================================================================

void ShuttleFactory::_destroyDatabindings()
{
    mDatabase->destroyDataBinding(mShuttleBinding);
}

//=============================================================================

