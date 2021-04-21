#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoMqttClient.h>

// Globale Variablen

char ssid[30], passwd[30], broker[30], dataSent[30];
int pinCount, port, countTopic, i = 0, j, valves[40], valvesPin[40];
bool establishedVALVES = false, establishedWIFI = false, establishedMQTT = false;
enum Betriebmodus { halt, manuell, automatisch};
String dataRecieved, topics[41], sentTopic;
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
  subscribeAllTopics();
  modus = halt;
  Serial.println("Setup Completed!\n To see all commands type: help");
}

void loop() 
{
    checkMQTT(); 
  
  if (Serial.available())
    serialCheckAndSetMode();
    
  steuerung(); 
  timeClient.update();
  mqttClient.poll();
  delay(500);
}

// Funktionen

void createPins()
{
  Serial.println("Please type in how many Pins would you like to use:");

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
        Serial.printf("Assign (On Board) Pin (%2d)\n", j);
        
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
      Serial.println("Main Topic:")
      
      while (!Serial.available())
        delay(100);

      while (Serial.available())
      {
          dataSent[i++] = Serial.read();
          delay(100);
      }
      dataSent[i-1] = '\0'; //Serial in Arduino  
      topics[j] = dataSent;
      i = 0;
      
      dataRecieved = "";
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

void subscribeAllTopics()
{
  for (j = 1; j <= pinCount; j++)
     mqttClient.subscribe(topics[j]);
}

void checkMQTT()
{
  if (mqttClient.parseMessage())
  {
    sentTopic = mqttClient.messageTopic();
    Serial.println(sentTopic);
    
    if (mqttClient.available())
    {
      while (mqttClient.available())
        dataSent[i++] = (char)mqttClient.read();

      dataSent[i] = '\0';
      i = 0;
      dataRecieved = dataSent;
      Serial.println(dataRecieved);
    } 

    for (j = 1; j <= pinCount; j++)
    {
      if (sentTopic == topics[j])
      {
        if (dataRecieved == "open")
          digitalWrite(valves[j], HIGH);
        else if (dataRecieved == "close")
          digitalWrite(valves[j], LOW);
      }
    }
    dataRecieved = "";
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
  
  i = 0;
  dataRecieved = "";
}

void steuerung()
{
  switch (modus)
  {
    case manuell: // Manuelle Steuerung
      
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
    digitalWrite(valves[j], HIGH);
}

void closeAllValves()
{
  for (j = 1; j <= pinCount; j++)
    digitalWrite(valves[j], LOW);
}

void printAllTopics()
{
  for (j = 1; j <= pinCount; j++)
  {
    Serial.println(topics[j]);
  }
  Serial.println(topics[j]);
}
