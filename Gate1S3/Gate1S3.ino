#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h> 

// MAC addresses
// 3C:84:27:C3:C1:D0 MAC FOR HUB
// 3C:84:27:C3:C1:FC MAC for Start gate

//for small start gate 10:20:BA:31:31:50
// for small hub const 10:20:ba:31:31:38

/*===========GATE 1 CODE================*/

const int irLedPin = 1;         // IR LED output pin (check if usable on your board)
const int pwmChannel = 0;        // PWM channel
const int pwmFreq = 38000;       // 38kHz carrier frequency
const int pwmResolution = 8;     // 8-bit resolution
int maxDuty = 20;        

const int receiverPin = 5;       // TSSP90438 output pin

int count = 0;

//==================================
//====VARIABLES=====================
//=================================
enum MessageType : uint8_t {
  MSG_RESET = 1,
  MSG_GATE_TRIGGER = 2,
  MSG_READY = 3,
  MSG_NOT_READY = 4,
  MAX_DUTY = 7
};

enum GateNumber : uint8_t {
  START_GATE = 1,
  MID_GATE_1 = 2,
  END_GATE = 3,
  REACTION_BOX = 4
};

typedef struct message {
  MessageType type;
  uint8_t gateType;
} message;

message outgoingMessage;
message resetMessage;

bool triggered = true;

uint8_t receiverMac[] = { 0x10, 0x20, 0xBA, 0x31, 0x31, 0x38 };  // MAC for hub

//==================================
//=========ESP-NOW RECEIVE==========
//==================================

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(message)) return;

  message incoming;
  memcpy(&incoming, incomingData, sizeof(message));

  if (incoming.type == MSG_RESET) {
    triggered = false;
    resetMessage.type = MSG_READY;
    resetMessage.gateType = START_GATE;
    esp_now_send(receiverMac, (uint8_t *)&resetMessage, sizeof(resetMessage));
  }

  if(incoming.type == MAX_DUTY ){
    int newDuty = incoming.gateType;
    maxDuty = newDuty;
    ledcWrite(irLedPin, maxDuty);
  }
}

//==================================
//==============SETUP===============
//==================================

void setup() {
  setCpuFrequencyMhz(80);
  pinMode(48, OUTPUT);
  digitalWrite(48, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // Ensure not connected to AP

  if (esp_now_init() != ESP_OK) return;

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) return;


  ledcAttach(irLedPin, pwmFreq, pwmResolution);
  ledcWrite(irLedPin, maxDuty);

  pinMode(receiverPin, INPUT);

  outgoingMessage.type = MSG_GATE_TRIGGER;
  outgoingMessage.gateType = START_GATE;
}

//==================================
//===============LOOP===============
//==================================

void loop() {
  bool beamDetected = digitalRead(receiverPin) == LOW;

  if (!beamDetected && !triggered) {
    count++;

  if(count >= 2){
    triggered = true;
    count = 0;
    esp_now_send(receiverMac, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
  }
  
  } else if (beamDetected) {
    digitalWrite(48, HIGH);
  } else {
    digitalWrite(48, LOW);
    delay(100);
  }

  if(triggered){
    delay(200);
  }

  delay(1); 
}
