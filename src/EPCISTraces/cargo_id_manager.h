#ifndef GARGOTIDMANAGER_H
#define GARGOTIDMANAGER_H

class CargoIdManager 
{
public:
    static CargoIdManager& Instance();
	long long getNextId();
private:
	long long currentId;
    CargoIdManager& operator= (const CargoIdManager&){}
    CargoIdManager (const CargoIdManager&){}

    static CargoIdManager m_instance;
    CargoIdManager();
    ~CargoIdManager();
};



#endif
