#include <DW1000.h>
#include <DW1000CompileOptions.h>
#include <DW1000Constants.h>
#include <DW1000Device.h>
#include <DW1000Mac.h>
#include <DW1000Ranging.h>
#include <DW1000Time.h>
#include <deprecated.h>
#include <require_cpp11.h>

// The purpose of this code is to set the tag address and antenna delay to default.
// Added monochrome OLED display so that range tests can be conducted without connection to a computer.
// OLED reports anchor ID (84) and range in meters.
// Now includes manual configuration of channel, pulse frequency, data rate and preamble length
// Plus preset configurations with simplified verification of actual settings
// UPDATED: All configurations now use 16MHz PRF, 6800kbps data rate, 64 preamble length

#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"
// connection pins
#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS 17
const uint8_t PIN_RST = 27; // reset pin
const uint8_t PIN_IRQ = 34; // irq pin
const uint8_t PIN_SS = 17;  // spi select pin
// TAG antenna delay defaults to 16384
// leftmost two bytes below will become the "short address"
char tag_addr[] = "7D:00:22:EA:82:60:3B:9C";

#include <Wire.h>
#include <U8g2lib.h>
// Declaration for an SH1106 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
#define I2C_SDA 22
#define I2C_SCL 21

U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);  // page-buffered version

// Configuration options
// 0 = Manual configuration
// 1-6 = Preset configurations (Channel 1, 2, 3, 4, 5, 7)
uint8_t configOption = 0;  // Set this to select configuration mode (0-6)

// Manual configuration settings (only used when configOption = 0)
// UPDATED: 16MHz PRF, 6800kbps, 64 preamble for low-power operation
uint8_t manualChannel = 5;
uint8_t manualPulseFrequency = DW1000.TX_PULSE_FREQ_16MHZ;
uint8_t manualDataRate = DW1000.TRX_RATE_6800KBPS;
uint8_t manualPreambleLength = DW1000.TX_PREAMBLE_LEN_64;

// Variables that will hold the active configuration
uint8_t channel;
uint8_t pulseFrequency;
uint8_t dataRate;
uint8_t preambleLength;

// Button configuration
const uint8_t BUTTON_PIN = 14;  // GPIO 14 for the button
uint8_t lastButtonState = HIGH; // Assume button is not pressed initially (pulled up)
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 300; // Debounce time to avoid multiple triggers

void printDetailedData() {
  // Get the device that completed ranging
  DW1000Device* device = DW1000Ranging.getDistantDevice();
  uint16_t deviceId = device->getShortAddress();
  
  // Get standard measurements
  float distance = device->getRange();
  float rxPower = device->getRXPower();
  float fpPower = device->getFPPower();
  float quality = device->getQuality();
  
  // Get raw signal measurements
  byte noiseBytes[2];
  byte fpAmpl1Bytes[2];
  byte fpAmpl2Bytes[2];
  byte fpAmpl3Bytes[2];
  byte cirPwrBytes[2];
  
  DW1000.readBytes(RX_FQUAL, STD_NOISE_SUB, noiseBytes, 2);
  DW1000.readBytes(RX_TIME, FP_AMPL1_SUB, fpAmpl1Bytes, 2);
  DW1000.readBytes(RX_FQUAL, FP_AMPL2_SUB, fpAmpl2Bytes, 2);
  DW1000.readBytes(RX_FQUAL, FP_AMPL3_SUB, fpAmpl3Bytes, 2);
  DW1000.readBytes(RX_FQUAL, CIR_PWR_SUB, cirPwrBytes, 2);
  
  uint16_t noise = (uint16_t)noiseBytes[0] | ((uint16_t)noiseBytes[1] << 8);
  uint16_t fpAmpl1 = (uint16_t)fpAmpl1Bytes[0] | ((uint16_t)fpAmpl1Bytes[1] << 8);
  uint16_t fpAmpl2 = (uint16_t)fpAmpl2Bytes[0] | ((uint16_t)fpAmpl2Bytes[1] << 8);
  uint16_t fpAmpl3 = (uint16_t)fpAmpl3Bytes[0] | ((uint16_t)fpAmpl3Bytes[1] << 8);
  uint16_t cirPwr = (uint16_t)cirPwrBytes[0] | ((uint16_t)cirPwrBytes[1] << 8);
  
  // Print in a single line with comma separation
  Serial.print("ID: 0x");
  Serial.print(deviceId, HEX);
  Serial.print(", Range: ");
  Serial.print(distance, 2);
  Serial.print("m, RXPower: ");
  Serial.print(rxPower, 2);
  Serial.print("dBm, FPPower: ");
  Serial.print(fpPower, 2);
  Serial.print("dBm, Quality: ");
  Serial.print(quality, 2);
  Serial.print(", Noise: ");
  Serial.print(noise);
  Serial.print(", FPAmpl1: ");
  Serial.print(fpAmpl1);
  Serial.print(", FPAmpl2: ");
  Serial.print(fpAmpl2);
  Serial.print(", FPAmpl3: ");
  Serial.print(fpAmpl3);
  Serial.print(", CIRPwr: ");
  Serial.println(cirPwr);
}

void setup()
{
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);  
  delay(1000);

  // Set up button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Set configuration based on selected option
  if (configOption == 0) {
    // Manual configuration - UPDATED to 16MHz PRF, 6800kbps, 64 preamble
    channel = manualChannel;
    pulseFrequency = manualPulseFrequency;
    dataRate = manualDataRate;
    preambleLength = manualPreambleLength;
    Serial.println(F("Using manual configuration"));
  } else {
    // Preset configurations - UPDATED to 16MHz PRF, 6800kbps, 64 preamble
    pulseFrequency = DW1000.TX_PULSE_FREQ_16MHZ;
    dataRate = DW1000.TRX_RATE_6800KBPS;
    preambleLength = DW1000.TX_PREAMBLE_LEN_64;
    
    // Set channel based on preset selection
    switch (configOption) {
      case 1: channel = 1; break;
      case 2: channel = 2; break;
      case 3: channel = 3; break;
      case 4: channel = 4; break;
      case 5: channel = 5; break;
      case 6: channel = 7; break;
      default: channel = 5; break; // Default to channel 5 if invalid option
    }
    Serial.print(F("Using preset configuration #"));
    Serial.println(configOption);
  }

  Serial.println(F("UWB: Setting up tag"));

  display.begin();
  display.setFont(u8g2_font_6x12_tf);  // Compact font

  // Clear the buffer and show initial info
  display.firstPage();
  do {
    display.setCursor(0, 12);  // Start at top-left corner
    display.print("UWB Tag Initializing");
    display.setCursor(0, 24);
    display.print("Channel: ");
    display.print(channel);
    display.setCursor(0, 36);
    display.print("PF: ");
    display.print((pulseFrequency == DW1000.TX_PULSE_FREQ_16MHZ ? "16MHz" : "64MHz"));
  } while (display.nextPage());

  // initialize the UWB configuration
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); // Reset, CS, IRQ pin

  // Create configuration array
  byte customMode[3];
  customMode[0] = dataRate;
  customMode[1] = pulseFrequency;
  customMode[2] = preambleLength;

  // attach callbacks
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  // Start as tag with our configuration
  DW1000Ranging.startAsTag(tag_addr, customMode, false);
  
  // Set channel explicitly after starting
  DW1000.setChannel(channel);
  // Commit the configuration
  DW1000.commitConfiguration();

  // Read and print the actual configuration from the device
  Serial.println("Actual configuration from DW1000:");
  char msgBuffer[100];
  DW1000.getPrintableDeviceMode(msgBuffer);
  Serial.println(msgBuffer);
}

void loop()
{
  // Check for button press to change configuration
  int reading = digitalRead(BUTTON_PIN);
  
  // If button state changed (pressed)
  if (reading == LOW && lastButtonState == HIGH) {
    // Debounce
    if ((millis() - lastDebounceTime) > debounceDelay) {
      // Increment configuration, with wraparound
      configOption = (configOption + 1) % 7;  // 0-6
      
      // Apply new configuration
      applyNewConfiguration();
      
      lastDebounceTime = millis();
    }
  }
  
  lastButtonState = reading;

  // Regular DW1000 loop
  DW1000Ranging.loop();
}

// Function to apply new configuration when button is pressed
void applyNewConfiguration() {
  // Set configuration based on new configOption
  if (configOption == 0) {
    // Manual configuration - UPDATED to 16MHz PRF, 6800kbps, 64 preamble
    channel = manualChannel;
    pulseFrequency = manualPulseFrequency;
    dataRate = manualDataRate;
    preambleLength = manualPreambleLength;
    Serial.println(F("Changed to manual configuration"));
  } else {
    // Preset configurations - UPDATED to 16MHz PRF, 6800kbps, 64 preamble
    pulseFrequency = DW1000.TX_PULSE_FREQ_16MHZ;
    dataRate = DW1000.TRX_RATE_6800KBPS;
    preambleLength = DW1000.TX_PREAMBLE_LEN_64;
    
    // Set channel based on preset selection
    switch (configOption) {
      case 1: channel = 1; break;
      case 2: channel = 2; break;
      case 3: channel = 3; break;
      case 4: channel = 4; break;
      case 5: channel = 5; break;
      case 6: channel = 7; break;
      default: channel = 5; break;
    }
    Serial.print(F("Changed to preset configuration #"));
    Serial.println(configOption);
  }
  
  // Reset DW1000 to apply new configuration
  DW1000.reset();
  
  // Need to reinitialize the Ranging module
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  
  // Set antenna delay for tags
  DW1000.setAntennaDelay(16384); // Default tag antenna delay
  
  // Create configuration array
  byte customMode[3];
  customMode[0] = dataRate;
  customMode[1] = pulseFrequency;
  customMode[2] = preambleLength;
  
  // Reattach callbacks
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);
  
  // Start as tag with our new configuration
  DW1000Ranging.startAsTag(tag_addr, customMode, false);
  
  // Set channel explicitly after starting
  DW1000.setChannel(channel);
  // Commit the configuration
  DW1000.commitConfiguration();
  
  // Read and print the actual configuration from the device
  Serial.println("New configuration applied. Actual configuration from DW1000:");
  char msgBuffer[100];
  DW1000.getPrintableDeviceMode(msgBuffer);
  Serial.println(msgBuffer);
  
  // Update OLED with new config
  display.firstPage();
  do {
    display.setCursor(0, 12);
    display.print("Config changed to #");
    display.print(configOption);
    display.setCursor(0, 24);
    display.print("CH:");
    display.print(channel);
    display.print(" ");
    display.print(pulseFrequency == DW1000.TX_PULSE_FREQ_16MHZ ? "16M" : "64M");
    display.setCursor(0, 36);
    display.print("Wait for connection...");
  } while (display.nextPage());
}

unsigned long lastOledUpdate = 0;
const unsigned long oledInterval = 200;  // update OLED every 200 ms

// In the newRange() function of the Tag code, update the OLED display part:

void newRange() {
  float distance = DW1000Ranging.getDistantDevice()->getRange();
  uint16_t anchorID = DW1000Ranging.getDistantDevice()->getShortAddress();
 
  // Print detailed data including all the signal metrics
  printDetailedData();

  unsigned long now = millis();
  if (now - lastOledUpdate >= oledInterval) {
    lastOledUpdate = now;

    display.firstPage();
    do {
      // First line: Configuration
      display.setCursor(0, 12);
      display.print("CH:");
      display.print(channel);
      display.print(" PRF:");
      display.print(pulseFrequency == DW1000.TX_PULSE_FREQ_16MHZ ? "16" : "64");
      
      // Second line: Data rate and preamble length - UPDATED to show correct values
      display.setCursor(0, 24);
      display.print("DR:");
      if(dataRate == DW1000.TRX_RATE_110KBPS)
        display.print("110");
      else if(dataRate == DW1000.TRX_RATE_850KBPS)
        display.print("850");
      else
        display.print("6800");
      display.print(" PL:");
      if(preambleLength == DW1000.TX_PREAMBLE_LEN_64)
        display.print("64");
      else if(preambleLength == DW1000.TX_PREAMBLE_LEN_128)
        display.print("128");
      else if(preambleLength == DW1000.TX_PREAMBLE_LEN_2048)
        display.print("2048");
      else
        display.print(preambleLength);
      
      // Third line: ID and distance only
      display.setCursor(0, 36);
      display.print("ID:");
      display.print(anchorID, HEX);
      display.print(" D:");
      display.print(distance, 2);
      display.print("m");
    } while (display.nextPage());
  }
}

void newDevice(DW1000Device *device)
{
  Serial.print("Device added: ");
  Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device *device)
{
  Serial.print("delete inactive device: ");
  Serial.println(device->getShortAddress(), HEX);
}