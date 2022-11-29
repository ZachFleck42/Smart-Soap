#include <ArduinoHttpClient.h>
#include <cmath>
#include <iostream>
#include <WiFiNINA.h>
#include "base64.hpp"

// Initialize WiFi stuff
char ssid[] = "YOUR_WIFI_SSID_HERE";
char pass[] = "YOUR_WIFI_PASS_HERE";
WiFiSSLClient client;

// Initialize server stuff
String endpoint_server = "YOUR_ENDPOINT_SERVER_HERE";
String endpoint_path = "YOUR_ENDPOINT_PATH_HERE";
String timestamp_server = "YOUR_TIMESTAMP_SERVER_HERE";
String timestamp_path = "YOUR TIMESTAMP_PATH_HERE";

// Initialize sensor stuff
#define SENSOR_PIN      A0
#define KNOWN_RESISTOR  560
#define VIN             3.3

// Intialize calibration values
float maxResistance = 0;
float minResistance = 0;
float stableValue   = 100;
float newValue      = 0;

// Intialize buffer values
#define BUFFER_SIZE 15
float buffer[BUFFER_SIZE] = {0};
int buffer_index = 0;


void setup(void) {
  // Open port and await connection
  Serial.begin(9600);
  while (!Serial);

  // Attempt to connect to the WiFi network
  int wifi_status = WL_IDLE_STATUS;
  while (wifi_status != WL_CONNECTED) {
    Serial.println("Attempting to connect to WiFi network...");
    wifi_status = WiFi.begin(ssid, pass);
    delay(10000);
  }

  // Connected to Wi-Fi. Print out network info
  Serial.println("Connected to the network");
  Serial.println("----------------------------------------");
  printWiFiData();
  Serial.println("----------------------------------------");
  Serial.println();
}

void loop(void) {
  // Read the sensor and convert to a digital value
  float sensorValue = analogRead(SENSOR_PIN);

  // Calculate the current resistance of the sensor
  float sensorResistance = calculateResistance(sensorValue);

  // Check to see if a new stable value has been found
  for (int i = 0; i < BUFFER_SIZE; i++) {
    int valueCount = 0;
    for (int j = 0; j < BUFFER_SIZE; j++) {
      if (buffer[i] == buffer[j]) {valueCount++;}
    }

    // If a stable value has been found, remember it
    if (valueCount >= (BUFFER_SIZE / 2 + 1)) {
      newValue = buffer[i];
      break;
    }
  }

  // If there's a new stable value and it's significantly different, post it
  if (newValue && (abs(stableValue - newValue) > 25)) {
    stableValue = newValue;
    newValue = 0;

    // Calculate volume and adjust if necessary
    float volume = calculateVolume(stableValue);
    if (volume >= 95) {volume = 100;}
    if (volume <= 5) {volume = 0;}

    Serial.print("Volume changed to: ");
    Serial.print(volume);
    Serial.println("%");

    // Get a timestamp
    Serial.println("Getting a timestamp...");
    String timestamp = getTimestamp();
    Serial.print("Timestamp aqcuired: ");
    Serial.println(timestamp);

    // Create a payload with the soapLevel and timestamp
    Serial.println("Creating a payload...");
    String payload = createPayload(volume, timestamp);
    Serial.println("Payload created");

    // Post the data
    Serial.println("Posting the data to endpoint...");
    postData(payload);
    Serial.println("Sucessfully posted to endpoint");
    Serial.println();
  }
  else {newValue = 0;}

  // Reset the buffer_index if necessary
  if (BUFFER_SIZE == buffer_index) {buffer_index = 0;}

  buffer[buffer_index++] = sensorResistance;
  delay(1000);
}


float printWiFiData() {
  Serial.print("Board IP Address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  Serial.print("Network SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("Network signal strength (RSSI): ");
  long rssi = WiFi.RSSI();
  Serial.println(rssi);

  Serial.print("Network encryption Type: ");
  byte encryption = WiFi.encryptionType();
  Serial.println(encryption, HEX);
}


float calculateResistance(float sensorValue) {
  float Vout = (sensorValue * VIN) / 1024.0;
  return KNOWN_RESISTOR * ((VIN / Vout) - 1);
}


float calculateVolume(float sensorResistance) {
  float difference = maxResistance - sensorResistance;
  return difference / (maxResistance - minResistance) * 100;
}


String getTimestamp() {
  // Attempt to connect to the timestamp API
  if (client.connect(timestamp_server, 443)) {
    Serial.println("Connected to timestamp API");
  }
  else {
    Serial.println("Error connecting to timestamp API");
    return "";
  }

  // If successfully connected, send a GET request
  client.print("GET ");
  client.print(timestamp_path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(timestamp_server);
  client.println("Connection: close");
  client.println();

  // Wait for the client to be available before continuing
  while(!client.available());

  // Get the response from the server
  String response = "";
  while (client.available()) {
    char c = client.read();
    String d = String(c);
    response = response + d;
  }
  client.stop();

  // Search the response for the specific timestamp needed
  int dateTimeIndex = response.indexOf("datetime");
  int timestampStart = dateTimeIndex + 11;
  int timestampEnd = dateTimeIndex + 43;

  // Pull the timestamp from the response and return it
  String timestamp = response.substring(timestampStart, timestampEnd);
  return timestamp;
}


String createPayload(int soapLevel, String timestamp) {
  // Create a very long string of necessary data
  String soapLevelStr = String(soapLevel);
  String dataString = "{\"Events\":[{\"USER_ID\":\"1\",\"DEVICE_ID\":\"111111\",\"TAG_TYPE\":\"gen2\",\"TID\":\"11111\",\"SCAN\":\"11-22-33\",\"SOAP_LEVEL\":" + soapLevelStr + ",\"DEVICE_HOST_NAME\":\"soap-101\",\"DEVICE_TYPE\":\"soapDispenser\",\"DEVICE_SERIALNO\":\"3234234\",\"EVENT_TYPE\":\"rfid\",\"EVENT_PRIORITY\":\"info\",\"EVENT_DATETIME\":\"" + timestamp + "\",\"CREATE_DATETIME\":\"" + timestamp + "\"}]}";

  // Encode the dataString into base64
  const char* data = dataString.c_str();
  size_t dataLength = strlen(data);
  char encodedData[BASE64::encodeLength(dataLength)];
  BASE64::encode((const uint8_t*)data, dataLength, encodedData);

  // Wrap the encodedData and return the complete payload
  String payload = "{\"message\":{\"data\":\"" + (String)encodedData + "\"}}";
  return payload;
}


void postData(String payload) {
  // Attempt to connect to the endpoint
  if (client.connect(endpoint_server, 443)) {
    Serial.println("Connected to endpoint API");
    client.print("POST ");
    client.print(endpoint_path);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(endpoint_server);
    client.println("User-Agent: Arduino/1.0");
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(payload.length());
    client.println();
    client.println(payload);
    client.stop();
  }
  else {
    Serial.println("Error connecting to endpoint API");
  }
}