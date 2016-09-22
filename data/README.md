This repository contains examples of generated data using this project. The data in "Example 1" and "Example 2" are generated using the saved games "savedGame1.sav" and "savedGame2.sav" respectively. These saved games are located in the folder "releases/windows 32/OpenTTD Generator/saves/". For example, in order to generate the data of "Example 1" on Windows, we executed the command line below in the folder "releases/windows 32/OpenTTD Generator/"
```
openttd.exe  -g .\saves\savedGame1.sav -v null:ticks=1000
```