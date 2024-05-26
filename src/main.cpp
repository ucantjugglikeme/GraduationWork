#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <PubSubClient.h>
#include <AsyncEventSource.h>
#include <Regexp.h>
#include <nlohmann/json.hpp>
#include <LiquidCrystal.h>

// TODO: Clean code and delete redundant serial prints

// Macro to read build flags
#define ST(A) #A
#define STR(A) ST(A)

using json = nlohmann::json;

#define THRESHOLD 1000
// #define BUZZER_FREQUENCY 900

#define MQ2_AO_PIN 1  // ESP32's pin GPIO1 connected to AO pin of the MQ2 sensor
#define MQ2_DO_PIN 2  // ESP32's pin GPIO2 connected to DO pin of the MQ2 sensor
#define BUZZER_PIN 9

AsyncWebServer WebServer(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

String WIFI_SSID_CLIENT;
String WIFI_PASS_CLIENT;
String HOSTNAME;
String ADDRESS;
String WIFI_SSID_SERVER = "gleakactor";
String WIFI_PASS_SERVER = "gleakactor1";
String SERVER = "188.225.58.207";
int MQTT_PORT = 1883;

const char * MQTT_USER_ = STR(MQTT_USER);
const char * MQTT_PASSWORD_ = STR(MQTT_PASSWORD);

// TODO: add structure for gas level data (json)

// TODO: set valid topics
String GAS_TOPIC = "gas-leak-detection/";
String TEST_TOPIC = "gas-leak-detection/";

// TODO: add json
// gas-value: int
// gas-leakage: bool
// threshold: int

bool CONNECTION_STATUS = false;

const char INDEX_HTML_CONFIGURE[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    <h1>Enter parameters</h1>
    <p>
      Wi-Fi network name: <input name="network_name" type="text" /><br />
      Wi-Fi network password: <input name="network_password" type="password" /><br />
      Device name: <input name="device_name" type="text" /><br />
      Address: <input name="address" type="text" /><br />
      <input type="submit" value="Submit" />
    </p>
  </form>
</body></html>)rawliteral";

constexpr uint8_t PIN_RS = 3;
constexpr uint8_t PIN_DB4 = 4;
constexpr uint8_t PIN_DB5 = 5;
constexpr uint8_t PIN_DB6 = 6;
constexpr uint8_t PIN_DB7 = 7;
constexpr uint8_t PIN_EN = 8;

LiquidCrystal lcd(PIN_RS, PIN_EN, PIN_DB4, PIN_DB5, PIN_DB6, PIN_DB7);

int cooldown = 5000;
long lastCheck = 0;

void setup_wifi_server() {
  Serial.print("Setting AP (Access Point)…");
  WiFi.softAP(WIFI_SSID_SERVER, WIFI_PASS_SERVER);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println("IP address: ");
  Serial.println(IP);
  Serial.println("Server started");
}

void setup_wifi_client() {
  delay(10);

  int attempts = 0;

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID_CLIENT);

  WiFi.setHostname(HOSTNAME.c_str());
  WiFi.begin(WIFI_SSID_CLIENT, WIFI_PASS_CLIENT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);

    Serial.print("connecting... ");
    Serial.println(attempts);
    attempts += 1;
    if (attempts == 30) {
      break;
    }
  }

  if (attempts >= 30) {
    WIFI_SSID_CLIENT = "";
    WIFI_PASS_CLIENT = "";
    HOSTNAME = "";
    ADDRESS = "";
    Serial.println("");
    Serial.println("WiFi connect faild");
  }
  else {
    CONNECTION_STATUS = true;
    WiFi.softAPdisconnect(true);
    WebServer.end();
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void recievedCallback(char * topic, byte * payload, unsigned int length) {
  Serial.print("Message received: ");
  Serial.println(topic);

  // TODO: add response to the topic
  Serial.print("payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup_mqtt_client() {
  bool available = Ping.ping(SERVER.c_str(), 3);

  if (available) {
    Serial.println("Server is available");
  }
  else {
    Serial.println("Server is not available");
  }

  mqttClient.setServer(SERVER.c_str(), MQTT_PORT);

  // ---DONE
  // TODO: add regex for these
  // if invalid, then turn to reserved topics smth like "gas-leak-detection/---/---/gas-level"
  MatchState msAddress;
  MatchState msDevice;
  msAddress.Target(const_cast<char*>(ADDRESS.c_str()));
  msDevice.Target(const_cast<char*>(HOSTNAME.c_str()));
  char matchAddress = msAddress.Match("^[0-9a-zA-Z,. -]+$");
  char matchDevice = msDevice.Match("^[0-9a-zA-Z,. -]+$");
  if (matchAddress > 0) {
    Serial.println("Address is valid");
  }
  else {
    Serial.println("Address is not valid");
    ADDRESS = "---";
  }
  if (matchDevice > 0) {
    Serial.println("Device is valid");
  }
  else {
    Serial.println("Device is not valid");
    HOSTNAME = "---";
  }

  GAS_TOPIC += ADDRESS + "/" + HOSTNAME + "/gas-level";
  TEST_TOPIC += ADDRESS + "/" + HOSTNAME + "/test";

  mqttClient.setCallback(recievedCallback);
}

void mqttConnect() {
  Serial.println("MQTT connecting...");
  
  if (mqttClient.connect(HOSTNAME.c_str(), MQTT_USER_, MQTT_PASSWORD_)) {
    Serial.println("connected");

    mqttClient.subscribe(TEST_TOPIC.c_str());
  }
  else {
    Serial.print("failed, status code = ");
    Serial.println(mqttClient.state());
    Serial.println("try again in 5 seconds");
    
    delay(5000);
  }
}

void not_found(AsyncWebServerRequest * request) {
  request -> send(404, "text/plain", "Not found");
}

void html_page_configure() {
  // Send web page with input fields to client
  WebServer.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request -> send_P(200, "text/html", INDEX_HTML_CONFIGURE);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  WebServer.on("/get", HTTP_GET, [](AsyncWebServerRequest * request) {
    String inputParam;
    if (request -> hasParam("network_name")) {
      WIFI_SSID_CLIENT = request -> getParam("network_name") -> value();
      WIFI_PASS_CLIENT = request -> getParam("network_password") -> value();
      HOSTNAME = request -> getParam("device_name") -> value();
      ADDRESS = request -> getParam("address") -> value();
      inputParam = "Parameters were uploaded successfully, access device in your network by name: \"" + HOSTNAME + "\"";
    } else {
      inputParam = "none";
    }
    Serial.println(WIFI_SSID_CLIENT + " " + WIFI_PASS_CLIENT);
    request -> send(200, "text/html", "HTTP GET request sent to your ESP. " +
      inputParam + ", with value: " +  WIFI_SSID_CLIENT +
      "<br><a href=\"/\">Return to Home Page</a></br>" +
      "<br>If you are still able to return to Home Page, " +
      "then your wifi credentials are invalid. Please check and try again</br>");
  });
  
  WebServer.onNotFound(not_found);
  WebServer.begin();
}

void network_configure() {
  setup_wifi_server();
  html_page_configure();
  while (!CONNECTION_STATUS) {
    while (true) {
      if (WIFI_SSID_CLIENT != "" and WIFI_PASS_CLIENT != "") {
        break;
      }
    }
    setup_wifi_client();
  }
  setup_mqtt_client();
}

void setup() {
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Gas Leakage");
  lcd.setCursor(0, 1);
  lcd.print("IoT Platform!");

  Serial.begin(115200);
  Serial.println("Warming up the MQ2 sensor");

  network_configure();

  delay(20000);  // wait for the MQ2 to warm up
  Serial.println("Warmed up");

  pinMode(BUZZER_PIN, OUTPUT); // buzzer
}

void loop() {
  if (!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();

  long now = millis();
  if (now - lastCheck > cooldown) {
    lastCheck = now;

    // TODO: add json, add publish
    int gasValue = analogRead(MQ2_AO_PIN);
    int gasLeakage = digitalRead(MQ2_DO_PIN);

    Serial.print("MQ2 sensor AO value: ");
    Serial.println(gasValue);
    Serial.print("MQ2 sensor DO value: ");
    Serial.println(gasLeakage);

    lcd.clear();
    lcd.setCursor(0, 1);
    if (gasValue >= THRESHOLD) {
      lcd.print("Leakage Alert!");

      ledcAttachPin(BUZZER_PIN, 0); // pin, channel
      ledcWriteNote(0, NOTE_F, 4); // channel, frequency, octave
      //tone(BUZZER_PIN, BUZZER_FREQUENCY);
      neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS, 0, 0);
    }
    else {
      lcd.clear();

      ledcDetachPin(BUZZER_PIN);
      //noTone(BUZZER_PIN);
      neopixelWrite(RGB_BUILTIN, 0, 0, 0);
    }
    lcd.setCursor(0, 0);
    lcd.print("Gas Level: ");
    lcd.print(gasValue);
  }
}
