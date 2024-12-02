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
