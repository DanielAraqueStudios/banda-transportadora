// ===== TEST SERVO 1 =====
// Servo moves: 180° → 0° → 180° (loop forever, 1 second interval)

#include <ESP32Servo.h>

// [HARDWARE COMPONENTS]
// - 1x Servo motor

// [PIN CONNECTIONS]
#define SERVO1_PIN 16  // Servo 1 connected to GPIO 16

// [TIMING REQUIREMENTS]
#define DELAY_TIME 1000  // 1 second between movements

Servo servo1;

void setup() {
  Serial.begin(115200);
  Serial.println("=== SERVO 1 TEST ===");
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);
  servo1.setPeriodHertz(50);
  servo1.attach(SERVO1_PIN, 500, 2500);
  
  // Start at 180°
  servo1.write(180);
  Serial.println("Servo initialized at 180°");
  delay(1000);
}

void loop() {
  // Move to 0°
  Serial.println("Moving to 0°...");
  servo1.write(0);
  delay(DELAY_TIME);
  
  // Move to 180°
  Serial.println("Moving to 180°...");
  servo1.write(180);
  delay(DELAY_TIME);
}
