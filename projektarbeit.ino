#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoMqttClient.h>

// Globale Variablen

char ssid[30], passwd[30], broker[30], dataSent[30];
int pinCount, port, countTopic, i = 0, j, valves[40], valvesPin[40];
bool establishedVALVES = false, establishedWIFI = false, establishedMQTT = false;
enum Betriebmodus { halt, manuell, automatisch};
String dataRecieved, topics[41];
long timeNow, timePrev;

// Objekte

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
Betriebmodus modus;

void setup() 
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 started");
  delay(500);
  createPins();
  establishConnectionWiFi();
  establishConnectionMQTT();
  modus = halt;
  Serial.println("Setup Completed!\n To see all commands type: help");
}

void loop() 
{
  if (Serial.available())
  {
   serialCheckAndSetMode();
   steuerung(); 
  }
  timeClient.update();
  mqttClient.poll();
  delay(500);
}

// Funktionen

void createPins()
{
  Serial.println("How many Valves?:");

  while (!establishedVALVES)
  {
    if (Serial.available())
    {
      while (Serial.available())
      {
        dataSent[i++] = Serial.read();
        delay(100);
      }
      dataSent[i-1] = '\0'; //Serial in Arduino
      pinCount = atoi(dataSent);
      Serial.println(pinCount);
      i = 0;
      establishedVALVES = true;

      for (j = 1; j <= pinCount; j++)
      {
        Serial.printf("Assign Pin (On Board) to current Pin (%2d)\n", j);
        
        while (!Serial.available())
          delay(250);

        while (Serial.available())
        {
          dataSent[i++] = Serial.read();
          delay(100);
        }
        dataSent[i-1] = '\0'; //Serial in Arduino
        valves[j] = atoi(dataSent);
        pinMode(valves[j], OUTPUT);
        i = 0;
        Serial.println("Assign topic to current Pin:");

        while (!Serial.available())
          delay(250);

        while (Serial.available())
        {
          dataSent[i++] = Serial.read();
          delay(100);
        }
        dataSent[i-1] = '\0'; //Serial in Arduino
        topics[j] = dataSent;
        i = 0;
        Serial.println(topics[j]);
      }
      topics[j] = "projektarbeit/main";
      Serial.println(topics[5]);
    }
  }  
}

void establishConnectionWiFi()
{
  Serial.println("SSID of your Network:");
  while (!establishedWIFI)
  {
    delay(100);
    if (Serial.available())
    {
      while (Serial.available())
      {
       ssid[i++] = Serial.read();
       delay(100); 
      } 
  
      ssid[i-1] = '\0';
      i = 0;
      Serial.println("Password:");
      
      while (!Serial.available())
        delay(100);
        
      while (Serial.available())
      {
       passwd[i++] = Serial.read();
       delay(100); 
      }

      passwd[i-1] = '\0';
      i = 0;
      establishedWIFI = true;
    }
  }
  delay(200);
  WiFi.begin(ssid, passwd);
  Serial.printf("\nConnecting to %s", ssid);

  while (WiFi.status() != WL_CONNECTED)
    delay(500);

  Serial.println("\nConnection succeed!");
  Serial.print("Local IP: ");
  Serial.print(WiFi.localIP());
}

void establishConnectionMQTT()
{
  Serial.println("\nIP Address or Domain of MQTT Server:");
  while (!establishedMQTT)
  {
    delay(100);
    if (Serial.available())
    {
      while (Serial.available())
      {
       broker[i++] = Serial.read();
       delay(100); 
      } 
      broker[i-1] = '\0';
      i = 0;
      Serial.println("Port:");

      while (!Serial.available())
        delay(100);
        
      while (Serial.available())
      {
        dataSent[i++] = Serial.read();
        delay(100);
      }
      dataSent[i-1] = '\0'; //Serial in Arduino
      i = 0;
      port = atoi(dataSent);
      establishedMQTT = true;
    }
  }
  
  timeClient.begin();
   if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("\nYou're connected to the MQTT broker!");
}

void checkMQTT()
{
  if (mqttClient.available())
  {
    while (mqttClient.available())
      dataRecieved[i++] = mqttClient.read();
  }
}

// Diese Funktion überprüft ob etwas über die Serielle Schnitstelle gesendet wurde.
// Wenn ja, dann speichert es die einzele Bits in CharArray und dann umwandelt CharArray in String um.

void serialCheckAndSetMode()
{
  while (Serial.available() > 0 && Serial.available() < 30)
    dataSent[i++] = Serial.read();

  dataSent[i-1] = '\0'; //Serial in Arduino
  //dataSent[i] = '\0';   // Serial in hterm 
  dataRecieved = dataSent;

  if (dataRecieved == "manual") 
    modus = manuell;
  else if (dataRecieved == "automatic")
    modus = automatisch;
  else if (dataRecieved == "stop")
  {
    WiFi.disconnect();
    modus = halt;   
  }
  else if (dataRecieved == "showip")
  {
   Serial.println(WiFi.localIP());
   mqttClient.beginMessage(topics[j]);
   Serial.println(topics[j]);
   mqttClient.print("hello");
   mqttClient.endMessage();
  }
  else if (dataRecieved == "time")
    Serial.println("Uhr: " + timeClient.getFormattedTime());
  else if (dataRecieved == "printalltopics")
    printAllTopics();  
  else if (dataRecieved == "help")
  {
    Serial.println("Commands:");
    Serial.println("stop - Disconnect from Network and close all Valves");
    Serial.println("manual - Switches to Manual Mode. In this Mode User can open/close specific Valve");
    Serial.println("automatic - Switches to Automatic Mode. In this Mode all Valves are being opened and closed frequently");
    Serial.println("showip - Shows current local IP");
    Serial.println("time - Shows current Time in 24H unit"); 
  }
    
  // Befehle für Debug Steuerung
  if (modus == manuell)
  {
    /*
    if (dataRecieved == "1 open")
      //stateV1 = offen;
    else if (dataRecieved == "1 close")
      //stateV1 = geschlossen;
    else if (dataRecieved == "2 open")
      //stateV2 = offen;
    else if (dataRecieved == "2 close")
      //stateV2 = geschlossen;
    else if (dataRecieved == "3 open")
      stateV3 = offen;
    else if (dataRecieved == "3 close")
      stateV3 = geschlossen;
    else if (dataRecieved == "4 open")
      stateV4 = offen;
    else if (dataRecieved == "4 close") 
      stateV4 = geschlossen;
      */
  }
  i = 0;
  dataRecieved = "";
}

void steuerung()
{
  switch (modus)
  {
    case manuell: // Manuelle Steuerung
      /*
      if (stateV1 == offen) 
      { 
        digitalWrite(V1, HIGH); 
        Serial.println("DEBUG (Zustand): 1 open");
      }
      else if (stateV1 == geschlossen) 
      { 
        digitalWrite(V1, LOW);
        Serial.println("DEBUG (Zustand): 1 closed");  
      }
      if (stateV2 == offen) 
      { 
        digitalWrite(V2, HIGH);
        Serial.println("DEBUG (Zustand): 2 open");
      }
      else if (stateV2 == geschlossen) 
      { 
        digitalWrite(V2, LOW);
        Serial.println("DEBUG (Zustand): 2 closed");
      }
      if (stateV3 == offen) 
        digitalWrite(V3, HIGH);
      else if (stateV3 == geschlossen) 
        digitalWrite(V3, LOW);
        
      if (stateV4 == offen) 
        digitalWrite(V4, HIGH);
      else if (stateV4 == geschlossen)
        digitalWrite(V4, LOW);
        */
    break;

    case automatisch: // Automatische Steuerung
      delay(250);
      openAllValves();
      Serial.println("Alle Ventile sind offen.");
      delay(1500);
      closeAllValves();
      Serial.println("Alle Ventile sind geschlossen.");
      delay(1500);
    break;

    case halt: // Schließt alle Ventile
      closeAllValves();
    break;

    default:
      closeAllValves();
      Serial.println("Fehler! Ungültiger Zustand. Alle Ventile wurden geschlossen.");
    break;
  }
}

void openAllValves()
{
  for (j = 1; j <= pinCount; j++)
  {
    digitalWrite(valves[j], HIGH);
  }
}

void closeAllValves()
{
  for (j = 1; j <= pinCount; j++)
  {
    digitalWrite(valves[j], LOW);
  }
}

void printAllTopics()
{
  for (j = 1; j <= pinCount; j++)
  {
    Serial.println(topics[j]);
  }
  Serial.println(topics[j]);
}
