#include "Modulino.h"
#include <WiFi.h>
#include <esp_now.h>
#include <usb.h>

ModulinoMovement movement;
ModulinoBuzzer buzzer;


int frequency = 300;  // Frequency of the tone in Hz
int duration = 500;

float x, y;
const float threshold = 0.03;  // Movement threshold for reaction

bool waitingForGoCommand = false;
bool waitingForMovement = false;

uint8_t receiverMac[] = { 0x3C, 0x84, 0x27, 0xC3, 0xC1, 0xD0 };

float filteredX = 0;
float prevX = 0;
float alpha = 0.5;

unsigned long start = 0;
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
  START = 6
};

typedef struct message {
  MessageType type;
  GateNumber gateType;
} message;


message reactionMessage;
message startMessage;


void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  if (len != sizeof(message)) {
    return;
  }
  message incoming;
  memcpy(&incoming, incomingData, sizeof(message));

  if (incoming.type == MSG_RESET) {
    waitingForGoCommand = true;
  } 
}

void setup() {

 //Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    return;
  }

  Modulino.begin();
  buzzer.begin();
  movement.begin();

  startMessage.type = START;
  startMessage.gateType = REACTION_GATE;

  reactionMessage.type = REACTION;
  reactionMessage.gateType = REACTION_GATE;
}

void loop() {

  if (waitingForGoCommand) {

    esp_now_send(receiverMac, (uint8_t *)&startMessage, sizeof(startMessage));
    
    buzzer.tone(5, 1000);
    delay(15);
    waitingForGoCommand = false;
    waitingForMovement = true;
  }

  // Update motion data
  movement.update();
  float rawX = movement.getX();
  filteredX = alpha * (filteredX + rawX - prevX);
  prevX = rawX;

  // Check for movement after "GO"
  if (waitingForMovement) {
    if (abs(filteredX) > threshold) {

      esp_now_send(receiverMac, (uint8_t *)&reactionMessage, sizeof(reactionMessage));

      // Reset for next round
      waitingForMovement = false;
    }
  }
 delay(1);
}