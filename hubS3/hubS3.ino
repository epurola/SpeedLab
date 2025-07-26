#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>
#include <vector>

//Convert to web socket

/*===========GATE 3 CODE================*/

const int irLedPin = 1;      // IR LED output pin
const int pwmChannel = 0;     // PWM channel
const int pwmFreq = 38000;    // 38kHz carrier frequency
const int pwmResolution = 8;  // 8-bit resolution
int maxDuty = 15;        // 50% duty cycle

const int receiverPin = 5;  // TSSP90438 output pin

bool beamPreviouslyBroken = false;
bool triggered = true;
unsigned long stimulusTime = 0;

int count = 0;

WebServer server(80);

// === Setup Access Point ===
const char* ssid = "GATE_HUB";
const char* password = "1234";  //OMG I PUT A PASSWORD ON GITHUB HOW WILL I EVER SURVIVE...

uint8_t gate1Mac[] = { 0x10, 0x20, 0xBA, 0x31, 0x31, 0x50 };
//uint8_t gate2Mac[] = { 0xDC, 0xDA, 0x0C, 0x3C, 0x53, 0x6C };
//uint8_t blockStartMac[] = { 0xDC, 0xDA, 0x0C, 0x3B, 0x67, 0x04 };


unsigned long reactionTime = 0;

unsigned long start =0;
unsigned long end = 0;

// === Enum and Structs ===
enum GateNumber : uint8_t {
  REACTION_GATE = 0,
  START_GATE = 1,
  MID_GATE_1 = 2,
  END_GATE = 3
};

enum MessageType : uint8_t {
  MSG_RESET = 1,
  MSG_GATE_TRIGGER = 2,
  MSG_READY = 3,
  MSG_NOT_READY = 4,
  REACTION = 5,
  START = 6,
  MAX_DUTY = 7
};

typedef struct {
  MessageType type;
  uint8_t gateNum;
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
  setCpuFrequencyMhz(80);
  pinMode(48, OUTPUT);
  digitalWrite(48, LOW);

  ledcAttach(irLedPin, pwmFreq, pwmResolution);
  ledcWrite(irLedPin, maxDuty);
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

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    return;
  }

 // memcpy(peerInfo.peer_addr, smallGate1Mac, 6);
  //if (esp_now_add_peer(&peerInfo) != ESP_OK) {
   // return;
  //}

  // Web server routes
  server.on("/status", HTTP_POST, handleStatus);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/last", HTTP_GET, handleLastResults);
  server.on("/MaxDuty", HTTP_POST, changeMaxDuty);
  delay(500);
  server.begin();
}

void loop() {
  if (triggered) {
    server.handleClient();
    delay(200);
    WiFi.setSleep(true);
  }
  
  bool beamDetected = digitalRead(receiverPin) == LOW;
 
  if (!beamDetected && !triggered) {
    unsigned long now = millis();

    count++;

  if(count >= 2){
    triggered = true;
    count = 0;
    raceData.push_back({ END_GATE, now });
  }

  //Serial.println("Runner detected!");
  } else if (beamDetected) {
    // Serial.println("Beam OK");
    digitalWrite(48, HIGH);

  } else {
    digitalWrite(48, LOW);
    //Serial.println("Beam break!");
    delay(100);
  }
  delay(1);
}

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t* data, int len) {
  unsigned long now = millis();
  if (len != sizeof(message)) return;

  message msg;
  memcpy(&msg, data, sizeof(msg));

  switch (msg.type) {
    case MSG_GATE_TRIGGER:
      //Serial.printf("Gate %d triggered at %lu ms\n", msg.gateNum, now);
      raceData.push_back(GateEvent{static_cast<GateNumber>(msg.gateNum), now});
      break;

    case MSG_READY:
      // Serial.printf("Gate %d is READY\n", msg.gateNum);
      gateStatus[msg.gateNum - 1] = MSG_READY;
      break;

    case MSG_NOT_READY:
      //Serial.printf("Gate %d is NOT READY\n", msg.gateNum);
      gateStatus[msg.gateNum - 1] = MSG_NOT_READY;
      break;

    case START:
      stimulusTime = millis();
      raceData.push_back(GateEvent{static_cast<GateNumber>(msg.gateNum), now});
      break;

    case REACTION: {
      reactionTime = millis() - stimulusTime;
    break;
    }
   
    default:
      //Serial.println("Unknown message type received");
      break;
  }
}

String getStatusString(MessageType status) {
  switch (status) {
    case MSG_READY:
      return "READY";
    case MSG_NOT_READY:
      return "NOT READY";
    default:
      return "UNKNOWN";
  }
}

// === HTTP Handlers ===
void handleStatus() {
  raceData.clear();
  reactionTime = 0;
  triggered = false;

  message resetMsg;
  resetMsg.type = MSG_RESET;
  
  esp_now_send(gate1Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));
  //esp_now_send(gate2Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));

 // delay(500);
  //esp_now_send(blockStartMac, (uint8_t*)&resetMsg, sizeof(resetMsg));
  //esp_now_send(smallGate1Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));

  server.send(200, "text/plain", "Status message sent");
}

void changeMaxDuty(){

  if(server.hasArg("value"))
  {
    int newDuty = server.arg("value").toInt();
    if(newDuty >= 0 && newDuty <= 100)
    {
      maxDuty = newDuty;
      ledcWrite(irLedPin, maxDuty);
      message resetMsg;
      resetMsg.type = MAX_DUTY;
      resetMsg.gateNum = newDuty;
      esp_now_send(gate1Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));
      server.send(200, "text/plain", "Duty cycle updated to: " + String(maxDuty));
    }
    else {
      server.send(400, "text/plain", "Invalid value (must be 0–255)");
    }
  }

}

void handleReset() {
  raceData.clear();
  reactionTime = 0;
  triggered = false;

  WiFi.setSleep(false);

  message resetMsg;
  resetMsg.type = MSG_RESET;

  esp_now_send(gate1Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));
  //esp_now_send(gate2Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));
  //esp_now_send(blockStartMac, (uint8_t*)&resetMsg, sizeof(resetMsg));
  //esp_now_send(smallGate1Mac, (uint8_t*)&resetMsg, sizeof(resetMsg));


  server.send(200, "text/plain", "Race data cleared");
}

void handleLastResults() {
  String json = "[";
  for (size_t i = 0; i < raceData.size(); i++) {
    json += "{\"gate\":" + String(raceData[i].gate) + ",\"time\":" + String(raceData[i].timestamp) + "}";
    if (i < raceData.size() - 1) json += ",";
  }
  if (raceData.size() > 0) json += ",";
  json += "{\"gate\":0,\"time\":" + String(reactionTime) + "}";
  json += "]";
  server.send(200, "application/json", json);
  // Serial.print("DATA FETCHED!");
}