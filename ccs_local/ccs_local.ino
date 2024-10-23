#include "LoRa_E22.h"
#include <HardwareSerial.h>

#define M0 32 //3in1 PCB mizde pin 7
#define M1 33 //3in1 PCB mizde pin 6
#define RX 27 //3in1 PCB mizde pin RX
#define TX 35  //3in1 PCB mizde pin TX

HardwareSerial fixajSerial(2);                            //Serial ikiyi seçiyoruz.
LoRa_E22 e22(TX, RX, &fixajSerial, UART_BPS_RATE_9600);

typedef struct{
  char command[200];
}CCS_MANUEL_COMMAND;

CCS_MANUEL_COMMAND manuel_command;

boolean receive = false;

String UART_DATA = "";
byte UART_LENGTH = 0;
boolean UART_COMPLETE = false;
byte indis = 0;
boolean transmit = false;

typedef struct
{
  bool sarj;
  bool yuk;
  bool engel;
  double agirlik;
}LFR_Datas;

LFR_Datas LFR_Data;

void LoRa_Listen(void);

void setup() {
  Serial.begin(9600); // RX (27), TX (35) pinleri için Serial2'yi başlat  
  fixajSerial.begin(9600, SERIAL_8N1, RX, TX); // RX ve TX pinlerini Serial2 olarak başlat
  
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  digitalWrite(M0, LOW);
  digitalWrite(M1, 0);
 
  delay(500);
  e22.begin();
  delay(500);
}

void loop() {
  LoRa_Listen();

  if(receive == true){
    Serial.print(String(manuel_command.command) + "\n"); // Serial2 üzerinden veri gönder
    fixajSerial.print(String(manuel_command.command) + "\n"); // Serial2 üzerinden veri gönder
      
    receive = false;
  }

  UART_Listen();
  LoRa_Send();
}

void LoRa_Listen(){
  while (e22.available()  > 0) {
    // Gelen mesaj okunuyor
    ResponseStructContainer rsc = e22.receiveMessage(sizeof(CCS_MANUEL_COMMAND));
    manuel_command = *(CCS_MANUEL_COMMAND*) rsc.data;

    rsc.close();
    receive = true;
  }
}

void UART_Listen(){
  while (Serial.available() > 0){ // UART_BUFFER = 2000,150,150,00111100,\n 
    char ch = Serial.read();
    if(ch != '\n'){
      UART_DATA += ch;
      UART_LENGTH++;
    }
    else{
      UART_COMPLETE = true;
    }
  }
  
  if(UART_COMPLETE){  // UART_DATA = 1,1,1,100.0,
    String str = "";
    for (int i = 0; i < UART_LENGTH; i++){
      char ch = UART_DATA[i];

      if(ch != ','){
        str += ch;
      }


      else{
        if(indis == 0)
          LFR_Data.sarj = str.toInt();
        else if(indis == 1)
          LFR_Data.yuk = str.toInt();
        else if(indis == 2)
          LFR_Data.engel = str.toInt();
        else if(indis == 3)
          LFR_Data.agirlik = str.toInt();
        indis++;
        str = "";
      }
      
    if(indis >= 4)
      break;
    }
    //Serial.println(String(UART_DATA));
    

    UART_DATA = "";
    UART_LENGTH = 0;
    UART_COMPLETE = false;
    transmit = true;
    indis=0;
  }
}

void LoRa_Send(){
  if(transmit){
    ResponseStatus rs = e22.sendFixedMessage(0, 2, 18, &LFR_Data, sizeof(LFR_Datas));
    Serial.println(rs.getResponseDescription());
    transmit = false;
    Serial.println(String(LFR_Data.sarj));
  }
}