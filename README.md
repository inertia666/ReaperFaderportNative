# Reaper Faderport 16/8 Native

## _A native controller for the Presonus 16/8 in Reaper_

This is alpha software and has bugs/unimplemented features and limitations. While it is public, I am not really doing support/requests for this at the moment. However, I will try to take care of bugs and issues as I find the time.

This controller is built for my use only and optimised for how I want my controller to behave in Reaper.

This public code version is a stripped down version of my non-public code. This simplified public version allows you to control one instance of either the Faderport 16 or Faderport 8.

The code base is originally designed to control two Faderport units at the same and offer advance features specific to my workflow. Therefore, there is still some legacy code and design patterns to take into account two Faderports, which I will try and remove/optimise as I go along. If anyone else has two Faderports and is interested in my driver, then just get in touch.

Because I do not know of anyone using two Faderports or expect them to understand my view on how they should be controlled, I have released this simplified code.

If you want more flexibility and customisation, then I recommend using the _Control Surface Integrator_, by _Geoff Waddington_ and installing _navelpluisje's_ Faderport configuration files as a starting point. This is now a very mature CSI implementation of the Faderport and is looking very promising!

Because the _Control Surface Integrator_ is a stateless design and will only mirror Reaper's views, it does not really work exactly as I want it to. This is why I developed a native controller for my own use. However, since testing navelpluisje's implementation I will likely move my FP16 to the CSI and retain my FP8 as a stateless controller.

I do encourage the use of the _Control Surface Integrator_ because it is a very mature project with plenty of contributors and more people to help with your problems. It opens up a world of flexibility.
This driver will also work even with the CSI controller installed (as long as you are not using the CSI to control your Faderport).
 
## _Disclaimer_ ##

I am not a C++ developer by trade. I prefer easier languages like C#. I have found it challenging to structure the code as I would like. I also had to learn how to do all this on-the-fly using the csurf_mcu controller as a base and understand how Reaper works with extensions.
The extension may crash Reaper. Don't use it on production environments if you are worried!

## _What is implemented?_

- Volume faders
- Arm, solo clear, mute clear, select, solo, mute, pan, transport buttons, metronome
- Edit plugins (opens the plugin window only of the selected tracks)
- Automation buttons
- A way to implement busses and view them on the surface
- VCA Leader view
- VI view (filters tracks if there is a virtual instrument somewhere in the fx chain)
- Navigation with transport wheel (not working very well)
- Pin the master bus to the surface 
- Link - toggle follow Reaper on/off
- Enable bypass, macro, buttons to assign to custom actions
- Pan/Param wheel
- Shift + Save, Redo, Undo

## _What is not yet implemented?_
- Sends
- Zoom, Scroll, Section, Marker buttons. I haven't decided if I want to use them or how I want to use them
- Limited support for Nagivation Wheel. At the moment, it only works to control the Master buss volume
- RTZ << >> buttons. They go to the start/end of the project but are not useful right now.

## _What will never be implemented?_

- Editing FX parameters

## _General Behaviour_

The intention behind the contoller is to partially mimic using a regular mixing desk at the computer. There are some things implemented differently from other surface controllers:

 - The Faderport will always show a full bank of tracks when paging. For example, if you have 20 tracks in Reaper the Faderport 16 will display tracks 1-16. When you press "Next" to page by bank, 
 the Faderport will show tracks 4-20. This is to avoid having empty faders constantly flip up and down when paging and not waste space.

- _Bus view_ will not change the TCP or Mixer view in Reaper when showing busses. The driver will hold the state internally and create a virtual view to reflect on the Faderport. This means, if you press the _Bus_ button it will 
 display all your tracks that use the bus prefix tag in _config.txt_ file. 

- _VCA view_ will show only on the Faderport and will not show/hide non-VCA tracks on the TCP or Mixer view in Reaper. The driver will hold the state internally and create a virtual view to reflect on the Faderport.  

## _Button behaviour_

*Note* if a button doesn't light up, it is not implemented yet. It should, however, still trigger a MIDI event and can be assigned an action.

The following buttons are currently implemented with details on how they work:

| Button | Description |
| ------ | ------ |
| Arm | Arm tracks by pressing _ARM_ then _Select_ each track to be armed. Disengage _ARM_ when done |
| Solo | Press _S_ to solo each track respectively. Quickly double pressing _S_  will exclusively solo the selected track (all other soloed tracks are cleared) |
| Mute | Press _M_ to mute each track respectively. Quickly double pressing _M_  will exclusively mute the selected track (all other muted tracks are cleared) |
| Solo Clear | Clear soloed tracks |
| Mute Clear | Clear muted tracks |
| Select | Pressing _Select_ will select the track. To exclusively select a track, double press the _Select_ button. To clear all selected tracks, double press <br> to exclusively select then click one more time to unselect the track. A single volume fader will control all selected tracks
| Edit Plugins| This will open the FX window for any selected tracks. It does not provide plugin parameter editing |
| Pan | Ability to set the pan with a volume fader |
| Audio | Show prefixed "audio_prefix=A-" tracks set in the config file (see Bus) |
| VI | Show tracks that have virtual instruments) |
| Bus | By adding the line "bus_prefix=B-" you can mark track names with a prefix (in this case, "B-", and show these tracks on the Faderport. Replace "B-" with any characters of your choice. <br> |
| VCA | Show VCA leader tracks |
| All | Show all tracks not in the prefix filter list |
| Channel | _Prev/Next_ will move the surface display by -1/+1 channel at a time |
| Bank | _Prev/Next_ will move the surface display by 8 or 16 channels at a time |
| Prev | Move the surface display left by one channel or a bank of 8/16 depending on the _Channel_ or _Bank_ buttons status |
| Next | Move the surface display right by one channel or a bank of 8/16 depending on the _Channel_ or _Bank_ buttons status |
| Master | Will control the master buss volume with the large navigation wheel |
| Click | Toggle metronome on/off |
| Transport buttons| Repeat, Stop, Play/Pause, Record are all implemented|
| Automation buttons| All buttons implemented except _Off_|

## _Getting started_

The Faderport must be set to "Studio One" mode. Make sure the Faderport is off. Hold down the first two _select_ buttons and turn on the unit. Make sure "Studio One" is highlighed and press the _select_ button undernearth _EXIT_

Install the dll from the _FP16.zip_ in your Reaper UserPlugins directory. Place the _config.txt_ in the same directory.

Edit the _config.txt_ :

Specify if you have the Faderport 8 or 16 version.:
```sh
#Set to 16 or 8
faderport=16
```

Set the track to start from:
```sh
start_track=0
```

Prefix filter list:

_Audio/VI/Bus/VCA/All_ work as radio buttons. By creating a prefix in the config you can use these buttons to show/hide groups of tracks from the Faderport.

E.g If you want to use fake busses and have the bus button display them on Faderport then you can add a track title prefix here to identify busses. To activate them, make sure your track has the defined prefix in the start of its name.

```sh
bus_prefix=B- // Add B- in the start of any tracks that will be filtered by the bus button
audio_prefix=A- // Add A- in the start of any tracks that will be filtered by the audio button
vi_prefix=V- // Add V- in the start of any tracks that will be filtered by the VI button
```
For example, to create a bus track for Drums, rename the track to _B-DRUMS_. This track will now be removed from the main view and will be shown when pressing the _Bus_ button on the Faderport. You can make the prefix anything you like. The filter will check for a matching prefix in each track.

Since Reaper doesn't really have traditional busses, you can think of this as a kind of filter instead that is engaged when pressing _Bus_. 

***Note*** The All button doesn't show all the tracks. It shows everything not in the prefix list.

## _Non-functioning buttons_
All buttons should trigger a MIDI event, except for Shift. You can set up actions from the Reaper action list to trigger an action from any button on the Faderport.

| Buttons without action |
| ------ |
| Bypass | 
| Macro | 
| Link | 
| Track | 
| Sends | 
| Off | 
| Zoom | 
| Scroll | 
| Section | 
| Marker | 

## _Links_
- [Download] - 64bit Windows Build (at the moment this is buggy as cleaning up the code has caused breakage)
- [CSI] - Control Surface Integrator
- [Navelpluisje] - Navelpluisje's Faderport config for CSI

   [download]: <https://stash.reaper.fm/v/43911/Fp16.zip>
   [csi]: <https://github.com/GeoffAWaddington/reaper_csurf_integrator/wiki>
   [navelpluisje]: <https://github.com/navelpluisje/reasonus-faderport/releases/tag/v0.0.1-alpha-10>

## _Change_log_

+ audio/vi/bus/vca/all radio buttons with prefix for audio/vi/bus