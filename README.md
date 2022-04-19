# ez-clang-arduino

This repository provides an Arduino-based reference implementation of the [ez-clang RPC and runtime interface](https://github.com/echtzeit-dev/ez-clang#documentation).
The build processes is based on [PlatformIO](https://platformio.org/) and allows for quick adaptation.
This is a good starting point for developing a compatible firmware for your own development boards.

## Supported host machines

Right now, build instructions are tested only on Intel x86_64 hosts running Ubuntu 20.04.
If you get it to work on other systems, please document your steps and create a pull request.

## Supported target devices

For the time being, the [Arduino Due](https://docs.arduino.cc/hardware/due) development board acts as the reference hardware for ez-clang.
It features an Atmel SAM3X8E ARM Cortex-M3 CPU (armv7-m) and has more than enough flash and RAM for typical purposes.
If you get it to work with other boards, please share your patches.

## Build

Install and run [PlatformIO](https://docs.platformio.org/en/latest/core/installation.html#super-quick-mac-linux):

```
➜ python3 -c "$(curl -fsSL https://raw.githubusercontent.com/platformio/platformio/master/scripts/get-platformio.py)"
➜ export PATH=$PATH:$HOME/.platformio/penv/bin
➜ git clone https://github.com/echtzeit-dev/ez-clang-arduino
➜ cd ez-clang-arduino
➜ platformio run -e due
...
Replacing .pio/build/due/firmware.elf
Relink succeeded
Building .pio/build/due/firmware.bin
```

## Run

Connect your board, upload the `firmware.bin` with [PlatformIO](https://docs.platformio.org/en/stable/core/quickstart.html#process-project) and 
verify the serial output:
```
> platformio run -t upload -t monitor -e due
...
Verify successful
done in 12.890 seconds
Set boot flash true
CPU reset.
...
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
␁#W��W#␁j␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␀␅␀␀␀␀␀␀␀0.0.5P5␇ ␀␀␀␀�F␁␀␀␀␀␀␁␀␀␀␀␀␀␀␕␀␀␀␀␀␀␀__ez_clang_rpc_lookup�␀␀␀␀␀
```
