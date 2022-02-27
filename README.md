# Reaper Faderport 16/8 Native

## _A native controller for the Presonus 16/8 in Reaper_

This is alpha software and has bugs/unimplemented features and limitations. While it is public, I am not really doing support/requests for this at the moment.

This controller is built for my use only and optimised for how I want my controller to behave in Reaper.

This public code version is a stripped down version of my non-public code. This simplified public version allows you to control one instance of either the Faderport 16 or Faderport 8.

The code base is originally designed to control two Faderport units at the same and offer advance features to my specific workflow. I can use both units as large virtual surface or I can split the units to control/view different aspects of Reaper with the press of a button.

If you want a more customisable way to control a Faderport surface then I recommend using the Control Surface Integrator, by Geoff Waddington and installing Airon's Faderport configuration files as a starting point.

Because the Control Surface Integrator is a completely stateless design, it does not really work exactly as I want it to. This is why I developed a native controller for my own use.

## _What is implemented?_

- Volume faders
- Arm, solo clear, mute clear, select, solo, mute, pan, transport buttons, metronome
- Edit plugins (opens the plugin window only of the selected tracks)
- Automation buttons
- A way to implement busses and view them on the surface
- Navigation with transport wheel (not working very well)
- Pin the master bus to the surface 
- Enable bypass, macro, link, VI, Audio buttons to assign to custom actions
- Pan/Param wheel

## _What is not yet implemented?_
- Sends
- VCA leader view

## _What will never be implemented?_

- Editing FX parameters

## _General Behaviour_

The intention behind the contoller is to partially mimic using a regular mixing desk at the computer. There are some things implemented differently from other surface controllers:

 - The Faderport will always show a full bank of tracks when paging. For example, if you have 20 tracks in Reaper the Faderport 16 will display tracks 1-16. When you press "Next" to page by channel or bank, 
 the Faderport will show tracks 4-20. This is to avoid having empty faders constantly flip up and down when paging.

- _Bus view_ will not change the TCP or Mixer view in Reaper when showing busses. The driver will hold the state internally and create a virtual view to reflect on the Faderport. This means, if you press the _Bus_ button it will 
 display all your tracks that use the bus prefix tag in _config.txt_ file. 

- _VCA view_* will not change the TCP or Mixer view in Reaper to show only VCA leaders. The driver will hold the state internally and create a virtual view to reflect on the Faderport. This means, if you press the _VCA_ button it will 
 display on the Faderport all the VCA leader tracks. ***NOT IMPLEMENTED YET**

## _Button behaviour_

*Note* if a button doesn't light up, it is not implemented yet.

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
| Bus | By adding the line "bus_prefix=A-" you can mark track names with a prefix (in this case, "A-", and show these tracks on the Faderport. Replace "A-" with characters of your choice. <br> |
| VCA | Will show VCA leader tracks in the future |
| Channel | _Prev/Next_ will move the surface display by -1/+1 channel at a time |
| Bank | _Prev/Next_ will move the surface display by 8 or 16 channels at a time |
| Prev | Move the surface display left by one channel or a bank of 8/16 depending on the _Channel_ or _Bank_ buttons status |
| Next | Move the surface display right by one channel or a bank of 8/16 depending on the _Channel_ or _Bank_ buttons status |
| Master | Will control the master buss volume with the large navigation wheel |
| Click | Toggle metronome on/off |
| Transport buttons| Repeat, Stop, Play/Pause, Record are all implemented|
| Automation buttons| All buttons implemented except _Off_|
## _Links_
- [Download] - 64bit Windows Build
- [CSI] - Control Surface Integrator
- [Airon] - Airon's Faderport config for CSI

   [download]: <https://stash.reaper.fm/v/43911/Fp16.zip>
   [csi]: <https://github.com/GeoffAWaddington/reaper_csurf_integrator/wiki>
   [airon]: <https://forum.cockos.com/showthread.php?t=240162>
