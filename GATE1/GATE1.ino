#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

/*=========== GATE 1 (BROADCAST FOR CHEST MASTER) ================*/

constexpr int irLedPin = 10;         // IR LED output pin
constexpr int pwmChannel = 0;        // PWM channel
constexpr int pwmFreq = 38000;       // 38 kHz carrier frequency
constexpr int pwmResolution = 8;     // 8-bit resolution
constexpr int maxDuty = 2;

constexpr int receiverPin = 7;       // TSSP90438 output pin

constexpr uint8_t WIFI_CHANNEL = 1;
constexpr uint32_t REARM_MS = 2500;
constexpr uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum MessageType : uint8_t {
  MSG_RESET = 1,
  MSG_GATE_TRIGGER = 2,
  MSG_READY = 3,
  MSG_NOT_READY = 4
};

enum GateNumber : uint8_t {
  START_GATE = 1,
  MID_GATE_1 = 2,
  END_GATE = 3,
  REACTION_BOX = 4
};

struct __attribute__((packed)) GatePacket {
  uint8_t type;
  uint8_t gateId;
  uint16_t seq;
};

GatePacket outgoingPacket{MSG_GATE_TRIGGER, START_GATE, 0};
bool triggered = false;
uint32_t lastTriggerMs = 0;
int count = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return;

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) return;

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(irLedPin, pwmChannel);
  ledcWrite(pwmChannel, maxDuty);

  pinMode(receiverPin, INPUT);
}

void loop() {
  if (triggered && (millis() - lastTriggerMs >= REARM_MS)) {
    triggered = false;
  }

  const bool beamDetected = (digitalRead(receiverPin) == LOW);

  if (!beamDetected && !triggered) {
    count++;
    if (count >= 2) {
      triggered = true;
      count = 0;
      lastTriggerMs = millis();
      outgoingPacket.seq++;

      for (int i = 0; i < 3; ++i) {
        esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&outgoingPacket), sizeof(outgoingPacket));
        delay(1);
      }
    }
  } else if (beamDetected) {
    count = 0;
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  delay(1);
}
