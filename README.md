# Reaper Faderport 16/8 Native

## _A native controller for the Presonus 16/8 in Reaper_

This is alpha software and has bugs/unimplemented features and limitations. While it is public, I am not really doing support/requests for this at the moment. However, I will try to take care of bugs and issues as I find the time.

This controller is built for my use only and optimised for how I want my controller to behave in Reaper.

This public code version is a stripped down version of my non-public code. This simplified public version allows you to control one instance of either the Faderport 16 or Faderport 8.

The code base is originally designed to control two Faderport units at the same and offer advance features specific to my workflow. Therefore, there is still some legacy code and design patterns to take into account two Faderports, which I will try and remove/optimise as I go along.

Because I do not know of anyone using two Faderports or expect them to understand my view on how they should be controlled, I have released this simplified code.

If you want more flexibility and customisation, then I recommend using the Control Surface Integrator, by Geoff Waddington and installing Airon's Faderport configuration files as a starting point.  

Because the _Control Surface Integrator_ is a stateless design and will only mirror Reaper, it does not really work exactly as I want it to. This is why I developed a native controller for my own use. From my tests with the CSI, it will never work as I would like it to work because of its design ethos. This is not a criticism aimed at the CSI because it is a highly ambitious project and cleverly implemented and I myself use it for other DAW controllers. I use the CSI to control my _Console One_.

I do encourage the use of the _Control Surface Integrator_ because it is a very mature project with plenty of contributors and more people to help with your problems. It opens up a world of flexibility you can only dream.
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
- Navigation with transport wheel (not working very well)
- Pin the master bus to the surface 
- Enable bypass, macro, link, VI, Audio buttons to assign to custom actions
- Pan/Param wheel
- Shift + Save, Redo, Undo

## _What is not yet implemented?_
- Sends
- VCA leader view
- Zoom, Scroll, Section, Marker buttons. I haven't decided if I want to use them or how I want to use them
- Limited support for Nagivation Wheel. At the moment, it only works to control the Master buss volume
- RTZ << >> buttons. They go to the start/end of the project but are not useful right now.

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

If you want to use busses and have the bus button display them on Faderport then you can add a track title prefix here to identify busses. To activate them, make sure your track has this prefix in the start of its name.

```sh
bus_prefix=A-
```
For example, to create a bus track for Drums, rename the track to _A-DRUMS_. This track will now be removed from main view and will be shown when pressing the _Bus_ button on the Faderport. You can make the prefix anything you like.

Since Reaper doesn't really have traditional busses, you can think of this as a kind of filter instead that is engaged when pressing _Bus_. 


## _Links_
- [Download] - 64bit Windows Build (at the moment this is buggy as cleaning up the code has caused breakage)
- [CSI] - Control Surface Integrator
- [Airon] - Airon's Faderport config for CSI

   [download]: <https://stash.reaper.fm/v/43911/Fp16.zip>
   [csi]: <https://github.com/GeoffAWaddington/reaper_csurf_integrator/wiki>
   [airon]: <https://forum.cockos.com/showthread.php?t=240162>
