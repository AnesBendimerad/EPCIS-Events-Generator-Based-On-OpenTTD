#include "../stdafx.h"
#include "trace_manager.h"
#include "../strings_func.h"
#include "../date_func.h"
#include <ctime>
#include <algorithm>
#include "../industry.h"
#include "../script/api/script_map.hpp"
#include "../town.h"
#include "../settings_type.h"
#include "../base_station_base.h"
#include "../station_base.h"
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <iostream>
#include <fstream>
#include <string>

std::string tracesFileName="EventData.xml";
std::string masterDataFileName="masterData.xml";
bool firstOpeningEPCISFile=true;
bool firstOpeningMasterFile=true;
std::thread  * writer;
std::vector<std::string*> elementsToWrite;
bool writeTerminated;
std::mutex elementsToWriteMutex;
std::mutex writeTerminatedMutex;
std::string fileSeparator;
std::string tracesFolder="TracesFolder";
std::string currentTracesFolder="Traces of";

TraceManager TraceManager::m_instance=TraceManager();

TraceManager::TraceManager()
{
	introGame=false;
	nextTimeThreshold=HALF_DAY_IN_MS;
	nextTimeDividor=2;
	currentTime=0;
	incrementationValue=INITIAL_INCREMENTATION_VALUE;
	lastUsedDate.day=0;
	lastUsedDate.month=0;
	lastUsedDate.year=0;

	time_t ts = 0;
	time(&ts);
	myTimeZone="+00:00";
	usedGCP="01000";

	char buf2[64];
	::strftime(buf2, sizeof(buf2), "%c", localtime(&ts));
	std::string dateTime=buf2;
	std::replace( dateTime.begin(), dateTime.end(), '/', '-');
	std::replace( dateTime.begin(), dateTime.end(), ':', '-');
	currentTracesFolder="Traces of "+dateTime;
	//tracesFileName="tracesFile "+dateTime+".xml";
	//masterDataFileName="masterDataFile "+dateTime+".xml";

	/*std::ifstream epcisParameters ("epcisEventParameters.txt");
	std::string outputFolder=".";
	std::string line;
	if (epcisParameters.is_open())
	{
		while ( std::getline (epcisParameters,line,'\n') )
		{
			std::cout<<line<<std::endl;
			std::string paramName=line.substr(0,line.find("="));
			if (paramName.compare("outputFolder"))
			{
				outputFolder=line.substr(line.find("=")+1,line.size());
			}
		}
	}
	epcisParameters.close();
	*/
	writeTerminatedMutex.lock();
	writeTerminated=false;
	writeTerminatedMutex.unlock();
	writer=NULL;
#ifdef _WIN32
   //define something for Windows
	fileSeparator="\\";

#else
  //define it for a Unix machine
	fileSeparator="/";
#endif
	//tracesFolder=outputFolder+fileSeparator+tracesFolder;
	DIR *dir;
	if ((dir = opendir (tracesFolder.c_str())) != NULL) {
		closedir(dir);
	}
	else {
#ifdef _WIN32
		mkdir(tracesFolder.c_str());
#else
  //define it for a Unix machine
		mkdir(tracesFolder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
	}
}
std::string TraceManager::getStringOf(uint32 number,int size)
{
	std::string nAsString=std::to_string(number);
	while (nAsString.size()<size)
	{
		nAsString="0"+nAsString;
	}
	return nAsString;
}
void TraceManager::setIntroGame()
{
	introGame=true;
}
void TraceManager::incrementCurrentTime()
{
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	if (ymd.day==lastUsedDate.day && ymd.month==lastUsedDate.month && ymd.year==lastUsedDate.year)
	{
		currentTime+=incrementationValue;
		if (currentTime>=nextTimeThreshold)
		{
			if (currentTime>=DAY_IN_MS)
			{
				currentTime=DAY_IN_MS-1000;
			}
			incrementationValue/=2;
			if (incrementationValue==0)
			{
				incrementationValue=1;
			}
			nextTimeThreshold+=(HALF_DAY_IN_MS/nextTimeDividor);
			if (nextTimeThreshold>=DAY_IN_MS)
			{
				nextTimeThreshold=DAY_IN_MS;
			}
			nextTimeDividor*=2;
		}
	}
	else 
	{
		lastUsedDate.day=ymd.day;
		lastUsedDate.month=ymd.month;
		lastUsedDate.year=ymd.year;
		currentTime=0;
		nextTimeThreshold=HALF_DAY_IN_MS;
		nextTimeDividor=2;
		incrementationValue=INITIAL_INCREMENTATION_VALUE;
	}
}

void writeInFile()
{
	std::string * toWriteNow;
	std::ofstream outfile;
	int cpt=0;
	outfile.open(tracesFolder+fileSeparator+currentTracesFolder+fileSeparator+tracesFileName, std::ios_base::app);
	writeTerminatedMutex.lock();
	while (!writeTerminated)
	{
		writeTerminatedMutex.unlock();
		elementsToWriteMutex.lock();
		if (elementsToWrite.size()>0)
		{
			toWriteNow=elementsToWrite[0];
			elementsToWrite.erase(elementsToWrite.begin());
			elementsToWriteMutex.unlock();
			outfile<<*toWriteNow;
			delete toWriteNow;
			cpt++;
			if (cpt==MAX_VALUE_FLUSH){
				outfile.flush();
				cpt=0;
			}
		}
		else 
		{
			elementsToWriteMutex.unlock();
		}

		writeTerminatedMutex.lock();
	}
	writeTerminatedMutex.unlock();
	elementsToWriteMutex.lock();
	while (elementsToWrite.size()>0)
	{
		toWriteNow=elementsToWrite[0];
		elementsToWrite.erase(elementsToWrite.begin());
		elementsToWriteMutex.unlock();
		outfile<<*toWriteNow;
		delete toWriteNow;
		elementsToWriteMutex.lock();
	}
	elementsToWriteMutex.unlock();
	outfile.close();
}
void TraceManager::sendWrite(std::string * toWrite)
{
	writeEvents();
	std::string allString="";
	if (firstOpeningEPCISFile){
		// create folder here
		firstOpeningEPCISFile=false;
		DIR *dir;
		if ((dir = opendir ((tracesFolder+fileSeparator+currentTracesFolder).c_str())) != NULL) {
			closedir(dir);
		}
		else {
#ifdef _WIN32
			mkdir((tracesFolder+fileSeparator+currentTracesFolder).c_str());
#else
  //define it for a Unix machine
			mkdir((tracesFolder+fileSeparator+currentTracesFolder).c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

		}


		std::ofstream outfile;
		outfile.open(tracesFolder+fileSeparator+currentTracesFolder+fileSeparator+tracesFileName);
		outfile.close();
		allString+="<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
		allString+="<epcis:EPCISDocument\n";
		allString+="\txmlns:epcis=\"urn:epcglobal:epcis:xsd:1\"\n";
		allString+="\txmlns:example=\"http://ns.example.com/epcis\"\n";
		allString+="\txmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n";
		allString+="\tcreationDate=\"";
		time_t t = time(0);   
		struct tm * now = localtime( & t );
		allString+= std::to_string(now->tm_year + 1900) + '-' 
			+ std::to_string(now->tm_mon + 1) + '-'
			+  std::to_string(now->tm_mday)
			+ "T"
			+ std::to_string(now->tm_hour) +":"
			+  std::to_string(now->tm_min) +":"
			+ std::to_string(now->tm_sec) +" "+myTimeZone+ "\"\n";
		allString+="\tschemaVersion=\"1.1\">\n";
		allString+="<EPCISBody>\n";
		allString+="\t<EventList>\n";
	}
	*toWrite=allString+*toWrite;
	elementsToWriteMutex.lock();
	elementsToWrite.push_back(toWrite);
	elementsToWriteMutex.unlock();
	TraceManager * manager=this;
	if (writer==NULL)
	{
		writer=new std::thread( writeInFile);
	}
}
void TraceManager::reinitEpcisFile()
{
	if (firstOpeningMasterFile==false)
	{
		std::ofstream masterFile;
		masterFile.open(tracesFolder+fileSeparator+currentTracesFolder+fileSeparator+masterDataFileName, std::ios_base::app);
		masterFile<<"\t\t\t</VocabularyElementList>"<<std::endl;
		masterFile<<"\t\t</Vocabulary>"<<std::endl;
		masterFile<<"\t</VocabularyList>"<<std::endl;
		masterFile<<"</EPCISBody>"<<std::endl;
		masterFile<<"</epcismd:EPCISMasterDataDocument>"<<std::endl;
		masterFile.close();
	}
	if (firstOpeningEPCISFile==false)
	{
		writeEvents();
		std::string *allString=new std::string ("");
		//std::ofstream outfile;
		//outfile.open(tracesFileName, std::ios_base::app);
		*allString+="\t</EventList>\n";
		*allString+="</EPCISBody>\n";
		*allString+="</epcis:EPCISDocument>\n";
		sendWrite(allString);
		writeTerminatedMutex.lock();
		writeTerminated=true;
		writeTerminatedMutex.unlock();
		writer->join();
		delete writer;
		writer=NULL;
		writeTerminatedMutex.lock();
		writeTerminated=false;
		writeTerminatedMutex.unlock();
		//outfile<<allString;
		//outfile.close();
	}
	introGame=false;
	firstOpeningEPCISFile=true;
	firstOpeningMasterFile=true;
	time_t ts = 0;
	time(&ts);
	char buf2[64];
	::strftime(buf2, sizeof(buf2), "%c", localtime(&ts));
	std::string dateTime=buf2;
	std::replace( dateTime.begin(), dateTime.end(), '/', '-');
	std::replace( dateTime.begin(), dateTime.end(), ':', '-');
	tracesFileName="EventData.xml";
	masterDataFileName="masterDataFile.xml";
	currentTracesFolder="Traces of "+dateTime;
}


TraceManager::~TraceManager()
{

}

TraceManager& TraceManager::Instance()
{
	return m_instance;
}

void TraceManager::fillCargoTypeTables(std::map<int,std::string> & cargoTypes)
{
	cargoTypes[0]="passengers";
	cargoTypes[1]="coal";
	cargoTypes[2]="mail";
	cargoTypes[3]="oil";
	cargoTypes[4]="livestock";
	cargoTypes[5]="goods";
	cargoTypes[6]="grain";
	cargoTypes[7]="wood";
	cargoTypes[8]="iron_ore";
	cargoTypes[9]="steel";
	cargoTypes[10]="valuables";
	cargoTypes[11]="food";
	if (_settings_game.game_creation.landscape==LT_ARCTIC)
	{
		cargoTypes[6]="wheat";
		cargoTypes[8]="hillyUnused";
		cargoTypes[9]="paper";
		cargoTypes[10]="gold";
		cargoTypes[11]="food";
	}
	if (_settings_game.game_creation.landscape==LT_TROPIC)
	{
		cargoTypes[1]="rubber";
		cargoTypes[4]="fruit";
		cargoTypes[6]="maize";
		cargoTypes[8]="copper_or";
		cargoTypes[9]="water";
		cargoTypes[10]="diamonds";
	}
	if (_settings_game.game_creation.landscape==LT_TOYLAND)
	{
		cargoTypes[1]="sugar";
		cargoTypes[3]="toys";
		cargoTypes[4]="batteries";
		cargoTypes[5]="candy";
		cargoTypes[6]="toffee";
		cargoTypes[7]="cola";
		cargoTypes[8]="cotton_candy";
		cargoTypes[9]="bubbles";
		cargoTypes[10]="plastic";
		cargoTypes[11]="fizzy_drinks";
	}
}


void TraceManager::createMasterDataFile()
{
	if (introGame)
	{
		return;
	}
	// entete
	// create folder here
	DIR *dir;
	if ((dir = opendir ((tracesFolder+fileSeparator+currentTracesFolder).c_str())) != NULL) {
		closedir(dir);
	}
	else {
#ifdef _WIN32
		mkdir((tracesFolder+fileSeparator+currentTracesFolder).c_str());
#else
  //define it for a Unix machine
		mkdir((tracesFolder+fileSeparator+currentTracesFolder).c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
	}

	std::ofstream masterFile;
	masterFile.open(tracesFolder+fileSeparator+currentTracesFolder+fileSeparator+masterDataFileName);
	masterFile<<"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"<<std::endl;
	masterFile<<"<epcismd:EPCISMasterDataDocument"<<std::endl;
	masterFile<<"\txmlns:epcismd=\"urn:epcglobal:epcis-masterdata:xsd:1\""<<std::endl;
	masterFile<<"\tschemaVersion=\"1\""<<std::endl;
	masterFile<<"\tcreationDate=\"";
	time_t t = time(0);   
	struct tm * now = localtime( & t );
	masterFile << (now->tm_year + 1900) << '-' 
		<< (now->tm_mon + 1) << '-'
		<<  now->tm_mday
		<< "T"
		<< now->tm_hour <<":"
		<<  now->tm_min <<":"
		<< now->tm_sec <<" "<<myTimeZone<< "\">"
		<< std::endl;
	masterFile<<"<EPCISBody>"<<std::endl;
	masterFile<<"\t<VocabularyList>"<<std::endl;
	masterFile<<"\t\t<Vocabulary type=\"urn:epcglobal:epcis:vtype:EPCClass\">"<<std::endl;
	masterFile<<"\t\t\t<VocabularyElementList>"<<std::endl;
	std::map<int,std::string> cargoTypes;
	fillCargoTypeTables(cargoTypes);
	int cpt;
	for (cpt=0;cpt<12;cpt++)
	{
		masterFile<<"\t\t\t\t<VocabularyElement id=\"";
		std::string epcClass="epc:id:sgtin:"+usedGCP+"."+getStringOf(cpt,4)+".*\"";
		masterFile<<epcClass<<">"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\""<<cargoTypes[cpt]<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	}
	masterFile<<"\t\t\t\t<VocabularyElement id=\"";
	std::string epcClass="epc:id:sgtin:"+usedGCP+".255.*\"";
	masterFile<<epcClass<<">"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\"invalid\"/>"<<std::endl;
	masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	masterFile<<"\t\t\t</VocabularyElementList>"<<std::endl;
	masterFile<<"\t\t</Vocabulary>"<<std::endl;
	masterFile<<"\t\t<Vocabulary type=\"urn:epcglobal:epcis:vtype:BusinessLocation\">"<<std::endl;
	masterFile<<"\t\t\t<VocabularyElementList>"<<std::endl;
		
	// parcourir les stations et les écrire
	Station *station;
	FOR_ALL_STATIONS(station)
	{
		masterFile<<"\t\t\t\t<VocabularyElement id=\"";
		std::string bizLocation="urn:epc:id:sgln:"+usedGCP+"."+getStringOf(station->xy,7)+".0";
		masterFile<<bizLocation<<"\">"<<std::endl;
		char buf[64];
		::SetDParam(0, station->index);
		GetString(buf,STR_STATION_NAME,lastof(buf));
		std::string	stationName=buf;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\""<<stationName<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:latitude\" value=\""<<std::to_string(ScriptMap::GetTileY(station->xy))<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:longitude\" value=\""<<std::to_string(ScriptMap::GetTileX(station->xy))<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:description\" value=\"station\"/>"<<std::endl;
		Town * town=station->town;
		::SetDParam(0, town->index);
		GetString(buf,STR_TOWN_NAME,lastof(buf));
		std::string	townName=buf;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:city\" value=\""<<townName<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	}
	// parcourir les industry et les écrire
	Industry *industry;
	FOR_ALL_INDUSTRIES(industry)
	{
		masterFile<<"\t\t\t\t<VocabularyElement id=\"";
		std::string bizLocation="urn:epc:id:sgln:"+usedGCP+"."+getStringOf(industry->location.tile,7)+".0";
		masterFile<<bizLocation<<"\">"<<std::endl;
		char buf[64];
		::SetDParam(0, industry->index);
		GetString(buf,STR_INDUSTRY_NAME,lastof(buf));
		std::string	industryName=buf;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\""<<industryName<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:latitude\" value=\""<<std::to_string(ScriptMap::GetTileY(industry->location.tile))<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:longitude\" value=\""<<std::to_string(ScriptMap::GetTileX(industry->location.tile))<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:description\" value=\"industry\"/>"<<std::endl;
		Town * town=industry->town;
		::SetDParam(0, town->index);
		GetString(buf,STR_TOWN_NAME,lastof(buf));
		std::string	townName=buf;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:city\" value=\""<<townName<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	}
	// parcourir les town et les écrire
	Town *curTown;
	FOR_ALL_TOWNS(curTown){
		masterFile<<"\t\t\t\t<VocabularyElement id=\"";
		std::string bizLocation="urn:epc:id:sgln:"+usedGCP+"."+getStringOf(curTown->xy,7)+".0";
		masterFile<<bizLocation<<"\">"<<std::endl;
		char buf[64];
		::SetDParam(0, curTown->index);
		GetString(buf,STR_TOWN_NAME,lastof(buf));
		std::string	townName=buf;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\""<<townName<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:latitude\" value=\""<<std::to_string(ScriptMap::GetTileY(curTown->xy))<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:longitude\" value=\""<<std::to_string(ScriptMap::GetTileX(curTown->xy))<<"\"/>"<<std::endl;
		masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:description\" value=\"Town\"/>"<<std::endl;
		masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	}
	masterFile.close();
}
void TraceManager::addTownToMasterDataFile(Town * town)
{
	if (introGame)
	{
		return;
	}	
	if (town==NULL)
	{
		return;
	}
	if (firstOpeningMasterFile)
	{
		createMasterDataFile();
		firstOpeningMasterFile=false;
		return;
	}
	std::ofstream masterFile;
	masterFile.open(tracesFolder+fileSeparator+currentTracesFolder+fileSeparator+masterDataFileName, std::ios_base::app);
	masterFile<<"\t\t\t\t<VocabularyElement id=\"";
	std::string bizLocation="urn:epc:id:sgln:"+usedGCP+"."+getStringOf(town->xy,7)+".0";
	masterFile<<bizLocation<<"\">"<<std::endl;
	char buf[64];
	::SetDParam(0, town->index);
	GetString(buf,STR_TOWN_NAME,lastof(buf));
	std::string	townName=buf;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\""<<townName<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:latitude\" value=\""<<std::to_string(ScriptMap::GetTileY(town->xy))<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:longitude\" value=\""<<std::to_string(ScriptMap::GetTileX(town->xy))<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:description\" value=\"Town\"/>"<<std::endl;
	masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	masterFile.close();
}
void TraceManager::addIndustryToMasterDataFile(Industry * industry)
{
	if (introGame)
	{
		return;
	}
	if (industry==NULL)
	{
		return;
	}
	if (firstOpeningMasterFile)
	{
		createMasterDataFile();
		firstOpeningMasterFile=false;
		return;
	}
	std::ofstream masterFile;
	masterFile.open(tracesFolder+fileSeparator+currentTracesFolder+fileSeparator+masterDataFileName, std::ios_base::app);
	masterFile<<"\t\t\t\t<VocabularyElement id=\"";
	std::string bizLocation="urn:epc:id:sgln:"+usedGCP+"."+getStringOf(industry->location.tile,7)+".0";
	masterFile<<bizLocation<<"\">"<<std::endl;
	char buf[64];
	::SetDParam(0, industry->index);
	GetString(buf,STR_INDUSTRY_NAME,lastof(buf));
	std::string	industryName=buf;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\""<<industryName<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:latitude\" value=\""<<std::to_string(ScriptMap::GetTileY(industry->location.tile))<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:longitude\" value=\""<<std::to_string(ScriptMap::GetTileX(industry->location.tile))<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:description\" value=\"industry\"/>"<<std::endl;
	Town * town=industry->town;
	::SetDParam(0, town->index);
	GetString(buf,STR_TOWN_NAME,lastof(buf));
	std::string	townName=buf;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:city\" value=\""<<townName<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	masterFile.close();
}
void TraceManager::addStationToMasterDataFile(Station * station)
{
	if (introGame)
	{
		return;
	}
	if (station==NULL)
	{
		return;
	}
	if (firstOpeningMasterFile)
	{
		createMasterDataFile();
		firstOpeningMasterFile=false;
		return;
	}
	std::ofstream masterFile;
	masterFile.open(tracesFolder+fileSeparator+currentTracesFolder+fileSeparator+masterDataFileName, std::ios_base::app);
	masterFile<<"\t\t\t\t<VocabularyElement id=\"";
	std::string bizLocation="urn:epc:id:sgln:"+usedGCP+"."+getStringOf(station->xy,7)+".0";
	masterFile<<bizLocation<<"\">"<<std::endl;
	char buf[64];
	::SetDParam(0, station->index);
	GetString(buf,STR_STATION_NAME,lastof(buf));
	std::string	stationName=buf;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:name\" value=\""<<stationName<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:latitude\" value=\""<<std::to_string(ScriptMap::GetTileY(station->xy))<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:fmcg:mda:longitude\" value=\""<<std::to_string(ScriptMap::GetTileX(station->xy))<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:description\" value=\"station\"/>"<<std::endl;
	Town * town=station->town;
	::SetDParam(0, town->index);
	GetString(buf,STR_TOWN_NAME,lastof(buf));
	std::string	townName=buf;
	masterFile<<"\t\t\t\t\t<attribute id=\"urn:epcglobal:epcis:mda:city\" value=\""<<townName<<"\"/>"<<std::endl;
	masterFile<<"\t\t\t\t</VocabularyElement>"<<std::endl;
	masterFile.close();	
}

void TraceManager::writeEvents()
{
	if (introGame)
	{
		return;
	}
	std::string allString="";
	if (firstOpeningMasterFile)
	{
		createMasterDataFile();
		firstOpeningMasterFile=false;
	}
	
}


// les appels vers sont ok
void TraceManager::addEventPacketToVehicle(CargoPacket *cargoPacket,std::vector<long long> & tracesIds,Vehicle *v,Station *station)
{
	if (introGame)
	{
		return;
	}
	if (tracesIds.size()==0)
	{
		return;
	}
	incrementCurrentTime();
	std::string* eventAsString=new std::string("\t\t<ObjectEvent>\n\t\t\t<eventTime>");
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	int eventTime=currentTime;
	int hours=eventTime/3600000;
	int rest1=eventTime%3600000;
	int minutes=rest1/60000;
	int rest2=rest1%60000;
	int seconds=rest2/1000;
	int milliseconds=rest2%1000;
	uint32 cType=v->cargo_type;
	std::string cTypeAsString=getStringOf(cType,4);
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>OBSERVE</action>\n"+"\t\t\t<epcList>\n";
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:shipping</bizStep>\n";
	*eventAsString+="\t\t\t<disposition>urn:epcglobal:cbv:disp:in_transit</disposition>\n";


	uint32 tileIndex=station->xy;
	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t\t<sourceList><source>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</source></sourceList>\n";
	StationID stationId=v->GetNextStoppingStation().value;
	if (stationId!=65535){
		Station *dest=Station::Get(stationId);
		uint32 nextTile=dest->xy;
		*eventAsString+="\t\t\t<destinationList><destination>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(nextTile,7)+".0</destination></destinationList>\n";
	}
	*eventAsString+="\t\t</ObjectEvent>\n";
	sendWrite(eventAsString);
}

// les appels vers sont ok
void TraceManager::addEventPacketToStation(CargoPacket *cargoPacket,std::vector<long long> & tracesIds,Station *station,Vehicle *v)
{
	if (introGame)
	{
		return;
	}
	if (tracesIds.size()==0)
	{
		return;
	}
	incrementCurrentTime();
	std::string* eventAsString=new std::string("\t\t<ObjectEvent>\n\t\t\t<eventTime>");
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	int eventTime=currentTime;
	int hours=eventTime/3600000;
	int rest1=eventTime%3600000;
	int minutes=rest1/60000;
	int rest2=rest1%60000;
	int seconds=rest2/1000;
	int milliseconds=rest2%1000;
	uint32 cType=v->cargo_type;
	std::string cTypeAsString=getStringOf(cType,4);
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>OBSERVE</action>\n"+"\t\t\t<epcList>\n";
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:receiving</bizStep>\n";
	*eventAsString+="\t\t\t<disposition>urn:epcglobal:cbv:disp:active</disposition>\n";
	uint32 tileIndex=station->xy;
	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</bizLocation>\n";
	StationID stationId=v->last_loading_station;
	//StationID stationId2=v->last_station_visited;
	if (stationId!=65535){
		Station *prevStation=Station::Get(stationId);
		//Station *prevStation2=Station::Get(stationId2);
		uint32 previousTile=prevStation->xy;
		//uint32 previousTile2=prevStation2->xy;
		*eventAsString+="\t\t\t<sourceList><source>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(previousTile,7)+".0</source></sourceList>\n";
	}
	*eventAsString+="\t\t\t<destinationList><destination>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</destination></destinationList>\n";
	*eventAsString+="\t\t</ObjectEvent>\n";
	sendWrite(eventAsString);
}
void TraceManager::addEventGoodsStoring(std::vector<long long> & tracesIds,TileIndex sourceTile,CargoID cargo_type)
{
	if (introGame)
	{
		return;
	}
	if (tracesIds.size()==0)
	{
		return;
	}
	incrementCurrentTime();
	std::string* eventAsString=new std::string("\t\t<ObjectEvent>\n\t\t\t<eventTime>");
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	int eventTime=currentTime;
	int hours=eventTime/3600000;
	int rest1=eventTime%3600000;
	int minutes=rest1/60000;
	int rest2=rest1%60000;
	int seconds=rest2/1000;
	int milliseconds=rest2%1000;
	uint32 cType=cargo_type;
	std::string cTypeAsString=getStringOf(cType,4);
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>OBSERVE</action>\n"+"\t\t\t<epcList>\n";
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:storing</bizStep>\n";

	uint32 tileIndex=sourceTile;	
	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t</ObjectEvent>\n";
	sendWrite(eventAsString);
}
void TraceManager::addEventGoodsCreation(std::vector<long long> & tracesIds,TileIndex sourceTile,CargoID cargo_type, bool storable)
{
	if (introGame)
	{
		return;
	}
	if (tracesIds.size()==0)
	{
		return;
	}
	incrementCurrentTime();
	std::string* eventAsString=new std::string("\t\t<ObjectEvent>\n\t\t\t<eventTime>");
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	int eventTime=currentTime;
	int hours=eventTime/3600000;
	int rest1=eventTime%3600000;
	int minutes=rest1/60000;
	int rest2=rest1%60000;
	int seconds=rest2/1000;
	int milliseconds=rest2%1000;
	uint32 cType=cargo_type;
	std::string cTypeAsString=getStringOf(cType,4);
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>ADD</action>\n"+"\t\t\t<epcList>\n";
	
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:commissioning</bizStep>\n";
	*eventAsString+="\t\t\t<disposition>urn:epcglobal:cbv:disp:active</disposition>\n";
	uint32 tileIndex=sourceTile;
	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t</ObjectEvent>\n";
	sendWrite(eventAsString);
	if (storable)
	{
		addEventGoodsStoring(tracesIds,sourceTile,cargo_type);
	}
}

void TraceManager::addEventGoodsTransformation(std::vector<long long>  inputTracesIds[],std::vector<long long>  outputTracesIds[],CargoID produced_cargo[],CargoID accepts_cargo[],Industry *industry)
{
	if (introGame)
	{
		return;
	}
	if (inputTracesIds[0].size()==0 && inputTracesIds[1].size()==0 && inputTracesIds[2].size()==0 )
	{
		return;
	}
	if (outputTracesIds[0].size()==0 && outputTracesIds[1].size()==0)
	{
		return;
	}
	incrementCurrentTime();
	std::string* eventAsString=new std::string("\t\t<TransformationEvent>\n\t\t\t<eventTime>");
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	int eventTime=currentTime;
	int hours=eventTime/3600000;
	int rest1=eventTime%3600000;
	int minutes=rest1/60000;
	int rest2=rest1%60000;
	int seconds=rest2/1000;
	int milliseconds=rest2%1000;
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<inputEPCList>\n";
	
	int cpt;
	for (cpt=0;cpt<3;cpt++)
	{
		uint32 cType=accepts_cargo[cpt];
		std::string cTypeAsString=getStringOf(cType,4);
		for (std::vector<long long>::iterator it = inputTracesIds[cpt].begin() ; it != inputTracesIds[cpt].end(); ++it)
		{
			*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		}
	}
	*eventAsString+="\t\t\t</inputEPCList>\n\t\t\t<outputEPCList>\n";
	for (cpt=0;cpt<2;cpt++)
	{
		uint32 cType=produced_cargo[cpt];
		std::string cTypeAsString=getStringOf(cType,4);
		for (std::vector<long long>::iterator it = outputTracesIds[cpt].begin() ; it != outputTracesIds[cpt].end(); ++it)
		{
			*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		}
	}
	uint32 tileIndex=industry->location.tile;
	*eventAsString+="\t\t\t</outputEPCList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:producing</bizStep>\n\t\t\t<disposition>urn:epcglobal:cbv:disp:produced</disposition>\n\t\t\t<bizLocation>urn:epc:id:sgln:"
		+usedGCP+"."+getStringOf(tileIndex,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t</TransformationEvent>\n";
	sendWrite(eventAsString);
	for (cpt=0;cpt<2;cpt++){
		addEventGoodsStoring(outputTracesIds[cpt],industry->location.tile,produced_cargo[cpt]);
	}
}
void TraceManager::addEventDeliveryGoodsToIndustry(std::vector<long long> & tracesIds,const Station *station,Industry *industry,CargoID cargo_type)
{
	if (introGame)
	{
		return;
	}
	if (tracesIds.size()==0)
	{
		return;
	}
	// sending from station :
	incrementCurrentTime();
	std::string* eventAsString=new std::string("\t\t<ObjectEvent>\n\t\t\t<eventTime>");
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	int eventTime=currentTime;
	int hours=eventTime/3600000;
	int rest1=eventTime%3600000;
	int minutes=rest1/60000;
	int rest2=rest1%60000;
	int seconds=rest2/1000;
	int milliseconds=rest2%1000;
	uint32 cType=cargo_type;
	std::string cTypeAsString=getStringOf(cType,4);
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>OBSERVE</action>\n"+"\t\t\t<epcList>\n";
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:shipping</bizStep>\n";
	*eventAsString+="\t\t\t<disposition>urn:epcglobal:cbv:disp:in_transit</disposition>\n";
	uint32 tileIndex=station->xy;
	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t\t<sourceList><source>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</source></sourceList>\n";
	uint32 tileIndex2=industry->location.tile;
	*eventAsString+="\t\t\t<destinationList><destination>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex2,7)+".0</destination></destinationList>\n";
	*eventAsString+="\t\t</ObjectEvent>\n";
	

	// receiving in industry
	incrementCurrentTime();
	*eventAsString+="\t\t<ObjectEvent>\n\t\t\t<eventTime>";
	eventTime=currentTime;
	hours=eventTime/3600000;
	rest1=eventTime%3600000;
	minutes=rest1/60000;
	rest2=rest1%60000;
	seconds=rest2/1000;
	milliseconds=rest2%1000;
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>OBSERVE</action>\n"+"\t\t\t<epcList>\n";
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:receiving</bizStep>\n";
	*eventAsString+="\t\t\t<disposition>urn:epcglobal:cbv:disp:active</disposition>\n";

	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex2,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t\t<sourceList><source>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</source></sourceList>\n";
	*eventAsString+="\t\t\t<destinationList><destination>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex2,7)+".0</destination></destinationList>\n";
	*eventAsString+="\t\t</ObjectEvent>\n";
	sendWrite(eventAsString);
	addEventGoodsStoring(tracesIds,industry->location.tile,cargo_type);
}
void TraceManager::addEventTransportIndustryGoods(std::vector<long long> & tracesIds,TileIndex sourceTile,const Station *station,CargoID cargo_type)
{
	if (introGame)
	{
		return;
	}
	if (tracesIds.size()==0)
	{
		return;
	}
	// sending from industry
	incrementCurrentTime();
	std::string* eventAsString=new std::string("\t\t<ObjectEvent>\n\t\t\t<eventTime>");
	::YearMonthDay ymd;
	::ConvertDateToYMD(_date, &ymd);
	int eventTime=currentTime;
	int hours=eventTime/3600000;
	int rest1=eventTime%3600000;
	int minutes=rest1/60000;
	int rest2=rest1%60000;
	int seconds=rest2/1000;
	int milliseconds=rest2%1000;
	uint32 cType=cargo_type;
	std::string cTypeAsString=getStringOf(cType,4);
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>OBSERVE</action>\n"+"\t\t\t<epcList>\n";
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:shipping</bizStep>\n";
	*eventAsString+="\t\t\t<disposition>urn:epcglobal:cbv:disp:in_transit</disposition>\n";


	uint32 tileIndex=station->xy;
	uint32 tileIndex2=sourceTile;

	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex2,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t\t<sourceList><source>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex2,7)+".0</source></sourceList>\n";
	*eventAsString+="\t\t\t<destinationList><destination>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</destination></destinationList>\n";
	*eventAsString+="\t\t</ObjectEvent>\n";
	

	// receiving in station
	incrementCurrentTime();
	*eventAsString+="\t\t<ObjectEvent>\n\t\t\t<eventTime>";
	eventTime=currentTime;
	hours=eventTime/3600000;
	rest1=eventTime%3600000;
	minutes=rest1/60000;
	rest2=rest1%60000;
	seconds=rest2/1000;
	milliseconds=rest2%1000;
	*eventAsString+=std::to_string(ymd.year)+ "-"
		+std::to_string(ymd.month)+'-'+std::to_string(ymd.day)+"T"+std::to_string(hours)+":"+std::to_string(minutes)+":"+std::to_string(seconds)+"."+std::to_string(milliseconds)+"</eventTime>\n"
		+"\t\t\t<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>\n"
		+"\t\t\t<action>OBSERVE</action>\n"+"\t\t\t<epcList>\n";
	for (std::vector<long long>::iterator it = tracesIds.begin() ; it != tracesIds.end(); ++it)
	{
		*eventAsString+="\t\t\t\t<epc>epc:id:sgtin:"+usedGCP+"."+cTypeAsString+"."+std::to_string(*it)+"</epc>\n";
		
	}
	*eventAsString+="\t\t\t</epcList>\n\t\t\t<bizStep>urn:epcglobal:cbv:bizstep:receiving</bizStep>\n";
	*eventAsString+="\t\t\t<disposition>urn:epcglobal:cbv:disp:active</disposition>\n";
	*eventAsString+="\t\t\t<bizLocation>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</bizLocation>\n";
	*eventAsString+="\t\t\t<sourceList><source>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex2,7)+".0</source></sourceList>\n";
	*eventAsString+="\t\t\t<destinationList><destination>urn:epc:id:sgln:"+usedGCP+"."+getStringOf(tileIndex,7)+".0</destination></destinationList>\n";
	*eventAsString+="\t\t</ObjectEvent>\n";
	sendWrite(eventAsString);
}