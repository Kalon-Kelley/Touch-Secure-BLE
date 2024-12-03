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

enum connection_mode {
  ANY,
  PREVIOUSLY_AUTHENTICATED
};

struct DeviceNode {
  String name;
  DeviceNode* next;
};

const char* DEVICE_NAME = "SecureBLE";
const int AUTH_TIMEOUT_DELAY_MS = 15000;
const int MAX_DEVICES = 10;
DeviceNode* devices = nullptr;
long time_last_read = 0;
String rolling_id_hex = "";
boolean authenticated = false;
boolean was_connected = true;
boolean advertising = false;
connection_mode mode = ANY;

// IMU values
float Ax, Ay, Az;

// Sample service and characteristic which only has meaningful data
// after authentication
BLEService imuService("ACCE");
BLEFloatCharacteristic axCharacteristic("10A1", BLERead | BLENotify);

// Authentication service
BLEService authenticationService("AAAA");
BLEStringCharacteristic authCharacteristic("EEEE", BLEWrite, 100);
BLEByteCharacteristic modeCharacteristic("FFFF", BLEWrite);


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
  nfc.setTagWriteable(false);

  BLE.setLocalName(DEVICE_NAME);
  BLE.setDeviceName(DEVICE_NAME);
  BLE.setAdvertisedService(authenticationService);
  authenticationService.addCharacteristic(authCharacteristic);
  authenticationService.addCharacteristic(modeCharacteristic);
  imuService.addCharacteristic(axCharacteristic);
  BLE.addService(authenticationService);
  BLE.addService(imuService);

  Serial.println("Setup Complete");
}


void loop() {
  if (was_connected || millis() - time_last_read > AUTH_TIMEOUT_DELAY_MS && !authenticated) {
    emulate_nfc_tag();
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
  was_connected = true;
  String central_address = central.address();
  Serial.print("\nStarting authentication process with device [");
  Serial.print(central_address);
  Serial.println("]");
  authenticated = authenticate(central);
  if (!authenticated) central.disconnect();
  Serial.println(authenticated ? " -> Authentication Successful" : " -> Authentication Failed");

  // Auth completed so this is just the loop while the device is connected
  while (central.connected() && authenticated) {
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(Ax, Ay, Az);
      axCharacteristic.writeValue(Ax);
    }
    if (modeCharacteristic.written()) {
      Serial.print(" -> Changing connection mode to ");
      switch(modeCharacteristic.value()) {
        case ANY:
        Serial.println("ANY");
        mode = ANY;
        break;
        case PREVIOUSLY_AUTHENTICATED:
        Serial.println("PREVIOUSLY_AUTHENTICATED");
        mode = PREVIOUSLY_AUTHENTICATED;
        break;
        default:
        Serial.println("INVALID");
        break;
      }
    }
    delay(104);
  }
  authenticated = false;
  Serial.println(" -> Disconnected");
  delay(1000);
}


void emulate_nfc_tag() {
  if (advertising) {
    BLE.stopAdvertise();
    advertising = false;
  }
  was_connected = false;
  rolling_id_hex = String(random(999999), HEX);
  while (rolling_id_hex.length() < 5) {
    rolling_id_hex = "0" + rolling_id_hex;
    Serial.println(rolling_id_hex);
  }
  message = NdefMessage();
  message.addTextRecord("{\"id\": \"" + String(rolling_id_hex) + "\", \"device\": \"" + DEVICE_NAME + "\"}");
  messageSize = message.getEncodedSize();
  Serial.print("Ndef encoded message size: ");
  Serial.println(messageSize);
  message.encode(ndefBuf);
  nfc.setNdefFile(ndefBuf, messageSize);

  nfc.emulate();
}


boolean authenticate(BLEDevice central) {
  while(central.connected() && millis() - time_last_read < AUTH_TIMEOUT_DELAY_MS) {
    if (authCharacteristic.written()) {
      String token = authCharacteristic.value();
      String id = token.substring(0, 5);
      String name = token.substring(6);
      boolean known_device = false;

      if (id != rolling_id_hex) return false;

      DeviceNode* cur = devices;
      DeviceNode* prev = nullptr;
      int i = 0;
      while (cur != nullptr && i < MAX_DEVICES) {
        if (cur->name == name) {
          Serial.println(" -> Device [" + name + "] previously connected");
          known_device = true;
          if (prev == nullptr) break;
          prev->next = cur->next;
          cur->next = devices;
          devices = cur;
          break;
        }
        prev = cur;
        cur = cur->next;
        i++;
      }
      switch(mode) {
        case ANY:
        if (!known_device) {
          DeviceNode* new_device = new DeviceNode;
          new_device->name = name;
          add_new_device(new_device);
        }
        break;
        case PREVIOUSLY_AUTHENTICATED:
        if (!known_device) return false;
        break;
      }
      return true;
    }
    delay(100);
  }
  return false;
}

void add_new_device(DeviceNode* new_device) {
  Serial.println(" -> Adding new device to list [" + new_device->name + "]");
  DeviceNode* prev = nullptr;
  DeviceNode* cur = devices;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (cur == nullptr) {
      new_device->next = devices;
      devices = new_device;
      return;
    }
    if (i == MAX_DEVICES-1) break;
    prev = cur;
    cur = cur->next;
  }
  Serial.println(" -> Had to evict [" + cur->name + "]");
  delete cur;
  prev->next = nullptr;
  new_device->next = devices;
  devices = new_device;
}