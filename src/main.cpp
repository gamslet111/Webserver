/******************************************************************

                        LoLin32 htmlMin
                                                қuran  march 2025
******************************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include "SPIFFS.h"

#define TRUE                            true
#define FALSE                           false
#define H                               HIGH
#define L                               LOW

#define LEN                             20  // Stings

#define ON_BOARD_LED                    5

#define EEPROM_SIZE                      100
#define EEPROM_ADDR                      0
#define EEPROM_SSID_ADDR                 0
#define EEPROM_PASSWORD_ADDR             40
#define EEPROM_MOTOR_SYS_ADDR            80

const int led5 = 5;
int ledState = L;

String receivedText = ""; // Variable zum Speichern der empfangenen Daten
String ssidFromEEPROM = "";
String passwordFromEEPROM = "";
String motorSysFromEEPROM = "";
String receivedWord = "";
volatile int startWiFi = 0;




AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void initSPIFFS();
String readFromEEPROM(int address);
void store2EEPROM(String word, int address);

void initWiFi();

void initWebSocket();
String processor(const String& var);
void notifyClients(String state);
void handleWebSocketMessage(void *arg, uint8_t * data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient * client, AwsEventType type, 
    void * arg, uint8_t * data, size_t len);

void readUart();


void setup() 
{
    Serial.begin(115200);
    printf("start!");

    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("EEPROM initialisieren fehlgeschlagen!");
        return;
    }

    //EEPROM.write(EEPROM_ADDR, dataToWrite);
    //EEPROM.commit();  // Änderungen speichern
    //dataToRead = 0;
    //dataToRead = EEPROM.read(EEPROM_ADDR);
    //Serial.println("Daten gelesen: \n" + String(dataToRead));

    ssidFromEEPROM = readFromEEPROM(EEPROM_SSID_ADDR);
    printf("im EEPROM gefunden: .%s.\n", ssidFromEEPROM);
    passwordFromEEPROM = readFromEEPROM(EEPROM_PASSWORD_ADDR);
    printf("im PASSWOR gefunden: .%s.\n", passwordFromEEPROM);

    // use ProcessingConfig to store new eeprom values

    pinMode(led5, OUTPUT);

    digitalWrite(led5, L); // inverse logic!

    initSPIFFS();
    initWiFi();
    initWebSocket();


    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_GET,
    [](AsyncWebServerRequest * request)
    {
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });

    server.on("/logo", HTTP_GET, 
    [](AsyncWebServerRequest *request)
    {
        request->send(SPIFFS, "/logo.png", "image/png");
    });

    
    server.begin();

}

void loop() 
{

    if (ledState == H)  digitalWrite(led5, LOW); 
    else digitalWrite(led5, HIGH); // inverse logic!

    readUart();





}

void initSPIFFS()
{
    if (!SPIFFS.begin(true))
    {
        printf("An error has occurred while mounting SPIFFS\n");
    }
    else
    {
        printf("SPIFFS mounted successfully!\n");
    }
}

void initWiFi()
{
    char text[LEN];
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidFromEEPROM, passwordFromEEPROM);

    printf("Connection to WiFi . . .");
    while ((WiFi.status() != WL_CONNECTED) && (startWiFi < 12))  // 12 instead of 21... 
    {
        delay(1000);
        printf(" .");
        startWiFi++;
    }

    //Serial.println("IP:");
    //Serial.println(WiFi.localIP());

    uint32_t ip = (uint32_t) WiFi.localIP();
    sprintf(text, "%u.%u.%u.%u", ip & 0xFF, (ip>>8) & 0xFF, (ip>>16) & 0xFF, (ip>>24) & 0xFF );
    printf("\nIP: %s\n", text);
}

void initWebSocket()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}


void store2EEPROM(String word, int address)
{
    int i;

    for (i = 0; i < word.length(); i++)
    {
        EEPROM.write(address + i, word[i]);
    }
    EEPROM.write(address + word.length(), '\0');    
    EEPROM.commit();  // Änderungen speichern
}

String readFromEEPROM(int address)
{
  String word = "";
  char c;
  while ((c = EEPROM.read(address++)) != '\0') 
  { // Lies Zeichen bis zur Null-Terminierung
    word += c;
  }
  return word;
}

void readUart(void)
{
    int startIndex, endIndex;

    while (Serial.available() > 0) 
    {
        char receivedChar = Serial.read(); // Einzelnes Zeichen lesen

        if (receivedChar == '\n') 
        {
        // Wenn ein Zeilenumbruch empfangen wird, Ausgabe und Text zurücksetzen
           Serial.println("Empfangene Daten: " + receivedText); // Daten ausgeben
           receivedText = ""; // Textfeld zurücksetzen
        } 
        else 
        {
           receivedText += receivedChar; // neues Zeichen an das Textfeld anhängen
           receivedText.trim(); // Leerzeichen am Anfang und Ende entfernen
           
           startIndex = 0; 
         
           // Auswerten: 
           startIndex = receivedText.indexOf("\"ssid\":\"");
           //"ssid":"   das sind 8 Zeichen!  - gesendet wurde zB: "ssid":"meinSSIDWort"
           //                                                     12345678  
           if (startIndex != -1)
           {
               startIndex += 8;
               endIndex = receivedText.indexOf("\"", startIndex);
               
               if (endIndex != -1)
               {
                   receivedWord = receivedText.substring(startIndex, endIndex);
                   printf("\nUserWort erkannt .%s.", receivedWord);
                   store2EEPROM(receivedWord, EEPROM_SSID_ADDR);
               }
            }

            startIndex = receivedText.indexOf("\"password\":\"");
            //"password":"   das sind 12 Zeichen!
            
            if (startIndex != -1)
            {
                startIndex += 12;
                endIndex = receivedText.indexOf("\"", startIndex);
                if (endIndex != -1)
                {
                    receivedWord = receivedText.substring(startIndex, endIndex);
                    printf("\nPassword erkannt .%s.", receivedWord);
                    store2EEPROM(receivedWord, EEPROM_PASSWORD_ADDR);
                }
            }
            
            if (receivedText == "RESET") 
            {
                Serial.println("Rebooting...");
                ESP.restart(); // Neustart des ESP32
            }
        }
    }
}


String processor(const String& var)
{
    if (var == "STATE") 
    {
        if (digitalRead(led5)) // hier sollte man vielleicht go verwenden 
        {
            ledState = 1; 
            printf("--on--\n");
            return "--ON--";
        }
        else
        {
            ledState = 0; 
            printf("--off--\n");
            return "--OFF--";
        }
    }

    if (var == "CHECK")
    {
        printf("check ...\n");
        return "ok!";
    }

    return String();
}

void notifyClients(String state)
{
    ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t * data, size_t len)
{
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len && info->opcode == WS_TEXT)
    {
        data[len] = 0;

        if (strcmp((char*)data, "bON") == 0)
        {
            ledState = H;
            notifyClients("ON!!!");

            printf("handleWebSocketMessage: on\n");

        }
        if (strcmp((char*)data, "bOFF") == 0)
        {
            ledState = L;
            notifyClients("OFF");
            printf("handleWebSocketMessage: off\n");
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient * client, AwsEventType type, 
             void * arg, uint8_t * data, size_t len)
{
    switch(type)
    {
        case WS_EVT_CONNECT: 
             printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;

        case WS_EVT_DISCONNECT:
             printf("WebSocket client #%u disconnected\n", client->id());
        break;

        case WS_EVT_DATA:
             handleWebSocketMessage(arg, data, len);
        break;

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
        break;
    }
}

