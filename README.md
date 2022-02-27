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

## _Links_
- [Download] - 64bit Windows Build
- [CSI] - Control Surface Integrator
- [Airon] - Airon's Faderport config for CSI

   [download]: <https://stash.reaper.fm/manage_file/43911/Fp16.zip>
   [csi]: <https://github.com/GeoffAWaddington/reaper_csurf_integrator/wiki>
   [airon]: <https://forum.cockos.com/showthread.php?t=240162>
