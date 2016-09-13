This project is an EPCIS Events Generator based on the game OpenTTD version 1.5.0. It captures all the transportation and the manufacturing events that happen in the game and writes them into an output file using the EPCIS Format.

This project is free and open-source software licenced under the GNU General Public Licence 2.0.

For more details about OpenTTD : https://www.openttd.org/en/development

For more details about EPCIS : http://www.gs1.org/docs/epc/epcis_1_1-standard-20140520.pdf

A great number of transportation and manufacturing events happen in the game OpenTTD. For example, events that correspond to the creation of cargos (wood, mail, coal,...), the shipping of cargopackets using a vehicle, and the reception of cargos in a station. The current Project collects all these events and writes them into a file using the EPCIS standard.
For each game, two files are produced:
- **EPCISEvent File**: this file contains the events captured in the game.
- **MasterData File**: this file defines the bizLocations and the object classes. This elements are explained below.

A bizLocation is a number that identifies a unique place in the game. This place can be a station, a town, or an industry. For each bizLocation, MasterData File specifies the name of the location, the geographic coordinates (latitude and longitude), and the type (whether it's a station, a town, or an industry). In Listing 1, the identifier "urn :epc :id :sgln :01000.0020935.0" is a bizLocation.
An object class is a range of identifiers that correspond to objects of a specified type. In Listing 1, the object class "epc :id :sgtin :01000.0001.*" correspond to the type "coal". This means that all the identifiers that begin with "epc :id :sgtin :01000.0001." correspond to objects of type coal. An example of identifiers of objects of type coal is "epc :id :sgtin :01000.0001.2047122".

**Listing 1:**
```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<epcismd:EPCISMasterDataDocument
	xmlns:epcismd="urn:epcglobal:epcis-masterdata:xsd:1"
	schemaVersion="1"
	creationDate="2016-9-8T11:1:38 +00:00">
<EPCISBody>
<VocabularyList>
	<Vocabulary type="urn:epcglobal:epcis:vtype:EPCClass"> |\label{lst:line:vocListEPC}|
		<VocabularyElementList>
			<VocabularyElement id="epc:id:sgtin:01000.0001.*">
				<attribute id="urn:epcglobal:epcis:mda:name" value="coal"/>
			</VocabularyElement>
		</VocabularyElementList>
	</Vocabulary>
	<Vocabulary type="urn:epcglobal:epcis:vtype:BusinessLocation"> |\label{lst:line:sglnListEPC}|
		<VocabularyElementList>
			<VocabularyElement id="urn:epc:id:sgln:01000.0020935.0"> |\label{lst:line:station}|
				<attribute id="urn:epcglobal:epcis:mda:name" value="Atlanta Est"/>
				<attribute id="urn:epcglobal:fmcg:mda:latitude" value="81"/>
				<attribute id="urn:epcglobal:fmcg:mda:longitude" value="199"/>
				<attribute id="urn:epcglobal:epcis:mda:description" value="station"/>
				<attribute id="urn:epcglobal:epcis:mda:city" value="Atlanta"/>
			</VocabularyElement>
		</VocabularyElementList>
	</Vocabulary>
</VocabularyList>
</EPCISBody>
</epcismd:EPCISMasterDataDocument>
```

##Types of events captured in the game
Several types of events are captured in this project:
- **Creation**: it is an event of type "ObjectEvent". It is captured when objects are created. It can happen in an industry when creating objects that don't require other objects as an input. It can also happen in a town when creating passengers and mails. Listing 2 is an example of a creation event. The bizStep (Business Step) is "commissinning".

**Listing 2:**
```xml
<ObjectEvent>
	<eventTime>2005-3-18T10:30:0.0</eventTime>
	<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>
	<action>ADD</action>
	<epcList>
		<epc>epc:id:sgtin:01000.0000.67900</epc>
		<epc>epc:id:sgtin:01000.0000.67901</epc>
	</epcList>
	<bizStep>urn:epcglobal:cbv:bizstep:commissioning</bizStep>
	<disposition>urn:epcglobal:cbv:disp:active</disposition>
	<bizLocation>urn:epc:id:sgln:01000.0011454.0</bizLocation>
</ObjectEvent>
```
- **Transformation**: it is an event of type "Transformation event". It is captured when a set of objects is transformed to new objets. An example of this event is the transformation of "Iron Ore" to "Steel". Listing 3 is an example of a transformation event. The bizStep is "producing".

**Listing 3:**
```xml
<TransformationEvent>
	<eventTime>2005-3-19T12:2:29.0</eventTime>
	<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>
	<inputEPCList>		
		<epc>epc:id:sgtin:01000.0004.57737</epc>
	</inputEPCList>
	<outputEPCList>
		<epc>epc:id:sgtin:01000.0005.69071</epc>
	</outputEPCList>
	<bizStep>urn:epcglobal:cbv:bizstep:producing</bizStep>
	<disposition>urn:epcglobal:cbv:disp:produced</disposition>
	<bizLocation>urn:epc:id:sgln:01000.0037871.0</bizLocation>
</TransformationEvent>
```
- **Storing**: It is an event of type "Object Event". It is captured when objects are stored in the industry. Listing 4 is an example of a storing event. The bizStep is "storing".

**Listing 4:**
```xml
<ObjectEvent>
	<eventTime>2005-3-18T11:0:16.0</eventTime>
	<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>
	<action>OBSERVE</action>
	<epcList>
		<epc>epc:id:sgtin:01000.0008.67976</epc>
	</epcList>
	<bizStep>urn:epcglobal:cbv:bizstep:storing</bizStep>
	<bizLocation>urn:epc:id:sgln:01000.0050146.0</bizLocation>
</ObjectEvent>
```
- **Shipping**: It is an event of type "Object Event". It is captured when a set of objects is shipped to another place. Listing 5 is an example of shipping event. The bizStep is "shipping".

**Listing 5:**
```xml
<ObjectEvent>
	<eventTime>2005-3-18T11:20:20.0</eventTime>
	<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>
	<action>OBSERVE</action>
	<epcList>
		<epc>epc:id:sgtin:01000.0000.67995</epc>
	</epcList>
	<bizStep>urn:epcglobal:cbv:bizstep:shipping</bizStep>
	<disposition>urn:epcglobal:cbv:disp:in_transit</disposition>
	<bizLocation>urn:epc:id:sgln:01000.0020096.0</bizLocation>
	<sourceList><source>urn:epc:id:sgln:01000.0020096.0</source></sourceList>
	<destinationList><destination>urn:epc:id:sgln:01000.0020610.0</destination></destinationList>
</ObjectEvent>
```
- **Receiving**: It is an event of type "Object Event". It is captured when a set of transported objets reaches its destination. Listing 6 is an example of receiving event. The bizStep is "receiving"

**Listing 6:**
```xml
<ObjectEvent>
	<eventTime>2005-3-18T10:0:21.0</eventTime>
	<eventTimeZoneOffset>+00:00</eventTimeZoneOffset>
	<action>OBSERVE</action>
	<epcList>
		<epc>epc:id:sgtin:01000.0000.67995</epc>
	</epcList>
	<bizStep>urn:epcglobal:cbv:bizstep:receiving</bizStep>
	<disposition>urn:epcglobal:cbv:disp:active</disposition>
	<bizLocation>urn:epc:id:sgln:01000.0020610.0</bizLocation>
	<sourceList><source>urn:epc:id:sgln:01000.0020096.0</source></sourceList>
	<destinationList><destination>urn:epc:id:sgln:01000.0020610.0</destination></destinationList>
</ObjectEvent>
```

##Compilation and execution
We have already built this project in some Operating Systems. The folder "Releases" contains some executables that are ready to compile. This folder contains two repositories:
- **windows 32**: this folder contains an executable "openttd.exe" that can be used directly on Windows 7, Windows 8, Windows 10 (both 32 bits and 64 bits)
- **ubuntu 15.10**: this folder contains an executable "openttd" that can be used directly on Ubuntu.

The script "ubuntuCompileScript.sh" allows to compile this project on Ubuntu. Otherwise, this project can be compiled in the same manner than the standard version of the game OpenTTD. The instructions of compilation are explained in the following links:
- For [Unix-Like systems](https://wiki.openttd.org/Compiling_on_(GNU/)Linux_and_*BSD)
- For [Mac OS X](https://wiki.openttd.org/Compiling_on_Mac_OS_X)
- For [Windows](https://wiki.openttd.org/Compiling_on_Windows_using_Microsoft_Visual_C%2B%2B_2012)

##How to use this project
There are two ways to generate events using this project:
- Launch a game from the graphic interface: we can create a new game or laod a saved game. While the user is playing, all the events that happen are saved in the output file.
- Launch a game using the command line: We can also launch a saved game without graphic interface. So the user can't see what happen in the game. The advantage is the time speed of the game is so high that a great quantity of data can be generated in a few time. To launch a saved game without graphic interface, we must use this command line: 
```
name_of_executable -g path_of_saved_game -v null:ticks=number_of_iterations
```
The parameters are:
- name_of_executable: the name of the executable. For example, it is "openttd.exe" for windows
- path_of_saved_game: the file path of the game that we want to launch.
- number_of_iterations: the number of iterations the game will execute.

For example, the following command can be executed on Windows in the folder "Releases\windows32\OpenTTD Generator":
```
openttd .exe -g saves \ autosave15 . sav -v null : ticks =100000
```
