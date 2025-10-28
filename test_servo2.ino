// ===== TEST SERVO 2 =====
// Servo moves: 180° → 0° → 180° (loop forever, 5 seconds interval)

#include <ESP32Servo.h>

// [HARDWARE COMPONENTS]
// - 1x Servo motor

// [PIN CONNECTIONS]
#define SERVO2_PIN 17  // Servo 2 connected to GPIO 17

// [TIMING REQUIREMENTS]
#define DELAY_TIME 1000  // 5 seconds between movements

Servo servo2;

void setup() {
  Serial.begin(115200);
  Serial.println("=== SERVO 2 TEST ===");
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);
  servo2.setPeriodHertz(50);
  servo2.attach(SERVO2_PIN, 500, 2500);
  
  // Start at 180°
  servo2.write(180);
  Serial.println("Servo initialized at 180°");
  delay(1000);
}

void loop() {
  // Move to 0°
  Serial.println("Moving to 0°...");
  servo2.write(0);
  delay(DELAY_TIME);
  
  // Move to 180°
  Serial.println("Moving to 180°...");
  servo2.write(180);
  delay(DELAY_TIME);
}
