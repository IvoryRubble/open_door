#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

WebServer server(80);

const int sketchId = 3428;
const int maxStringLength = 30;

const int ledPin = 8;
unsigned long blinkLowPeriod = 250;
unsigned long blinkHighPeriod = 250;

const int doorButtonPin = 7;
const unsigned long doorButtonPressTime = 1500;

const unsigned long readNewWiFiCredsTimeout = 5000;

String getMainPage() {
  return R"raw(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>Door</title>
  <style>
    body {
      display: flex;
      flex-direction: column;
      height: 100vh;
      margin: 0;
      align-items: center;
      justify-content: center;
      background-color: #f0f0f0;
      font-family: Arial, sans-serif;
    }

    button {
      position: relative;
      padding: 2rem 4rem;
      font-size: 2rem;
      border: none;
      border-radius: 10px;
      background-color: #007bff;
      color: white;
      cursor: pointer;
      box-shadow: 0 5px 15px rgba(0, 0, 0, 0.2);
      transition: transform 0.1s ease, background-color 0.3s ease;
      min-width: 250px;
    }

    button:hover:enabled {
      background-color: #0056b3;
    }

    button:active:enabled {
      transform: scale(0.98);
    }

    button:disabled {
      background-color: #999;
      cursor: not-allowed;
    }

    .spinner {
      position: absolute;
      top: 50%;
      left: 50%;
      width: 24px;
      height: 24px;
      margin: -12px 0 0 -12px;
      border: 4px solid white;
      border-top: 4px solid transparent;
      border-radius: 50%;
      animation: spin 1s linear infinite;
      display: none;
    }

    .loading .spinner {
      display: block;
    }

    .loading .btn-text {
      visibility: hidden;
    }

    @keyframes spin {
      to { transform: rotate(360deg); }
    }

    #errorMessage {
      margin-top: 1rem;
      color: red;
      font-size: 1rem;
      text-align: center;
    }
  </style>
</head>
<body>
  <button id="toggleButton" onclick="openDoor()">
    <span class="btn-text">Open door</span>
    <div class="spinner"></div>
  </button>
  <div id="errorMessage"></div>

  <script>
    function openDoor() {
      const button = document.getElementById('toggleButton');
      const errorDiv = document.getElementById('errorMessage');

      errorDiv.textContent = '';
      button.classList.add('loading');
      button.disabled = true;

      fetch('/openDoor')
        .then(response => {
          if (!response.ok) throw new Error(response.status + ', ' + response.statusText);
          return response.text();
        })
        .then(data => {
          console.log('Response: "', data + '"');
        })
        .catch(error => {
          //alert('Request failed: ' + error)
          console.error('Request failed:', error);
          errorDiv.textContent = 'Request failed: ' + error;
        })
        .finally(() => {
          button.classList.remove('loading');
          button.disabled = false;
        });
    }
  </script>
</body>
</html>
)raw";
}

void handleRoot() {
  server.send(200, "text/html", getMainPage());
}

void handleOpenDoor() {
  digitalWrite(doorButtonPin, HIGH);
  digitalWrite(ledPin, HIGH);
  delay(doorButtonPressTime);
  digitalWrite(doorButtonPin, LOW);
  digitalWrite(ledPin, LOW);
  Serial.println("Pressed door button");
  server.send(200);  
}

void blinkLed() {
  if ((blinkHighPeriod + blinkLowPeriod) > 0 && millis() % (blinkHighPeriod + blinkLowPeriod) < blinkLowPeriod) {
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, HIGH);
  }
}

void waitWiFiConnection() {
  blinkLowPeriod = 250;
  blinkHighPeriod = 250;
  unsigned long previousTimePrintConnectionProgress = 0; 
  while (WiFi.status() != WL_CONNECTED) {
    blinkLed();
    if (millis() - previousTimePrintConnectionProgress > 250) {
      Serial.print(".");
      previousTimePrintConnectionProgress = millis();
    }
  }
  digitalWrite(ledPin, LOW);
  Serial.println();

  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

int writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  int length = strToWrite.length();
  if (length > maxStringLength) length = maxStringLength;

  EEPROM.write(addrOffset, length);
  for (int i = 0; i < length; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  return addrOffset + 1 + length;
}

int readStringFromEEPROM(int addrOffset, String &strToRead)
{
  int length = EEPROM.read(addrOffset);
  if (length > maxStringLength) length = maxStringLength;

  char data[length + 1];
  for (int i = 0; i < length; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[length] = '\0';
  strToRead = String(data);
  return addrOffset + 1 + length;
}

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(doorButtonPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  digitalWrite(doorButtonPin, LOW);
  
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println();
  Serial.println("Please stand by...");
  Serial.println();
  delay(1000);

  EEPROM.begin(512);
  int sketchIdRead; 
  EEPROM.get(0, sketchIdRead);
  bool isSavedDataFound;
  if (sketchIdRead == sketchId) {
    isSavedDataFound = true;
  } else {
    isSavedDataFound = false;
  }

  String ssid;
  String password;
  if (isSavedDataFound) {
    int nextOffset = readStringFromEEPROM(sizeof(sketchId), ssid);
    readStringFromEEPROM(nextOffset, password);
    Serial.print("Found saved WiFi SSID: "); Serial.println(ssid);
    // Serial.print("password: "); Serial.println(password);
  } else {
    Serial.println("No saved WiFi creds found");
  }
  Serial.println();

  Serial.println("Enter new WiFi SSID:");
  const unsigned long startCheckInputTime = millis();
  while ((!isSavedDataFound || (millis() - startCheckInputTime) < readNewWiFiCredsTimeout) && !Serial.available()) { }
  if (Serial.available()) {
    ssid = Serial.readString();
    ssid.trim();
    ssid.remove(maxStringLength);
    Serial.print("New SSID: "); Serial.println(ssid);
    Serial.println("Enter new WiFi password:");
    while (!Serial.available()) {}
    password = Serial.readString();
    password.trim();
    password.remove(maxStringLength);
    Serial.print("New password: "); Serial.println(password);
    EEPROM.put(0, sketchId);
    int nextOffset = writeStringToEEPROM(sizeof(sketchId), ssid);
    writeStringToEEPROM(nextOffset, password);
    EEPROM.commit();
  } else {
    Serial.println("No input detected. Use saved WiFi creds");
  }
  EEPROM.end();
  Serial.println();

  WiFi.begin(ssid, password);

  // Bad antenna design workaround:
  // https://forum.arduino.cc/t/no-wifi-connect-with-esp32-c3-super-mini/1324046/12
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  Serial.print("MAC address: "); Serial.println(WiFi.macAddress());

  Serial.print("Connecting to WiFi");
  waitWiFiConnection();

  server.on("/", handleRoot);
  server.on("/openDoor", HTTP_GET, handleOpenDoor);

  server.begin();
  Serial.println("Started HTTP server");
}

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting..");
    waitWiFiConnection();
  }
  
  blinkLowPeriod = 3000;
  blinkHighPeriod = 100;
  blinkLed();
  //delay(2000);
}
