/*********
  Author: Tran Tuan Anh
  Date: 7/29/2021
*********/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>
#include <ThingSpeak.h>

#define DHTTYPE DHT11
#define load1 GPIO_NUM_25 // define pin connect to control load 
#define load2 GPIO_NUM_26
#define DHTPIN 4     // Digital pin connected to the DHT sensor

AsyncWebServer server(80);

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "VNPT 2.4G";
const char* password = "hoilamgi";

const char* PARAM_TEMP = "inputTemp";
const char* PARAM_HUMID = "inputHumid";
unsigned long myChannelNumber = 1;
const char * myWriteAPIKey = "PTXT65EGKSUIP6QJ";
WiFiClient  client;
// Set Static IP address
IPAddress local_IP(192, 168, 1, 99);
// Set Gateway IP address
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;
byte degree_symbol[8] =
{
  0b00111,
  0b00101,
  0b00111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};
// HTML web page to handle 2 input fields (Temp, Humid)and show the result from the sensor
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
  <title>ESP32 Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css"
    integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
</head>
<style>
  html {
    font-family: Arial;
    display: inline-block;
    margin: 0px auto;
    text-align: center;
  }

  h2 {
    font-size: 3.0rem;
  }

  p {
    font-size: 3.0rem;
  }

  .units {
    font-size: 1.2rem;
  }

  .dht-labels {
    font-size: 1.5rem;
    vertical-align: middle;
    padding-bottom: 15px;
  }
</style>

<body>
  <h2>ESP32 DHT11 Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i>
    <span class="dht-labels">Temperature</span>
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i>
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">%</sup>
  </p>
  <script>
    function submitMessage() {
      alert("Saved value to ESP SPI Flash File System");
      setTimeout(function () { document.location.reload(false); }, 500);
    }
    setInterval(function () {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
          document.getElementById("temperature").innerHTML = this.responseText;
        }
      };
      xhttp.open("GET", "/temperature", true);
      xhttp.send();
    }, 10000);

    setInterval(function () {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
          document.getElementById("humidity").innerHTML = this.responseText;
        }
      };
      xhttp.open("GET", "/humidity", true);
      xhttp.send();
    }, 10000);
  </script>

  <form action="/get" target="hidden-form">
    Limit Temperature: <input type="text" name="inputTemp">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form><br>
  <form action="/get" target="hidden-form">
    Limit Humidity: <input type="text " name="inputHumid">
    <input type="submit" value="Submit" onclick="submitMessage()">
  </form>
<iframe style="display:none" name="hidden-form"></iframe><br>
</body>

</html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {

    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}

// Replaces placeholder with stored values
String processor_get(const String& var) {
  //Serial.println(var);
  if (var == "inputTemp") {
    return readFile(SPIFFS, "/inputTemp.txt");
  }
  else if (var == "inputHumid") {
    return readFile(SPIFFS, "/inputHumid.txt");
  }
  return String();
}

String processor_post(const String& var) {
  if (var == "TEMPERATURE") {
    return String(t);
  }
  else if (var == "HUMIDITY") {
    return String(h);
  }
  return String();
}

void setup() {
  pinMode(load1, OUTPUT);
  pinMode(load2, OUTPUT);
  Serial.begin(115200);
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degree_symbol);
  if (!WiFi.config(local_IP, gateway, subnet)) 
  {
    Serial.println("STA Failed to configure");
  }
  WiFi.mode(WIFI_STA);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect");
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      delay(5000);
    }

    Serial.println("\nConnected.");
  }
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  ThingSpeak.begin(client);
  dht.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {

    // Connect or reconnect to WiFi

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send_P(200, "text/html", index_html, processor_post);
    });
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send_P(200, "text/plain", String(t).c_str());
    });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send_P(200, "text/plain", String(h).c_str());
    });

    // Send web page with input fields to client
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send_P(200, "text/html", index_html, processor_get);
    });

    // Send a HTTP_GET request to <ESP_IP>/get?inputString=<inputMessage>
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
      String inputMessage;
      // GET inputTemp value on <ESP_IP>/get?inputString=<inputMessage>
      if (request->hasParam(PARAM_TEMP)) {
        inputMessage = request->getParam(PARAM_TEMP)->value();
        writeFile(SPIFFS, "/inputTemp.txt", inputMessage.c_str());
      }
      // GET inputInt value on <ESP_IP>/get?inputInt=<inputMessage>
      else if (request->hasParam(PARAM_HUMID)) {
        inputMessage = request->getParam(PARAM_HUMID)->value();
        writeFile(SPIFFS, "/inputHumid.txt", inputMessage.c_str());
      }
      else {
        inputMessage = "No message sent";
      }
      Serial.println(inputMessage);
      request->send(200, "text/text", inputMessage);
    });
    server.onNotFound(notFound);
    server.begin();

    // Wait a few seconds between measurements.
    delay(2000);

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    t = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    //print the result via LCD
    lcd.setCursor(0, 0);
    lcd.print("Temp: " + String(t));
    lcd.write(0);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Humid: " + String(h) + " %");
    //set field to upload data
    ThingSpeak.setField(1, t);
    ThingSpeak.setField(2, h);

    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200) {
      Serial.println("Channel update successful.");
    }
    else {
      Serial.println("Problem updating channel. HTTP error code " + String(x));//if not post successfully
    }
    // To access your stored values on inputs
    float limitTemp = readFile(SPIFFS, "/inputTemp.txt").toFloat();
    int limitHumid = readFile(SPIFFS, "/inputHumid.txt").toInt();
    if (t < limitTemp || h < limitHumid)
    {
      digitalWrite(load1, HIGH);
    }
    else if (t > limitTemp || h > limitHumid)
    {
      digitalWrite(load1, LOW);
    }
    lastTime = millis();
  }
}
