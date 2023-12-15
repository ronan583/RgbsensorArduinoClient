#include "arduino_secrets.h"
/*!
 * @file scanI2C.ino
 * @brief Connect I2C devices to I2C Multiplexer, and connect I2C Multiplexer to Arduino, 
 * @n     then download this example open serial monitor to check the I2C addr.
 * @detail  I2C address selection
 * @n       A0  A1  A2  I2C_addr
 * @n       0   0   0   0x70(default)
 * @n       0   0   1   0x71
 * @n       0   1   0   0x72
 * @n       0   1   1   0x73
 * @n       1   0   0   0x74
 * @n       1   0   1   0x75
 * @n       1   1   0   0x76
 * @n       1   1   1   0x77
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author      PengKaixing(kaixing.peng@dfrobot.com)
 * @version  V1.0.0
 * @date  2022-03-23
 * @url https://github.com/DFRobot/DFRobot_I2C_Multiplexer
 */
#include <DFRobot_I2C_Multiplexer.h>
#include <DFRobot_TCS3430.h>
#include <ArduinoHttpClient.h>
#include <WiFiNINA.h>
#include "config.h"

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
/////// WiFi Settings ///////
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

char serverAddress[] = SERVER_IP;  // server address
int serverPort = SERVER_PORT;

WiFiClient wifi;
WebSocketClient client = WebSocketClient(wifi, serverAddress, serverPort);
int status = WL_IDLE_STATUS;

/*Create an I2C Multiplexer object, the address of I2C Multiplexer is 0x70*/
DFRobot_I2C_Multiplexer I2CMultiplexer(&Wire, 0x70);
DFRobot_TCS3430 TCS3430;
uint8_t reg = 0;

String adcInput = "0";
int aPin0 = A0;  // select the input pin for the potentiometer
int aPin1 = A1;
int aPin2 = A2;
int aPin3 = A3;
int aValue0 = 0;  // variable to store the value coming from the sensor
int aValue1 = 0;
int aValue2 = 0;
int aValue3 = 0;
int aValues[4];
int RESOLUTION = ANALOG_RESOLUTION;  //resolution in bits
int maxAnalogValue = 4095;
int highThreshold = HIGH_THRESHOLD;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  initBegin();
  connectToWifi();
  beginI2C();
  beginClient();
}

void initBegin() {
  Serial.begin(9600);
  analogReadResolution(RESOLUTION);
}

void connectToWifi() {
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    status = WiFi.begin(ssid, pass);
    delay(1000);
  }
  printWifiInfo();
}

void printWifiInfo() {
  Serial.print("Connecting success, SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
}

void beginI2C() {
  I2CMultiplexer.begin();
  delay(500);
  Serial.println("Ready to scan i2c device.");
  uint8_t buf[3] = { 0 };
  /*Print I2C device of each port*/
  for (uint8_t port = 0; port < 8; port++) {
    I2CMultiplexer.selectPort(port);
    delay(500);
    if (!TCS3430.begin()) {
      Serial.println("Please check that the IIC device on port " + String(port) + " is properly connected");
      continue;
    }
    Serial.print("Port:");
    Serial.print(port);
    Serial.println(" is ready.");
    uint8_t *dev = I2CMultiplexer.scan(port);
    while (*dev) {
      Serial.print("  i2c addr: ");
      Serial.print(*dev);
      Serial.print(", read i2c: ");
      Serial.println(I2CMultiplexer.read(port, *dev, reg, buf, 3));
      Serial.print("  BUF: ");
      Serial.print((int)buf[0], HEX);
      Serial.print(",");
      Serial.print((int)buf[1], HEX);
      Serial.print(",");
      Serial.println((int)buf[2], HEX);
      dev++;
    }
    Serial.println();
  }
}

void beginClient() {
  Serial.println("Start Websocket client.");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(200);
  client.begin();
}

void closeClient() {
  Serial.println("Stop Websocket client.");
  client.stop();
}

void loop() {
  // oldLoop();
  reconnectWifi();
  // needs to be "xxxx", x= 0 or 1
  adcInput = readAnalogInput();
  if (adcInput == "0000") {
    Serial.println("Halt!");
    if (client.connected()) {
      closeClient();
    }
    delay(1000);
  } else {
    if (client.connected()) {
      Serial.println("Read sensors data");
      String portsData = readPortsData();
      sendData(portsData);
    } else {
      Serial.println("disconnected");
      beginClient();
    }
  }
}

void reconnectWifi() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Reconnecting to Network named: ");
    Serial.println(ssid);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    status = WiFi.begin(ssid, pass);
    delay(1000);
  }
}

String readAnalogInput() {
  char input[5];
  aValues[0] = analogRead(aPin0);
  aValues[1] = analogRead(aPin1);
  aValues[2] = analogRead(aPin2);
  aValues[3] = analogRead(aPin3);
  for (int i = 0; i < 4; i++) {
    Serial.println("analog " + String(i) + " reads " + String(aValues[i]));
    if (aValues[i] < highThreshold) {
      input[i] = '0';
    } else {
      input[i] = '1';
    }
  }
  input[4] = '\0';
  Serial.println("Analog inputs are" + String(input));
  return String(input);
}

void sendData(String data) {
  client.beginMessage(TYPE_TEXT);
  client.print(data);
  client.endMessage();
  // check if a message is available to be received
  int messageSize = client.parseMessage();
  if (messageSize > 0) {
    Serial.println("Received a message:");
    Serial.println(client.readString());
  }
}
String readPortsData() {
  String jsonData = "{\"Sens\":[";
  for (uint8_t port = 0; port < 8; port++) {
    I2CMultiplexer.selectPort(port);
    // delay to wait for port selected
    delay(100);
    String sensorData = readSensor(port);
    Serial.println(sensorData);
    jsonData += "{\"id\":" + String(port) + ",\"d\":" + sensorData + "}";
    if (port < 7) {
      jsonData += ",";
    }
  }
  jsonData += "],\"adc\":\"" + String(adcInput) + "\"";
  jsonData += ",\"A0\":\"" + String(aValues[0]) + "\"";
  jsonData += ",\"A1\":\"" + String(aValues[1]) + "\"";
  jsonData += ",\"A2\":\"" + String(aValues[2]) + "\"";
  jsonData += ",\"A3\":\"" + String(aValues[3]) + "\"";
  jsonData += ",\"ssId\":\"" + String(ssid) + "\"}";
  Serial.println(jsonData);
  Serial.println(jsonData.length());
  return jsonData;
}

String readSensor(uint8_t port) {
  String jsonStr;
  if (TCS3430.begin()) {
    ///0: 1x gain, 1: 4x gain, 2: 16x, 3: 64x
    TCS3430.setALSGain(0);
    // Integration Time calculation: 0x40 * 2.78ms = 177.92ms
    TCS3430.setIntegrationTime(0x40);   
    // delay value needs to be larger than integration time
    delay(250);   
    uint16_t XData = TCS3430.getXData();
    uint16_t YData = TCS3430.getYData();
    uint16_t ZData = TCS3430.getZData();
    uint16_t IR1Data = TCS3430.getIR1Data();
    uint16_t IR2Data = TCS3430.getIR2Data();
    jsonStr = createJson(port, XData, YData, ZData, IR1Data, IR2Data);
  } else {
    jsonStr = "null";
  }
  return jsonStr;
}

String createJson(uint8_t port, uint16_t Xdata, uint16_t Ydata, uint16_t Zdata, uint16_t IR1data, uint16_t IR2data) {
  String json = "{";
  // json += "\"Sensor\": " + String(port) + ",";
  json += "\"X\": " + String(Xdata) + ",";
  json += "\"Y\": " + String(Ydata) + ",";
  json += "\"Z\": " + String(Zdata) + ",";
  json += "\"I1\": " + String(IR1data) + ",";
  json += "\"I2\": " + String(IR2data);
  json += "}";
  return json;
}
