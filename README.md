# Overview
What is this?  This is a project that I worked on nearly a year ago (circa July 2025).  I liked the aesthetics of the TP-Link Tapo S500D and Kasa HS220, but *HATED* their firmware.  Talking with remote servers was very undesirable, and the delay associated with using them was unreasonable.  The deal-breaker was when my wife kept rapidly pushing the button on a HS220 because our disposal (controlled by a ESPhome-based relay) wasn't responding quick enough.  She didn't understand the latency associated with global function calls, nor the issues with polling the state from Home Assistant.

This is an effort to understand just enough of the switch and re-implement it using ESPHome so that everything is local and the repsonse time is near instantaneous.

# Status
I have a mix of over eighty (yes 80!) TP-Link Tapo S500D and Kasa HS220 deployed across two homes.  Both have 100% of all wall switches replaced by the TP-Link Tapo S500D and Kasa HS220 runing ESPHome with the ESPHome code from this repo.  The project is heavily dependent on the great work done by the LibreTiny project to support the RTL8710CF microcontroller, as well as the early work from Prakoma to make OTA flashing possible!  Huge thanks to all of those individuals!

As with any project, there were some bumps along the way.  I'll try to keep this updated, but no promises.

# Known Issues
###
- Very infrequently (~once every few weeks), the UART communication between the two microcontrollers breaks down.  This appears to be an issue with LibreTiny.  As a workaround, the firmware detects this condition and automatically reboots the offending microcontroller
- Dimming is not always perfectly smooth.  The dimming is implemented in software, and it appears that sometimes an interrupt occurs that interrupts what should be a smooth transition

# Usage
Firstly, have a look at the [Programming](Programming/README.md) directory to understand what is required in order to access and re-program the board.  After that, clone the [Example](Example/) directory and compile/upload one of the examples, like so:

```
mkdir tp-link-tapo
cd tp-link-tapo

python3 -m venv venv
source venv/bin/activate

python3 -m pip install --upgrade pip
pip install esphome

git clone -n --depth=1 --filter=tree:0 https://github.com/Pwedge/tapo_smart.git
cd tapo_smart
git sparse-checkout set --no-cone /Example
git checkout
cd Example

cp packages/secrets.yaml.template packages/secrets.yaml
(edit packages/secrets.yaml to suite your environment and save the file)

esphome compile dimmer-single.yaml
esphome upload dimmer-single.yaml --device /dev/ttyUSB0
```

After the upload, power cycle or hard-reset the device using the "Restart" button on front of the switch.  The switch should now be running ESPHome-based firmware.  It will have the ability to function as a proper dimmer, allow configuration of the minimum and maximum dimming values, and it will be ready to be updated over the air in the future.
