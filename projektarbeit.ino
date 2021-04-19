#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoMqttClient.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);


int        port; //1883;
const char topic[]  = "sex/simple";


#define V1 23
#define V2 22
#define V3 21
#define V4 19

char ssid[30];
char passwd[30];
char broker[30]; //"test.mosquitto.org"
char brokerPort[6];
char dataSent[30];
int valveCount;
int i = 0;
bool gotSSID = false, gotPASSWD = false, gotBROKER = false, gotBROKERport = false;

enum Betriebmodus { halt, manuell, automatisch};
Betriebmodus modus;

enum Zustaende { offen, geschlossen };
Zustaende stateV1, stateV2, stateV3, stateV4;


String dataRecieved;
long timeNow, timePrev;

void setup() 
{
  Serial.begin(115200);
  delay(250);
  Serial.println("ESP32 started");
  delay(500);
  
  establishConnectionWiFi();
  
  while (!gotBROKER && !gotBROKERport)
  {
    if (Serial.available())
    {
      while (Serial.available())
      {
       broker[i++] = Serial.read();
       delay(100); 
      } 
      broker[i-1] = '\0';
      i = 0;
      Serial.println(broker);
      gotBROKER = true;
      Serial.println("\nPort:");

      while (!Serial.available())
        delay(100);
        
      while (Serial.available())
      {
       brokerPort[i++] = Serial.read();
       delay(100); 
      }

      brokerPort[i-1] = '\0';
      port = (int)brokerPort;
      i = 0;
      gotBROKERport = true;
    }
  }
  
  timeClient.begin();
   if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("\nYou're connected to the MQTT broker!");
  Serial.println();

  pinMode(V1, OUTPUT);
  pinMode(V2, OUTPUT);
  pinMode(V3, OUTPUT);
  pinMode(V4, OUTPUT);
  modus = halt;
  Serial.println("\nSetup Completed!\n To see all commands type: help");
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

/* Diese Funktion überprüft ob etwas über die Serielle Schnitstelle gesendet wurde
#  Wenn ja, dann speichert es die einzele Bits in CharArray und dann umwandelt CharArray in String um */ 
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
   mqttClient.beginMessage(topic);
   mqttClient.print("hello ");
   mqttClient.endMessage();
  }
  else if (dataRecieved == "time")
    Serial.println("Uhr: " + timeClient.getFormattedTime());
  else if (dataRecieved == "connect")
    establishConnectionWiFi();  
  else if (dataRecieved == "help")
  {
    //
    Serial.println("Commands: \nhelp - Shows this list of commands");
    Serial.println("stop - Disconnect from Network and close all Valves");
    Serial.println("manual - Switches to Manual Mode. In this Mode User can open/close specific Valve");
    Serial.println("automatic - Switches to Automatic Mode. In this Mode all Valves are being opened and closed frequently");
    Serial.println("showip - Shows current local IP");
    Serial.println("time - Shows current Time in 24H unit"); 
    Serial.println("connect - Opens ");
  }
    
  // Befehle für Debug Steuerung
  if (modus == manuell)
  {
    if (dataRecieved == "1 open")
      stateV1 = offen;
    else if (dataRecieved == "1 close")
      stateV1 = geschlossen;
    else if (dataRecieved == "2 open")
      stateV2 = offen;
    else if (dataRecieved == "2 close")
      stateV2 = geschlossen;
    else if (dataRecieved == "3 open")
      stateV3 = offen;
    else if (dataRecieved == "3 close")
      stateV3 = geschlossen;
    else if (dataRecieved == "4 open")
      stateV4 = offen;
    else if (dataRecieved == "4 close") 
      stateV4 = geschlossen;
  }

  // Befehle für automatische Steuerung
  
  i = 0;
  dataRecieved = "";
}

void steuerung()
{
  switch (modus)
  {
    case manuell: // Manuelle Steuerung

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

void establishConnectionWiFi()
{
  Serial.println("SSID of your Network:");
  while (!gotSSID && !gotPASSWD)
  {
    if (Serial.available())
    {
      while (Serial.available())
      {
       ssid[i++] = Serial.read();
       delay(100); 
      } 
  
      ssid[i-1] = '\0';
      i = 0;
      gotSSID = true;
      Serial.println("Passwort:");
      
      while (!Serial.available())
        delay(100);
        
      while (Serial.available())
      {
       passwd[i++] = Serial.read();
       delay(100); 
      }

      passwd[i-1] = '\0';
      i = 0;
      gotPASSWD = true;
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

void openAllValves()
{
  digitalWrite(V1, HIGH);
  digitalWrite(V2, HIGH);
  digitalWrite(V3, HIGH);
  digitalWrite(V4, HIGH);
}

void closeAllValves()
{
  digitalWrite(V1, LOW);
  digitalWrite(V2, LOW);
  digitalWrite(V3, LOW);
  digitalWrite(V4, LOW);
}
