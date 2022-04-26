# Wallytron
This repo is for the Wallytron project, a guitar that makes sound using light! It's kind of a weird instrument but it looks cool and is fun to use.

## Hardware Requirements
This project was not designed from the ground up to be easily replicatable. If I do a second version I might pay more attention to that. If you still decide you want to try this, here's a list of parts:
### Fan
* Fan with clearance for the PCB (see layout and keep in mind the board doesn't easily mount to a shaft)

### Blade
* Wireless charging PCBA (can be had on amazon for ~$25)
* Buck converter 
* Blade PCB (see schematic and BOM inside)
* Teensy 4.0 - Heart and Soul of this project

### Keyboard
* Buttons of some kind (I used 10x Cherry Mx switches)
* Keyboard PCB (see corresponding BOM)
* Arduino Nano - A very important part but not quite the heart
* A means of mounting the keyboard to the fan - I used a block of wood and some pipe clamps, then screwed the PCB to it.

### Receiver
* A solar panel of some sort (bigger is likely better? I haven't tried more than one size)
* An amp
* (Optional) You may want some sort of high pass RC filter. I don't design audio equipment so I would suggest researching best practices, but a simple resistor and cap worked for me.

## Software Requirements
TeensyDuino 1.54+ is required to use this with the hardware I've originally used (the Teensy 4.0). You also need my [TLC5948 library](https://github.com/WilliamASumner/Tlc5948) and my [teensy graphics library](https://github.com/WilliamASumner/teensy-graphics). There are probably good chips out there that have nice libraries already, but I thought it would be fun to work from the ground up. Sorry that it had to be this way. Each of those should be a simple `git clone` into your `$ARDUINO/libraries` folder. You also need the RF24 and nRF24L01 libraries and may need to make a small modification to get the SPI to work with the TLC5948's.

## Setup
1) `git clone` this repo
2) Upload the `rf_comm_keyboard.ino` to the Arduino Nano/Board that is the keyboard
3) Upload the `rf_comm_blade.ino` to the Teensy board/LED board

It should be enough to just `git clone` this repo and upload the ino files. This component currently only supports a Teensy 4.0, I haven't tested it on anything else. It might work on a Teensy 4.1 provided the pins are wired correctly, but I'm fairly certain it won't work on something like an Arduino Nano, but that shouldn't discourage you from trying! I'm open to pull requests : )

## Ideas for Next Time
* Mounting holes on the PCB
* Integrated charging coil + receiver circuit + power circuit on PCB
* Independent LAT lines to each TLC5948 (allows for independent animatiion of TLC5948's)
* Integrated mounting on a motor shaft
* Larger keyboard (fretboard) (like a real guitar!)
* RF24-based programming of Teensy for ease of use
