#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>
#include <vector>

//Convert to web socket

/*===========GATE 3 CODE================*/

const int irLedPin = 10;         // IR LED output pin
const int pwmChannel = 0;       // PWM channel
const int pwmFreq = 38000;      // 38kHz carrier frequency
const int pwmResolution = 8;    // 8-bit resolution
const int maxDuty = 5;        // 50% duty cycle 

const int receiverPin = 7;      // TSSP90438 output pin

bool beamPreviouslyBroken = false;
bool triggered = true;

WebServer server(80);

// === Setup Access Point ===
const char* ssid = "SpeedLab";
const char* password = "12345678"; //OMG I PUT A PASSWORD ON GIT HOW WILL I EVER SURVIVE...

uint8_t gate1Mac[] = {0x3C, 0x84, 0x27, 0xC3, 0xC1, 0xFC}; 
uint8_t gate2Mac[] = {0xDC, 0xDA, 0x0C, 0x3C, 0x53, 0x6C};

unsigned long reactionTime = 0;

// === Enum and Structs ===
enum GateNumber : uint8_t {
  REACTION_GATE = 0,
  START_GATE = 1,
  MID_GATE_1 = 2,
  END_GATE   = 3
};

enum MessageType : uint8_t {
  MSG_RESET = 1,
  MSG_GATE_TRIGGER = 2,
  MSG_READY = 3,
  MSG_NOT_READY = 4
};

typedef struct {
  MessageType type;
  GateNumber gateNum; 
} message;


//Bits:  [ type (2 bits) | gateNum (2 bits) | unused/reserved (4 bits) ]
uint8_t messagem = 0x00;

typedef struct {
  GateNumber gate;
  unsigned long timestamp;
} GateEvent;

std::vector<GateEvent> raceData;
std::vector<MessageType> gateStatus(3, MSG_NOT_READY);

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(irLedPin, pwmChannel);
  ledcWrite(pwmChannel, maxDuty);
  pinMode(receiverPin, INPUT);

 WiFi.mode(WIFI_AP_STA);
 WiFi.softAP(ssid, password, 6);

 if (esp_now_init() != ESP_OK) {
    return;
  }

  esp_now_register_recv_cb(onDataReceived);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gate1Mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    return;
  }

  memcpy(peerInfo.peer_addr, gate2Mac, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    return;
  }
  

  // Web server routes
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/last", HTTP_GET, handleLastResults);
  server.begin();
 // Serial.print("Access Point IP: ");
  //Serial.println(WiFi.softAPIP());  
  //Serial.print("Ready!");
}

void loop() 
{
  if(triggered){
    server.handleClient();
  }
  
   bool beamDetected = digitalRead(receiverPin) == LOW;

    if (!beamDetected && !triggered) {
      unsigned long now = millis();
    
      beamPreviouslyBroken = true;
    
      triggered = true;
      raceData.push_back({END_GATE, now});
    
      //Serial.println("Runner detected!");
    } else if(beamDetected) {
     // Serial.println("Beam OK");
      digitalWrite(LED_BUILTIN, HIGH);

    }else{
      digitalWrite(LED_BUILTIN, LOW);
      //Serial.println("Beam break!");
    }
   delay(1); 
}

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len)
{
  unsigned long now = millis();
  if (len != sizeof(message)) return;

  message msg;
  memcpy(&msg, data, sizeof(msg));

  switch (msg.type) {
    case MSG_GATE_TRIGGER:
      //Serial.printf("Gate %d triggered at %lu ms\n", msg.gateNum, now);
      raceData.push_back({msg.gateNum, now});
      break;

    case MSG_READY:
     // Serial.printf("Gate %d is READY\n", msg.gateNum);
      gateStatus[msg.gateNum - 1] = MSG_READY;
      break;

    case MSG_NOT_READY:
      //Serial.printf("Gate %d is NOT READY\n", msg.gateNum);
      gateStatus[msg.gateNum -1 ] = MSG_NOT_READY;
      break;

    default:
      //Serial.println("Unknown message type received");
      break;
  }
}

String getStatusString(MessageType status) {
  switch(status) {
    case MSG_READY:
      return "READY";
    case MSG_NOT_READY:
      return "NOT READY";
    default:
      return "UNKNOWN";
  }
}

// === HTTP Handlers ===
void handleStatus() 
{
  String json = "[";
  for (size_t i = 0; i < gateStatus.size(); i++) {
    json += "{\"gate\":" + String(i + 1) +  // Gates are 1-indexed in your system
            ",\"status\":\"" + getStatusString(gateStatus[i]) + "\"}";
    if (i < gateStatus.size() - 1) json += ",";  // ← correct loop size
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleReset() 
{
  raceData.clear();
  reactionTime = 0;
  triggered = false;

  message resetMsg;
  resetMsg.type = MSG_RESET;

  esp_now_send(gate1Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));
  esp_now_send(gate2Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));

  server.send(200, "text/plain", "Race data cleared");
}

void handleLastResults() {
  String json = "[";
  for (size_t i = 0; i < raceData.size(); i++) {
    json += "{\"gate\":" + String(raceData[i].gate) + 
            ",\"time\":" + String(raceData[i].timestamp) + "}";
    if (i < raceData.size() - 1) json += ",";
  }
  if (raceData.size() > 0) json += ",";
  json += "{\"gate\":0,\"time\":" + String(reactionTime) + "}";
  json += "]";
  server.send(200, "application/json", json);
 // Serial.print("DATA FETCHED!");
}