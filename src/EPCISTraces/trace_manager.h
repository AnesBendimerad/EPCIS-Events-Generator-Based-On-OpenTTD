#ifndef TRACE_MANAGER_H
#define TRACE_MANAGER_H
#define MAX_OUTPUT_EVENTS 50
#define DAY_IN_MS 86400000
#define HALF_DAY_IN_MS 43200000
#define INITIAL_INCREMENTATION_VALUE 1000
#define MAX_VALUE_FLUSH 1000
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include "../cargopacket.h"
#include "../station_base.h"
#include "../vehicle_base.h"
#include "../cargopacket.h"
#include <thread>
#include <mutex>



class TraceManager 
{
public:
    static TraceManager& Instance();
	void createMasterDataFile();
	void addStationToMasterDataFile(Station * station);
	void addIndustryToMasterDataFile(Industry * industry);
	void addTownToMasterDataFile(Town * town);
	void addEventGoodsStoring(std::vector<long long> & tracesIds,TileIndex sourceTile,CargoID cargo_type);
	void addEventPacketToVehicle(CargoPacket *cargoPacket,std::vector<long long> & tracesIds,Vehicle *v,Station *station);
	void addEventPacketToStation(CargoPacket *cargoPacket,std::vector<long long> & tracesIds,Station *station,Vehicle *v);
	void addEventGoodsCreation(std::vector<long long> & tracesIds,TileIndex sourceTile,CargoID cargo_type, bool storable);
	//void addEventGoodsTransformation(std::vector<long long> & inputTracesIds,std::vector<long long> & outputTracesIds,Industry *industry);
	void addEventGoodsTransformation(std::vector<long long>  inputTracesIds[],std::vector<long long>  outputTracesIds[],CargoID produced_cargo[],CargoID accepts_cargo[],Industry *industry);
	void addEventDeliveryGoodsToIndustry(std::vector<long long> & tracesIds,const Station *station,Industry *industry,CargoID cargo_type);
	void addEventTransportIndustryGoods(std::vector<long long> & tracesIds,TileIndex sourceTile,const Station *station,CargoID cargo_type);
	
	// ajouter l'evenement d'aggrégation et désaggrégation de cargopacket
	void reinitEpcisFile();
	void setIntroGame();
private:
	bool introGame;
	//std::vector<EpcisEvent*> outputEvents;
	std::string myTimeZone;
	std::string usedGCP;
	int nextTimeThreshold;
	int nextTimeDividor;
	int currentTime;
	int incrementationValue;
	YearMonthDay lastUsedDate;
	void fillCargoTypeTables(std::map<int,std::string> & cargoTypes);
	void incrementCurrentTime();
	std::string getStringOf(uint32 number,int size);
	void writeEvents();
    TraceManager& operator= (const TraceManager&){}
    TraceManager (const TraceManager&){}

    static TraceManager m_instance;
    TraceManager();
    ~TraceManager();
	void sendWrite(std::string *toWrite);

};
void writeInFile();


#endif 
