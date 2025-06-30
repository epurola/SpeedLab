#include <esp_now.h>
#include <WiFi.h>

// 3C:84:27:C3:C1:D0 MAC FOR HUB
// 3C:84:27:C3:C1:FC MAC for Start gate

/*===========GATE 1 CODE================*/

const int irLedPin = 10;         // IR LED output pin
const int pwmChannel = 0;       // PWM channel
const int pwmFreq = 38000;      // 38kHz carrier frequency
const int pwmResolution = 8;    // 8-bit resolution
const int maxDuty = 5;        // 50% duty cycle 

const int receiverPin = 7;      // TSSP90438 output pin

//==================================
//====VARIABLES=====================
//=================================
enum MessageType : uint8_t {
  MSG_RESET = 1,
  MSG_GATE_TRIGGER = 2,
  MSG_READY = 3,
  MSG_NOT_READY = 4
};

enum GateNumber : uint8_t {
  START_GATE = 1,
  MID_GATE_1 = 2,
  END_GATE = 3
};

typedef struct message {
  MessageType type;
  GateNumber gateType;
} message;

message outgoingMessage;

bool beamPreviouslyBroken = false;
bool triggered = true;

int tempGate = 0;

uint8_t receiverMac[] = { 0x3C, 0x84, 0x27, 0xC3, 0xC1, 0xD0 };  //Mac for hub


//==================================
//=========ON ESP NOW RECEIVED======
//==================================

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) 
{
  if (len != sizeof(message)) 
  {
   // Serial.println("Received unexpected data size");
    return;
  }
  message incoming;
  memcpy(&incoming, incomingData, sizeof(message));

  if (incoming.type == MSG_RESET) 
  {
    //Serial.println("Reset command received!");
    triggered = false;
    tempGate = 0;
    outgoingMessage.type = MSG_READY;
    outgoingMessage.gateType = (GateNumber)(START_GATE + tempGate);
    esp_err_t result = esp_now_send(receiverMac, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
  } 
  else 
  {
    //Serial.print("Unknown message type: ");
    //Serial.println(incoming.type, HEX);
  }
}

//==================================
//=========SETUP===================
//==================================

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) 
  {
    //Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) 
  {
   // Serial.println("Failed to add peer");
    return;
  }

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(irLedPin, pwmChannel);
  ledcWrite(pwmChannel, maxDuty);
  
  pinMode(receiverPin, INPUT);

  //Serial.println("Sender ready.");
}

void loop() {

    bool beamDetected = digitalRead(receiverPin) == LOW;

    if (!beamDetected && !triggered) {
      outgoingMessage.type = MSG_GATE_TRIGGER;
      outgoingMessage.gateType = (GateNumber)(MID_GATE_1);
    
      beamPreviouslyBroken = true;
    
      triggered = true;
      esp_err_t result = esp_now_send(receiverMac, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
  
      //Serial.println("Runner detected!");
    } else if(beamDetected) {
      //Serial.println("Beam OK");
      digitalWrite(LED_BUILTIN, HIGH);

    }else{
      digitalWrite(LED_BUILTIN, LOW);
      //Serial.println("Beam break!");
    }
   delay(1); 
}