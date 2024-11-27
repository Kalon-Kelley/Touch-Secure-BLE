#include <ArduinoBLE.h>
#include <Arduino_LSM6DSOX.h>
#include "emulatetag.h"
#include "NdefMessage.h"
#include <PN532_HSU.h>
#include <PN532.h>

PN532_HSU pn532hsu(Serial1);
EmulateTag nfc(pn532hsu);

uint8_t ndefBuf[120];
NdefMessage message;
int messageSize;
uint8_t uid[3] = {0x12, 0x34, 0x56};

const char* DEVICE_NAME = "SecureBLE";
const int AUTH_TIMEOUT_DELAY_MS = 10000;
const int MAX_DEVICES = 10;
String devices[MAX_DEVICES] = {};
long time_last_read = -AUTH_TIMEOUT_DELAY_MS;
int rolling_id = 0;
boolean authenticated = false;
boolean advertising = false;

// IMU values
float Ax, Ay, Az;

// Sample service and characteristic which only has meaningful data
// after authentication
BLEService imuService("ACCE");
BLEFloatCharacteristic axCharacteristic("10A1", BLERead | BLENotify);

// Authentication service
BLEService authenticationService("AAAA");
BLEIntCharacteristic keyCharacteristic("EEEE", BLEWrite);

void setup() {
  Serial.begin(115200);
  while(!Serial);

  if (!BLE.begin()) {
    Serial.println("Failed to initialize BLE");
    while(1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU");
    while(1);
  }

  nfc.setUid(uid);
  nfc.init();

  BLE.setLocalName(DEVICE_NAME);
  BLE.setDeviceName(DEVICE_NAME);
  BLE.setAdvertisedService(authenticationService);
  authenticationService.addCharacteristic(keyCharacteristic);
  imuService.addCharacteristic(axCharacteristic);
  BLE.addService(authenticationService);
  BLE.addService(imuService);

  Serial.println("Setup Complete");
}

void loop() {
  if (millis() - time_last_read > AUTH_TIMEOUT_DELAY_MS && !authenticated) {
    if (advertising) {
      BLE.stopAdvertise();
      advertising = false;
    }
    rolling_id = random(999999);
    message = NdefMessage();
    message.addTextRecord(String(rolling_id));
    messageSize = message.getEncodedSize();
    Serial.print("Ndef encoded message size: ");
    Serial.println(messageSize);
    message.encode(ndefBuf);
    nfc.setNdefFile(ndefBuf, messageSize);

    boolean emulated = nfc.emulate();
    Serial.println("Emulation interrupted, likely by a read");
    time_last_read = millis();
  }
  if (!advertising) {
    BLE.advertise();
    advertising = true;
  }
  
  BLEDevice central = BLE.central();
  if (!central || !central.connected()) {
    return;
  }
  String central_address = central.address();
  Serial.print("\nStarting authentication process with device [");
  Serial.print(central_address);
  Serial.println("]");
  while(central.connected() && !authenticated) {
    if (millis() - time_last_read > AUTH_TIMEOUT_DELAY_MS) {
      Serial.println(" -> Timeout... Disconnecting");
      central.disconnect();
      return;
    }
    if (keyCharacteristic.written()) {
      Serial.println(keyCharacteristic.value());
      authenticated = keyCharacteristic.value() == rolling_id;
      if (!authenticated) {
        Serial.println(" -> Invalid Key... Disconnecting");
        central.disconnect();
        return;
      } else {
        Serial.println(" -> Authenticated");
      }
    }
    delay(100);
  }

  while (central.connected() && authenticated) {
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(Ax, Ay, Az);
      axCharacteristic.writeValue(Ax);
    }
    delay(104);
  }
  authenticated = false;
  Serial.println(" -> Disconnected");
  delay(1000);
}