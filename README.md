# Lambda1VR

Lambda1VR is a port / mod of the Xash3D-FWG Half-Life Engine for the Oculus Quest.

The easiest way to install this on your Quest is using SideQuest, a PC desktop app designed to simplify sideloading apps and games ( even beat saber songs on quest ) on Standalone Android Headsets like Oculus Quest and Oculus Go. It supports drag and drop for installing APK files!

Download the latest version of SideQuest here:
https://github.com/the-expanse/SideQuest/releases

Once you have installed Lambda1VR you will need to copy the game assets from a legally owned half-life install. Details below.


## IMPORTANT NOTE

This is solely an engine port/mod, no game assets are included whatsoever. YOU *MUST* LEGALLY OWN HALF-LIFE IF YOU WISH TO PLAY IT IN VR USING LAMBDA1VR.

The original half-life can be purchased on Steam:  https://store.steampowered.com/app/70/HalfLife/

*PLEASE NOTE:* Half-life: Source is *not* compatible.


## Copying the Full Game files to your Oculus Quest

Before attempting to start Lambda1VR you need to do the following:

- Create a folder on your Quest called "xash"
- Locate the install of half-life on your PC, if installed from Steam then it will be somewhere like C:/Program Files (x86)/Steam/steamApps/common/HalfLife/
- Copy the entire folder "valve" to the newly created "xash" folder on the Quest (This will take a long time)
- (optional) Copy the contents of the valve_hd folder into the valve folder now on your Quest (this gives you some nicer models)



## Controls

Before you ask... YES!, the crowbar has to be swung in real life to smash stuff! (except in multi-player, you still have to press the trigger I'm afraid).

### Left-handed people
add the following cvar to your config.cfg file:  hand "1"


### Button / Controller Mappings

|Dominant Hand Controller||
| --- | --- |
|Orientation/Position|Weapon Orientation/Position|
|Trigger|Fire Primary / Secondary|
|Grip (Hold)|Enable secondary fire|
|Grip (Quick Click)|Reload|
|Click Thumbstick|Use (action)|

|Off-hand Controller||
| --- | --- |
|Orientation/Position|Flashlight Orientation/Position|
|Trigger|Hold down to run)|
|Grip (hold)|Weapon Stabilise (hold to remain active)*|
|Click Thumbstick|**reserved for future use**|

\* Weapon stabilisation has a deadzone when the two controllers are less than 15cm from each other to prevent screwing up aiming with the Glock etc

|Left Controller Specific||
| --- | --- |
|Thumbstick|Move/Strafe|
|Menu Button|Menu|
|X|Flashlight on/off|
|Y|Show multi-player score table (switches to screen layer view while Y is held down)|

|Right Controller Specific||
| --- | --- |
|Thumbstick Left/Right|Turn|
|Thumbstick Up/Down|Select Weapon (Fire to accept new weapon)|
|A|Crouch|
|B|Jump|


## Things to note / FAQs

* Your screen name (for multi-player) can be set in the config.cfg file
* FPS counter can be enabled in the Video Options menu
* Weapon recoil is disabled by default.. if you want a more authentic experience it can be enabled with the cvar: vr_weaponrecoil 1
* If the player height is messed up. Try reseting the view by holding the home button for 2 seconds. After that go into the main menu and back into the game.
* After changing files in xash/vale folder, often the device needs to be restarted for the game to use the changed files.

## Known Issues

Hopefully there will be fixes in time for most of the following, but rather than delay release any longer, none of could be considered show-stoppers:

* Application crashes when you complete the Hazard Course
* Application crashes if you select "Disconnect Server" in the menu
* RPG (and some other weapon) laser-targetting is based on head orientation
* No way to enter text at the moment
* Positional tracking doesn't really work on multi-player servers (unless you run your own server and set the following cvar:  sv_accelerate 10000), it sort of works if you hold down the "run" trigger, but it isn't very good
* Lambda1VR will crash if you haven't copied the half-life game assets to the right location, no warning, just crash


## Future To-Dos

* Text entry / Virtual Keyboard - Frustratingly the Quest virtual keyboard isn't available through the native SDK :(
* Add a flashlight model for the left hand when not stabilising a weapon
* Laser sight for easier aiming
* Better grenade throwing mechanics (pretty janky at the moment)

## Building

You need the following:

* Android Developer Studio
* Oculus Mobile SDK 1.23.0
* The Lambda1VR folder should be below VrSamples in the extracted SDK

Alternatively you can use the docker image created by BrainSlugs83 which can be found here: https://github.com/BrainSlugs83/DockerOvrSdk


## Credits

I would like to thank the following teams and individual for making this possible:

* Baggyg - My long-time VR friend whose roles in this have been varied and all helpful, also the creator of excellent websites/artwork/assets for this mod
* Xash3D team
* GLE4ES without which this wouldn't have worked at all: https://github.com/ptitSeb/gl4es
* Max Vollmer for the initial 6DoF weapons piece
* The SideQuest team - For making it easy for people to install this
* https://github.com/formicsapien for the weapon models that work much better with 6DoF


## Notice of Non-Affiliation and Disclaimers

Lambda1VR is not affiliated, associated, authorized, endorsed by, or in any way officially connected with Valve Corporation, or any of its subsidiaries or its affiliates.
Lambda1VR is an unofficial port of the Xash3D-FWGS engine, which was originally written by Uncle Mike as a fully compatible open-source GoldSrc engine alternative, more details on that extraordinary project can be found here: https://www.moddb.com/engines/xash3d-engine
However this port (Lambda1VR) is not affiliated, associated, authorized, endorsed by, or in any way officially connected with the Xash3D team.
This port was developed using the now deprecated Xash3D port found here: https://github.com/FWGS/xash3d
