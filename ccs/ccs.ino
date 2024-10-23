#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "LoRa_E22.h"
#include <HardwareSerial.h>

// Replace the next variables with your SSID/Password combination
const char* ssid = "OTONOM SISTEMLER LABORATUVARI";
const char* password = "16megabit";

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.0.150";

WiFiClient espClient;
PubSubClient client(espClient);

#define M0 32 //3in1 PCB mizde pin 7
#define M1 33 //3in1 PCB mizde pin 6
#define RX 27 //3in1 PCB mizde pin RX
#define TX 35  //3in1 PCB mizde pin TX
 
HardwareSerial fixajSerial(1);                            //Serial biri seçiyoruz.
LoRa_E22 e22(TX, RX, &fixajSerial, UART_BPS_RATE_9600);

typedef struct{
  char command[200];
}CCS_MANUEL_COMMAND;

CCS_MANUEL_COMMAND manuel_command;

#define TOPIC "durum"
boolean receive = false;

typedef struct
{
  bool sarj;
  bool yuk;
  bool engel;
  double agirlik;
}LFR_Datas;

LFR_Datas LFR_Data;


void setup() {
  Serial.begin(9600);
  WiFi_Connect();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  MQTT_Connect();
  
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, 0);
 
  delay(500);
  e22.begin();
  delay(500);
}

void loop() {
  if (!client.connected()) {
    MQTT_Connect();
  }
  client.loop();

  LoRa_Listen(); // loaradan veri alma
  MQTT_Send(); // loradan alınan veriyi brockera iletme
}

void WiFi_Connect(){
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void MQTT_Connect(){
   while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("rota");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

String filterCommands(const String& jsonString) {
  // JSON verisini ayrıştır
  StaticJsonDocument<512> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, jsonString);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return "[]"; // Boş JSON Array döndür
  }

  // JSON Array'yi elde et
  JsonArray array = jsonDoc.as<JsonArray>();

  // Filtrelenmiş komutları saklayacak String
  String filteredCommands = "[";

  // JSON Array'deki elemanları kontrol et
  bool firstElement = true;
  for (JsonObject obj : array) {
    for (JsonPair pair : obj) {
      String key = pair.key().c_str();   // Key'i String'e dönüştür
      String value = pair.value().as<String>();

      // İkinci indeks "İleri" değilse ekle
      if (value != "İleri" && value != "ileri")   {
        if (!firstElement) {
          filteredCommands += ",";
        }
        filteredCommands += "{\"" + key + "\":\"" + value + "\"}";
        firstElement = false;
      }
    }
  }

  filteredCommands += "]";

  return filteredCommands;
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  boolean transmit = false;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  StaticJsonDocument <256> doc;
  deserializeJson(doc,message);

  String command = doc["rota"].as<String>();
  Serial.println("command: " + command);
  
  // JSON verisini filtreleme
  String filteredCommands = filterCommands(command);
  Serial.println("Filtered commands: " + filteredCommands);

  if (String(topic) == "rota") {
    
    // String'ten char array'e dönüştürme
    filteredCommands.toCharArray(manuel_command.command, sizeof(manuel_command.command));
    Serial.println(manuel_command.command);
    transmit = true;
    
    if(transmit == true){
      ResponseStatus rs = e22.sendFixedMessage(0, 2, 18, &manuel_command, sizeof(CCS_MANUEL_COMMAND));
      Serial.println(rs.getResponseDescription());
    }
  }
}

void LoRa_Listen(){
  while (e22.available()  > 0) {
    // Gelen mesaj okunuyor
    ResponseStructContainer rsc = e22.receiveMessage(sizeof(LFR_Datas));
    LFR_Data = *(LFR_Datas*) rsc.data;

    rsc.close();
    receive = true;
  }
}

void MQTT_Send(){
  if(receive){
    StaticJsonDocument<500> JSONbuffer;
    JSONbuffer["charge"] = LFR_Data.sarj;
    JSONbuffer["load"] = LFR_Data.yuk;
    JSONbuffer["barrier"] = LFR_Data.engel;
    JSONbuffer["loadLevel"] = LFR_Data.agirlik;

    

    char JSONmessageBuffer[256];
    int Json_Byte = serializeJson(JSONbuffer, JSONmessageBuffer);

    //PubSubClient.cpp 376 satır!
    if (client.publish(TOPIC, JSONmessageBuffer,true)){
      Serial.println("Success sending message");
      Serial.println(String(JSONmessageBuffer));
    } 
    else{
      Serial.println("Error sending message");
    }
    receive = false;
  }
}