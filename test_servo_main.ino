// ===== TEST SERVO MAIN (PIN 21) =====
// Main pusher servo - works with stepper motor
// Servo moves: 180° → 0° → 180° (loop forever, 1 second interval)

#include <ESP32Servo.h>

// [HARDWARE COMPONENTS]
// - 1x Servo motor (main pusher)

// [PIN CONNECTIONS]
#define SERVO_MAIN_PIN 21  // Main servo connected to GPIO 21

// [TIMING REQUIREMENTS]
#define DELAY_TIME 1000  // 1 second between movements

Servo servoMain;

void setup() {
  Serial.begin(115200);
  Serial.println("=== SERVO MAIN (PIN 21) TEST ===");
  Serial.println("This is the pusher servo that works with stepper motor");
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);
  servoMain.setPeriodHertz(50);
  servoMain.attach(SERVO_MAIN_PIN, 500, 2500);
  
  // Start at 180° (retracted position)
  servoMain.write(180);
  Serial.println("Servo initialized at 180° (retracted)");
  delay(1000);
}

void loop() {
  // Move to 0° (push position)
  Serial.println("Moving to 0° (PUSH)...");
  servoMain.write(0);
  delay(DELAY_TIME);
  
  // Move to 180° (retracted position)
  Serial.println("Moving to 180° (RETRACT)...");
  servoMain.write(180);
  delay(DELAY_TIME);
}
