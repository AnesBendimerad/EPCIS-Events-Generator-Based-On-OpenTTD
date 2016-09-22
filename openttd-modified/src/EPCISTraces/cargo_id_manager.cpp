#include "cargo_id_manager.h"

CargoIdManager CargoIdManager::m_instance=CargoIdManager();

CargoIdManager::CargoIdManager()
{
    currentId=0;
}

long long CargoIdManager::getNextId()
{
	currentId++;
	return currentId;
}

CargoIdManager::~CargoIdManager()
{
    
}

CargoIdManager& CargoIdManager::Instance()
{
    return m_instance;
}