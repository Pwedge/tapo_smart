# Overview
What is this?  This is a project that I worked on nearly a year ago (circa July 2025).  I liked the aesthetics of the TP-Link Tapo S500D and Kasa HS220, but *HATED* their firmware.  Talking with remote servers was very undesirable, and the delay associated with using them was unreasonable.  The deal-breaker was when my wife kept rapidly pushing the button on a HS220 because our disposal (controlled by a ESPhome-based relay) wasn't responding quick enough.  She didn't understand the latency associated with global function calls, nor the issues with polling the state from Home Assistant.  This is was an effort to understand just enough of the switch and re-implement it using ESPHome so that everything is local and the repsonse time is near instantaneous.

# Status
I have a mix of over eighty (yes 80!) TP-Link Tapo S500D and Kasa HS220 deployed across two homes.  Both have 100% of all wall switches replaced by the TP-Link Tapo S500D and Kasa HS220 runing ESPHome with the ESPHome code from this repo.  The project is heavily dependent on the great work done by the LibreTiny project to support the RTL8710CF microcontroller, as well as the early work from Prakoma to make OTA flashing possible!  Huge thanks to all of those individuals!

As with any project, there were some bumps along the way.  I'll try to keep this updated, but no promises.

# Known Issues
###
- Very infrequently (~once every few weeks), the UART communication between the two microcontrollers breaks down.  This appears to be an issue with LibreTiny.  As a workaround, the firmware detects this condition and automatically reboots the offending microcontroller
- Dimming is not always perfectly smooth.  The dimming is implemented in software, and it appears that sometimes and interrupt occurs that interrupts what should be a smoot transition
