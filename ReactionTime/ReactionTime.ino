#include "Modulino.h"

ModulinoMovement movement;
ModulinoBuzzer buzzer;

int frequency = 440;  
int duration = 500;

float x, y;
const float threshold = 0.04; // Movement threshold for reaction 

unsigned long stimulusTime = 0;
bool waitingForGoCommand = true;
bool waitingForMovement = false;

void setup() {
  Serial.begin(9600);
  Modulino.begin();
  movement.begin();
  buzzer.begin();
  Serial.println("Type 'go' and press Enter to start the reaction timer countdown.");
}

void loop() {
  // Check for serial input
  if (waitingForGoCommand && Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove whitespace

    if (input.equalsIgnoreCase("go")) {
      // Start the countdown
      Serial.println("Countdown starting...");
      for (int i = 3; i >= 1; i--) {
        Serial.print(i);
        Serial.println("...");
        delay(1000);
      }
      buzzer.tone(frequency, duration);
      Serial.println("GO! Move now!");
      stimulusTime = millis();
      waitingForGoCommand = false;
      waitingForMovement = true;
    }
  }

  // Update motion data
  movement.update();
  x = movement.getX();
  y = movement.getY();

  // Check for movement after "GO"
  if (waitingForMovement) {
    if (abs(x) > threshold ) {
      unsigned long reactionTimeMs = millis() - stimulusTime;
      float reactionTimeSec = reactionTimeMs / 1000.0;

      Serial.print("Reaction time: ");
      Serial.print(reactionTimeSec, 3);
      Serial.println(" seconds");

      // Reset for next round
      waitingForMovement = false;
      waitingForGoCommand = true;
      Serial.println("\nType 'go' to try again.");
    }
  }

  delay(5);
}