# Touch Secure BLE

NFC mediated BLE connections with two modes

1. Allow all users which have physical access (NFC) to connect and use device

2. Allow only previously authenticated devices to continue using device provided
they have physical access

## How it works
- WebApp will read NFC tag being emulated by the PN532 NFC module on the
  device which has a single NDEF text record of the form
  ```bash
  {id: <ID_IN_HEX>, device: <DEVICE_NAME>}
  ```
  where `ID_IN_HEX` is a rolling random ID in HEX and `DEVICE_NAME` is the name
  of the BLE device to use for connection.
- The WebApp will then attempt to connect to the given device (provided by 
  `DEVICE_NAME`)
- Upon connecting to the device the WebApp will write the given ID and an 
  identifier which the user will decide in the WebApp to the characteristic 
  `EEEE`.
  
  The format of what it writes is as follows as a string.
  
  ```bash
  <ID_IN_HEX>:<IDENTIFIER>
  ```
- After this depending on the mode of the device the WebApp will either be 
  connected or disconnected depending on several factors which I am too lazy to 
  write

## Setup
1. After installing the Arduino IDE, clone the following repos into the `/Documents/Arduino/libraries` directory on your computer.
- On Windows, the location is `%userprofile%/Documents/Arduino/libraries`
```bash
git clone --recursive https://github.com/Seeed-Studio/PN532.git
```
```bash
git clone --recursive https://github.com/don/NDEF.git
```

2. Add `-DNFC_INTERFACE_HSU` to the bottom of defines.txt file for the NANO_RP2040 board.  Check the version number after `mbed_nano` if you cannot locate the directory.
- On MacOS, this is located in  `~/Library/Arduino15/packages/arduino/hardware/mbed_nano/4.2.1/variants/NANO_RP2040_CONNECT/`
- On Windows, this is located in `%userprofile%\AppData\Local\Arduino15\packages\arduino\hardware\mbed_nano\4.1.5\variants\NANO_RP2040_CONNECT`

3. In `Documents/Arduino/libraries/NFC/src/emulatetag.cpp` or `%userprofile%\Documents\Arduino\libraries\PN532\src\emulatetag.cpp` replace the line 82 `command[]` with the following
```c++
  uint8_t command[] = {
      PN532_COMMAND_TGINITASTARGET,
      0x05, // MODE: PICC only, Passive only

      0x04, 0x00,       // SENS_RES
      0x00, 0x00, 0x00, // NFCID1
      0x20,             // SEL_RES

      // FELICA PARAMS
      0x01, 0xFE,         // NFCID2t (8 bytes) 
      0x05, 0x01, 0x86,
      0x04, 0x02, 0x02,
      0x03, 0x00,         // PAD (8 bytes)
      0x4B, 0x02, 0x4F, 
      0x49, 0x8A, 0x00,   
      0xFF, 0xFF,         // System code (2 bytes)

      0x01, 0x01, 0x66,   // NFCID3t (10 bytes)
      0x6D, 0x01, 0x01, 0x10,
      0x02, 0x00, 0x00,

      0x00, // length of general bytes
      0x00  // length of historical bytes
  };
```
