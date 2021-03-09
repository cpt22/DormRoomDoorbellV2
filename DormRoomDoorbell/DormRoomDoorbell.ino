#include <WiFi.h>
#include <WebServer.h>
#include <ButtonDebounce.h>
#include <Preferences.h>
#include <WiFiClient.h>
#include <Regexp.h>
#include <limits.h>
#include <driver/adc.h>
#include <uri/UriBraces.h>
#include <driver/i2s.h>

// Definitions
#define status_button_PIN 14
#define RING_BUTTON_PIN 32
#define RED_LED 33
#define GREEN_LED 15

#define SETUP_TIMEOUT 900
#define WIFI_RETRY_INTERVAL 5
#define SERVER_RETRY_INTERVAL 1

#define SERVER_PORT 4567

#define AUDIO_BUFFER_MAX 25

// SSID for Setup - definition
const char *setup_ssid = "doorbell_setup";
const char *setup_pass = "setup";

// IP Addresss Setup
const IPAddress local_IP(192, 168, 0, 3);
const IPAddress gateway(192, 168, 0, 1);
const IPAddress subnet(255, 255, 255, 0);

const String deviceID = WiFi.macAddress();

bool doneSettingUp = false;
bool wifiConfigured = false;
bool wifiConnected = false;

String wifi_ssid = "";
String wifi_pass = "";

int64_t wifiRetryTime = -20000000;
int64_t wifiConnectTime = -20000000;
int64_t serverRetryTime = -10000000;

// Audio Streaming Stuff
uint8_t audioBuffer[AUDIO_BUFFER_MAX];
uint8_t transmitBuffer[AUDIO_BUFFER_MAX];
uint32_t bufferPointer = 0;

bool transmitNow = false;
bool timerEnabled = false;

WebServer server(80);

WiFiClient client;

Preferences preferences;

// Input setup
ButtonDebounce status_button(status_button_PIN, 250);
ButtonDebounce ring_button(RING_BUTTON_PIN, 250);

void setup() {
  Serial.begin(115200);
  Serial.print("Device ID:");
  Serial.println(deviceID);

  // Setup LED Pins
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Load Preferences
  preferences.begin("drd", false);
  loadPreferences();
}

/**
   Loads preferences from the flash memory
*/
void loadPreferences() {
  if (preferences.getString("wifi_ssid", "") == "") {
    wifi_ssid = "";
    wifi_pass = "";
    wifiConfigured = false;
  } else {
    wifi_ssid = preferences.getString("wifi_ssid", "");
    wifi_pass = preferences.getString("wifi_pass", "");

    Serial.print("SSID: ");
    Serial.println(wifi_ssid);
    Serial.print("PW: ");
    Serial.println(wifi_pass);
    wifiConfigured = true;
  }
  Serial.println("Preferences Loaded!");
}

/**
   Enter wifi setup and configuration mode
*/
void enterSetupMode() {
  doneSettingUp = false;
  int64_t startSetupTime = currTime();

  // Disconnect from exisiting wifi network
  WiFi.disconnect();

  // Show setup LED
  digitalWrite(RED_LED, HIGH);

  // Enable setup AP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  bool apReady = WiFi.softAP(setup_ssid);
  Serial.print("Setup AP ");
  Serial.println(apReady ? "enabled" : "failed to enable");
  Serial.print("Listening on IP: ");
  Serial.println(WiFi.softAPIP());

  // Enable setup WebServer
  server.on(UriBraces("/setup/ssid/{}/password/{}"), handleNewWifiWithPW);
  server.on(UriBraces("/setup/ssid/{}"), handleNewWifiWithoutPW);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Setup webserver enabled");

  while (!doneSettingUp && microToSec(currTime() - startSetupTime) < SETUP_TIMEOUT) {
    server.handleClient();
  }

  server.stop();

  // Turn off setup LED
  digitalWrite(RED_LED, LOW);
}

/**
   Process a reuquest to the root webpage
*/
void handleRoot() {
  Serial.print("doing stuff");
  server.send(200, "text/html", "<h1>You made it</h1>");
}

/**
   Process a request that sets up a new wifi network
*/
void handleNewWifiWithPW() {
  String ssid = server.pathArg(0);
  String pass = server.pathArg(1);

  handleNewWifi(ssid, pass);
}

void handleNewWifiWithoutPW() {
  String ssid = server.pathArg(0);
  String pass = "";

  handleNewWifi(ssid, pass);
}

void handleNewWifi(String ssid, String pass) {
  if (ssid != "") {
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", pass);
    loadPreferences();
    server.send(200, "text/json", "{'status':'success'}");

    doneSettingUp = true;
  } else {
    Serial.println("Invalid ssid");
  }
}

/**
   Handle a not found error
*/
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

/**
   Handle the ring button push
*/
void handleRing() {
  // Turn on recording LED
  digitalWrite(GREEN_LED, HIGH);

  delay(2000);

  // Turn off recording LED
  digitalWrite(GREEN_LED, LOW);
}

void showStatus() {
  if (wifiConnected) {
    for (int i = 0; i < 5; i++) {
      digitalWrite(GREEN_LED, HIGH);
      delay(500);
      digitalWrite(GREEN_LED, LOW);
      delay(500);
    }
  } else if (wifiConfigured && !wifiConnected) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      delay(500);
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
      delay(500);
    }
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
  } else {
    for (int i = 0; i < 5; i++) {
      digitalWrite(RED_LED, HIGH);
      delay(500);
      digitalWrite(RED_LED, LOW);
      delay(500);
    }
  }
}


void loop() {
  status_button.update();
  ring_button.update();
  if (status_button.state() == LOW && ring_button.state() == LOW) {
    Serial.println("Setup button pressed");
    enterSetupMode();
  } else if (status_button.state() == LOW) {
    Serial.println("Status button pressed");
    showStatus();
  } else if (ring_button.state() == LOW) {
    Serial.println("Ring Button pressed");
    handleRing();
  }
  yield();
  if (wifiConfigured) {
    if (WiFi.status() != WL_CONNECTED && microToSec(currTime() - wifiRetryTime) > WIFI_RETRY_INTERVAL) {
      wifiRetryTime = currTime();
      wifiConnected = false;
      Serial.println("Attempting WiFi connection");
      WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    } else if (WiFi.status() == WL_CONNECTED && microToSec(currTime() - wifiConnectTime) > 1) {
      wifiConnectTime = currTime();
      wifiConnected = true;
    }
  }
  yield();
  if (wifiConnected) {
  }
}

int64_t currTime() {
  return esp_timer_get_time();
}

/**
   Converts a microseconds value to milliseconds value
*/
int64_t microToMillis(int64_t val) {
  return val / 1000;
}

/**
   Converts microseconds to seconds
*/
float microToSec(int64_t val) {
  return val / 1000000.0;
}
