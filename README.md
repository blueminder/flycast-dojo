<p align="center"><img src="https://raw.githubusercontent.com/blueminder/flycast-dojo/master/FlycastDojoFoxlum.png" alt="Flycast Dojo Logo" width="243" height="283"><br /><i>Logo Credit: <a href="https://github.com/Foxlum">@Foxlum</a></i></p>
<h1 align="center">Flycast Dojo</h1>
 
**Flycast Dojo** is a fork of [**Flycast**](https://github.com/flyinghead/flycast), a multi-platform Sega Dreamcast, Naomi and Atomiswave emulator derived from [**Reicast**](https://reicast.com/), with a focus on netplay features and replay.

The latest releases of **Flycast Dojo** can be retrieved from GitHub, or as part of the **[Dojo Lobby](https://lobby.dojo.ooo/)** matchmaking service. However you obtain it, you will always have the option to use it standalone for P2P games using [relays](https://lobby.dojo.ooo/host-relays.html), through a direct IP connection, or a virtual LAN like ZeroTier or Radmin.

For quick start guides, frequently asked questions, online replays, and community links where you can find people to play against, head to the [Flycast Dojo](http://flycast.dojo.ooo/) site.

For general emulator help and updates on Flycast Dojo, check out the [Dojo Project Discord](https://discord.gg/JKBJhjea3J).

## Fightcade Support

For the latest versions compatible with [Fightcade](https://www.fightcade.com/), Flycast Dojo 6, refer to the `master` branch of this repository. The latest Fightcade-compatible release is [6.53](https://github.com/blueminder/flycast-dojo/releases/tag/dojo-6.53) while the one bundled with the software is [6.46](https://github.com/blueminder/flycast-dojo/releases/tag/dojo-6.46).

For Fightcade-specific help, be sure to read the pins and ask questions in the **#flycast-emulator** channel on the [Fightcade Discord](https://discord.gg/EyERRSg)

# Contents
- [Visual Aids](#visual-aids)
  * [Match Code Quick Start](#match-code-quick-start)
  * [Fightcade Quick Start](#fightcade-quick-start)
- [Manual Operation](#manual-operation)
    + [Set Server IP & Port](#set-server-ip--port)
    + [Delay Calculation](#manual-delay-calculation)
    + [Launch Game](#launch-game)
- [Savestates](#savestates)
    + [Custom Savestates](#custom-savestates)
- [Training Mode](#training-mode)
- [Command Line](#command-line)
  * [Netplay](#netplay)
  * [Spectating](#spectating)
  * [Test Game Screen](#test-game-screen)
- [Game-specific Guides](#game-specific-guides)
- [Video Demos](#video-demos)

# Visual Aids

## Match Code Quick Start
<img src="https://raw.githubusercontent.com/blueminder/flycast-dojo/master/match-code-qs-kofxi.gif" alt="Match Code Quick Start" width="945" height="387">

## Fightcade Quick Start
<a href="https://www.youtube.com/watch?v=47oJOUoGYmI"><img src="http://img.youtube.com/vi/47oJOUoGYmI/0.jpg" alt="Fightcade Quick Start" width="480" height="360"></a>

# Manual Operation

## Set Server IP & Port
If you are hosting, you can enter your server details manually in the "Netplay" settings. Once you have specified your session details, make sure that your opponent enters your IP address and port specified.

If you are a guest, make sure that "Act As Server" is not checked and that you have entered the matching IP address and port of your opponent in the "Server" column.

## Manual Delay Calculation
To calculate delay, we would use the following formula:

`Ceiling( Ping / 2 * FrameDuration (16 ms) ) = Delay #`

For instance, if my opponent’s average ping is 42 ms, I would divide it by 32 ms (2 * 16 ms) and round it up to 2.

```
= Ceiling( 42 ms / 2*16 ms )
= Ceiling( 42 ms / 32 ms )
= Ceiling( 1.3125 )
= 2
```

## Launch Game
On the Flycast main screen, you may now select your game of choice. You may also filter your list of games by typing in the text box on the top of the screen.

If you are hosting, you must start the game first, then have your opponent join afterward. If you are joining someone else's game, you must wait for them to start first. Be sure that you and your opponent have the same files before starting your session. These would include your ROM files, as well at your `eeprom.net`/`nvmem.net` files found in your `data/` subdirectory.

# Savestates
During a netplay match, if a custom savestate is present, you can "jump" to a starting savestate by pressing the **Backspace** button by default. This may be edited in the Control mapping section of the Settings.

## Custom Savestates
To use custom savestates, you must first disable **Ignore Netplay Savestates** in the **Memory Management** section of your Dojo settings.

<img alt="Memory Management - Disabled Ignore Netplay Savestates" src="https://raw.githubusercontent.com/blueminder/flycast-dojo/master/mm-disable-ignore-savestate.png" width="673" height="130">

From here, you must rename your desired game savestate to be `[ROM Name].state.net` and replace the existing file in your `data/` folder. Be sure you and your opponent have the same files when you start a session, or else you will be looking at different game states entirely.

# Training Mode

With the new Training Mode, you can enable game cheats while playing offline using RetroArch's `.cht` file format to adjust game parameters, quickly load states, save states, record, and play back user inputs for practice on any supported Dreamcast, NAOMI, or Atomiswave game. You can enter Training mode by selecting the **TRAIN** option in the drop-down menu on the top left of the Flycast main menu.

<img alt="Open Training Mode Menu" src="https://raw.githubusercontent.com/blueminder/flycast-dojo/master/training-menu.png" width="642" height="512">

To specify which player is being controlled and recorded, just press the **F7** key or press the **Menu** button (TAB by default) and click on "Watching Player". Player 1 corresponds with Port A, while Player 2 corresponds with Port B. To set playback loop settings, hit the **Menu** button and click on "Playback Loop Off" (or "On").

To record inputs, hit **F1**, and hit it again to stop recording. To play back the input, press **F4**. If the Playback Loop is enabled, the input will be repeated until the Play button is hit again. If it is disabled, playback will be invoked on demand. You also have two more slots to record on **F2** and **F3**, with playback on **F5**, and **F6**. To play a random input slot from the ones you have recorded, just hit the **F10** button.

To save your current game state, hit, **F8** and to quickly load it again, hit **F9**.

All of the Training Mode controls can be assigned to any keyboard key or gamepad input in the Controls settings.

Defaults:
* **F1-F3** Record Slots
* **F4-F6** Play Slots
* **F7** One-Button Player Change
* **F8** Quick Save
* **F9** Quick Load

<img alt="Training Mode Controller Mapping" src="https://raw.githubusercontent.com/blueminder/flycast-dojo/master/training-mode-map.png" width="642" height="512">

# Command Line
You may also call Flycast from the command line. All command line flags correspond with the options found in `emu.cfg`. Here are some example calls:

## Netplay
 * Server (Without Match Codes)

```flycast.exe -config network:GGPO=yes -config network:ActAsServer=yes -config network:server=127.0.0.1 -config network:GGPOPort=6000 -config network:GGPORemotePort=6001 -config dojo:PlayerName=Player1 ControllerTest-DJ.cdi```

*  Client (Without Match Codes)

```flycast.exe -config network:GGPO=yes -config network:ActAsServer=no -config network:server=127.0.0.1 -config network:GGPOPort=6001 -config network:GGPORemotePort=6000 -config dojo:PlayerName=Player2 ControllerTest-DJ.cdi```

 * Relay

Point both peers at a matchmaking relay. The first player to connect is the host; the guest connects to the same relay and waits for the host:

```flycast.exe -config network:GGPO=yes -config dojo:Relay=yes -config dojo:RelayServer=match.dojo.ooo -config dojo:RelayPort=8001 ControllerTest-DJ.cdi```

## Spectating
 * TCP Match Transmission (Spectating)

_append to server arguments_

```-config dojo:Transmitting=yes -config dojo:SpectatorIP=<IP> -config dojo:SpectatorPort=7000```

 * TCP Match Receiving (client)

```-config dojo:Receiving=yes -config network:ActAsServer=no```

 * TCP Match Receiving (server)

```-config dojo:Receiving=yes -config network:ActAsServer=yes -config dojo:SpectatorPort=7000```

## Test Game Screen

```-config dojo:TestGame=yes```

# Game-specific Guides
 * [The Rumble Fish 2 Wiki - Netplay](https://wiki.gbl.gg/w/The_Rumble_Fish_2/Netplay)
 * [Akatsuki Blitzkampf Wiki - Netplay](https://wiki.gbl.gg/w/Akatsuki_Blitzkampf/Netplay)

# Video Demos
## Flycast Netplay Testing - Capcom vs SNK 2 (NAOMI) VS
<a href="http://www.youtube.com/watch?v=zZoonpVJRjI"><img alt="Flycast Netplay Testing - Capcom vs SNK 2 (NAOMI) VS" src="http://img.youtube.com/vi/zZoonpVJRjI/0.jpg" width="480" height="360"></a>

## Flycast Netplay Testing - Akatsuki Blitzkampf Ausf Achse (NAOMI) VS
<a href="http://www.youtube.com/watch?v=s0MXenZPLiU"><img alt="Flycast Netplay Testing - Akatsuki Blitzkampf Ausf Achse (NAOMI) VS" src="http://img.youtube.com/vi/s0MXenZPLiU/0.jpg" width="480" height="360"></a>

## Flycast Dojo - Spectating / TCP Transmission - Test 1
<a href="https://www.youtube.com/watch?v=AmRf7MwOrQA"><img alt="Flycast Dojo - Spectating / TCP Transmission - Test 1" src="https://img.youtube.com/vi/AmRf7MwOrQA/0.jpg" width="480" height="360"></a>

## Flycast Dojo - The Rumble Fish 2 (Atomiswave) - LAN Lobby Spectating Test 1
<a href="https://www.youtube.com/watch?v=U9Nexso4lpo"><img alt="Flycast Dojo - The Rumble Fish 2 - LAN Lobby Spectating Test 1" src="https://img.youtube.com/vi/U9Nexso4lpo/0.jpg" width="480" height="360"></a>
