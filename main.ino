#include <Arduino.h>
#include <Stepper.h>
#include <ESP32Servo.h>

// ===== Sensores ultrasónicos =====
#define TRIG1 14
#define ECHO1 12

// ===== Motor DC Banda de Entrada (L298N canal B) =====
#define IN3 26
#define IN4 27
#define ENB 25

// ===== Motor DC Banda de Salida (L298N canal A) =====
#define IN1_DC 15  // IN1 del L298N para canal A
#define IN2_DC 2   // IN2 del L298N para canal A
#define ENA 4      // ENA del L298N para canal A

// ===== Motor paso a paso (28BYJ-48 + ULN2003) =====
#define IN1_STP 32
#define IN2_STP 33
#define IN3_STP 19
#define IN4_STP 23
#define STEPS_PER_REV 2048L

// ===== Servomotor para modo INGRESAR =====
#define SERVO_PIN 21
Servo servoEmpujador;
const int SERVO_POS_INICIAL = 180;
const int SERVO_POS_EMPUJE = 0;
const int SERVO_DELAY = 500;

// ===== Servomotores para modo RETIRAR (uno por compartimiento) =====
#define SERVO_COMP1_PIN 16  // Servo para compartimiento 1
#define SERVO_COMP2_PIN 17  // Servo para compartimiento 2
#define SERVO_COMP3_PIN 22  // Servo para compartimiento 3
#define SERVO_COMP4_PIN 13  // Servo para compartimiento 4 

Servo servoComp1;
Servo servoComp2;
Servo servoComp3;
Servo servoComp4;

const int SERVO_RETIRAR_INICIAL = 0;    // Posición inicial para servos de RETIRAR
const int SERVO_RETIRAR_EMPUJE = 180;   // Posición de empuje para servos de RETIRAR
const int SERVO_RETIRAR_DELAY = 500;    // Tiempo en cada posición

Stepper stepper(STEPS_PER_REV, IN1_STP, IN3_STP, IN2_STP, IN4_STP);

// ===== Variables de estado =====
enum State { WAITING_MODE, IDLE, MOTOR_ON, WAIT_FOR_DESTINATION, MOVING, PUSHING, WAITING_MANUAL };
State stateMachine = WAITING_MODE;

// ===== Modo de operación =====
enum Mode { MODE_NONE, MODE_INGRESAR, MODE_RETIRAR };
Mode modoOperacion = MODE_NONE;

float distance1 = 0.0;
bool motorEntradaEncendido = false;
bool motorSalidaEncendido = false;

int destino = 1;
long currentPositionSteps = 0;
long positionsSteps[5];

// ===== Parámetros =====
const int LEDC_CHANNEL_ENTRADA = 5;  // Canal PWM para motor de entrada
const int LEDC_CHANNEL_SALIDA = 4;   // Canal PWM para motor de salida
const int LEDC_FREQ = 5000;
const int LEDC_RESOLUTION = 8;

const float UMBRAL_SENSOR1 = 10.0;

// ===== Funciones =====
float medirDistancia(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duracion = pulseIn(echoPin, HIGH, 30000);
  if (duracion == 0) return -1.0;
  return (duracion * 0.0343) / 2.0;
}

void encenderMotorEntrada() {
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, 200);  // Usar PIN directamente (ESP32 Core 3.x)
  motorEntradaEncendido = true;
}

void detenerMotorEntrada() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, 0);  // Usar PIN directamente (ESP32 Core 3.x)
  motorEntradaEncendido = false;
}

void encenderMotorSalida() {
  digitalWrite(IN1_DC, HIGH);
  digitalWrite(IN2_DC, LOW);
  ledcWrite(ENA, 200);  // Usar PIN directamente (ESP32 Core 3.x)
  motorSalidaEncendido = true;
  Serial.println("Motor de salida ENCENDIDO (banda de salida activa)");
}

void detenerMotorSalida() {
  digitalWrite(IN1_DC, LOW);
  digitalWrite(IN2_DC, LOW);
  ledcWrite(ENA, 0);  // Usar PIN directamente (ESP32 Core 3.x)
  motorSalidaEncendido = false;
  Serial.println("Motor de salida APAGADO");
}

void definirPosiciones() {
  const float DEG_TO_STEPS = 2048.0 / 360.0;
  
  if (modoOperacion == MODE_INGRESAR) {
    // Posiciones normales para INGRESAR (desde 0°)
    positionsSteps[1] = (long)(-125 * DEG_TO_STEPS);   // Compartimiento 1
    positionsSteps[2] = (long)(-55 * DEG_TO_STEPS);    // Compartimiento 2
    positionsSteps[3] = (long)(71 * DEG_TO_STEPS);   // Compartimiento 3
    positionsSteps[4] = (long)(121 * DEG_TO_STEPS);  // Compartimiento 4
  } else {
    // Posiciones INVERTIDAS para RETIRAR (desde 180°)
    // El motor está al otro lado, entonces los ángulos se invierten
    const long OFFSET_180 = (long)(180 * DEG_TO_STEPS);
    positionsSteps[1] = OFFSET_180 + (long)(60 * DEG_TO_STEPS);  // Comp 1 invertido
    positionsSteps[2] = OFFSET_180 + (long)(125 * DEG_TO_STEPS);   // Comp 2 invertido
    positionsSteps[3] = OFFSET_180 - (long)(110 * DEG_TO_STEPS);   // Comp 3 invertido
    positionsSteps[4] = OFFSET_180 - (long)(57 * DEG_TO_STEPS);  // Comp 4 invertido
  }
}

void moverAPosicion(int compartimiento) {
  if (compartimiento < 1 || compartimiento > 4) return;

  stepper.setSpeed(12);
  long targetSteps = positionsSteps[compartimiento];
  long delta = targetSteps - currentPositionSteps;

  if (delta != 0) {
    stepper.step(delta);
    currentPositionSteps = targetSteps;
    Serial.print("Stepper movido a compartimiento ");
    Serial.print(compartimiento);
    Serial.print(" (");
    Serial.print(currentPositionSteps);
    Serial.println(" pasos).");
  }
}

void volverABase() {
  stepper.setSpeed(12);
  long targetBase = 0;
  
  if (modoOperacion == MODE_RETIRAR) {
    // En modo retirar, la base es 180°
    const float DEG_TO_STEPS = 2048.0 / 360.0;
    targetBase = (long)(180 * DEG_TO_STEPS);
  }
  
  long delta = targetBase - currentPositionSteps;
  if (delta != 0) {
    stepper.step(delta);
    Serial.print("Stepper regresó a base desde ");
    Serial.print(currentPositionSteps);
    Serial.print(" pasos a ");
    Serial.println(targetBase);
    currentPositionSteps = targetBase;
  } else {
    Serial.println("Stepper ya está en la posición base.");
  }
}

void empujarTapa() {
  Serial.println("Iniciando empuje de tapa (modo INGRESAR)...");
  
  // Mover servo a posición de empuje (0°)
  servoEmpujador.write(SERVO_POS_EMPUJE);
  Serial.println("Servo empujador -> 0° (empujando)");
  delay(SERVO_DELAY);
  
  // Regresar servo a posición inicial (180°)
  servoEmpujador.write(SERVO_POS_INICIAL);
  Serial.println("Servo empujador -> 180° (retraído)");
  delay(SERVO_DELAY);
  
  Serial.println("Empuje completado (modo INGRESAR).");
}

void empujarTapaRetirar(int compartimiento) {
  Serial.print("Activando servo del compartimiento ");
  Serial.println(compartimiento);
  
  // Activar el servo correspondiente al compartimiento
  switch(compartimiento) {
    case 1:
      Serial.println("Moviendo servo compartimiento 1 (INVERTIDO)...");
      servoComp1.write(SERVO_RETIRAR_INICIAL);  // 0° empuja
      Serial.println("Servo 1 -> 0° (empujando)");
      delay(SERVO_RETIRAR_DELAY);
      servoComp1.write(SERVO_RETIRAR_EMPUJE);   // 180° retrae
      Serial.println("Servo 1 -> 180° (retraído)");
      delay(SERVO_RETIRAR_DELAY);
      break;
      
    case 2:
      Serial.println("Moviendo servo compartimiento 2...");
      servoComp2.write(SERVO_RETIRAR_EMPUJE);
      Serial.println("Servo 2 -> 180° (empujando)");
      delay(SERVO_RETIRAR_DELAY);
      servoComp2.write(SERVO_RETIRAR_INICIAL);
      Serial.println("Servo 2 -> 0° (retraído)");
      delay(SERVO_RETIRAR_DELAY);
      break;
      
    case 3:
      Serial.println("Moviendo servo compartimiento 3 (INVERTIDO)...");
      servoComp3.write(SERVO_RETIRAR_INICIAL);  // 0° empuja
      Serial.println("Servo 3 -> 0° (empujando)");
      delay(SERVO_RETIRAR_DELAY);
      servoComp3.write(SERVO_RETIRAR_EMPUJE);   // 180° retrae
      Serial.println("Servo 3 -> 180° (retraído)");
      delay(SERVO_RETIRAR_DELAY);
      break;
      
    case 4:
      Serial.println("Moviendo servo compartimiento 4...");
      servoComp4.write(SERVO_RETIRAR_EMPUJE);
      Serial.println("Servo 4 -> 180° (empujando)");
      delay(SERVO_RETIRAR_DELAY);
      servoComp4.write(SERVO_RETIRAR_INICIAL);
      Serial.println("Servo 4 -> 0° (retraído)");
      delay(SERVO_RETIRAR_DELAY);
      break;
      
    default:
      Serial.println("Error: compartimiento inválido");
      return;
  }
  
  Serial.print("Empuje completado en compartimiento ");
  Serial.println(compartimiento);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);

  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(IN1_DC, OUTPUT);
  pinMode(IN2_DC, OUTPUT);

  // Configurar PWM para motor de entrada (versión moderna ESP32 Core 3.x)
  ledcAttach(ENB, LEDC_FREQ, LEDC_RESOLUTION);
  
  // Configurar PWM para motor de salida (versión moderna ESP32 Core 3.x)
  ledcAttach(ENA, LEDC_FREQ, LEDC_RESOLUTION);

  stepper.setSpeed(12);
  detenerMotorEntrada();
  detenerMotorSalida();

  // Allocate timers for all 5 servos (ESP32 has 4 timers: 0,1,2,3)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  Serial.println("Inicializando todos los servos...");

  // Servo Main (pin 21) - uses timer 0
  servoEmpujador.setPeriodHertz(50);
  servoEmpujador.attach(SERVO_PIN, 500, 2500);
  servoEmpujador.write(SERVO_POS_INICIAL);
  Serial.println("Servo empujador (pin 21) inicializado en 180°");

  // Servo 1 (pin 16) - uses timer 1
  servoComp1.setPeriodHertz(50);
  servoComp1.attach(SERVO_COMP1_PIN, 500, 2500);
  servoComp1.write(SERVO_RETIRAR_EMPUJE);
  Serial.println("Servo 1 (pin 16) inicializado en 180° (INVERTIDO)");
  
  // Servo 2 (pin 17) - uses timer 2
  servoComp2.setPeriodHertz(50);
  servoComp2.attach(SERVO_COMP2_PIN, 500, 2500);
  servoComp2.write(SERVO_RETIRAR_INICIAL);
  Serial.println("Servo 2 (pin 17) inicializado en 0°");
  
  // Servo 3 (pin 22) - uses timer 2 (shared with servo 2)
  servoComp3.setPeriodHertz(50);
  servoComp3.attach(SERVO_COMP3_PIN, 500, 2500);
  servoComp3.write(SERVO_RETIRAR_EMPUJE);
  Serial.println("Servo 3 (pin 22) inicializado en 180° (INVERTIDO)");
  
  // Servo 4 (pin 13) - uses timer 3
  servoComp4.setPeriodHertz(50);
  servoComp4.attach(SERVO_COMP4_PIN, 500, 2500);
  servoComp4.write(SERVO_RETIRAR_INICIAL);
  Serial.println("Servo 4 (pin 13) inicializado en 0°");

  
  Serial.println("Todos los servos de compartimientos (RETIRAR) inicializados en posición 0°");

  // Iniciar en posición 0
  currentPositionSteps = 0;
  Serial.println("Stepper inicializado en posición 0");

  Serial.println("ESP32 listo. Estado: WAITING_MODE");
  Serial.println("MODE_REQUEST");
}

// ===== Loop =====
void loop() {
  
  // --- WAITING_MODE ---
  if (stateMachine == WAITING_MODE) {
    if (Serial.available() > 0) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      
      if (line == "INGRESAR") {
        // IMPORTANTE: Si viene de modo RETIRAR, apagar motor de salida
        if (motorSalidaEncendido) {
          detenerMotorSalida();
          Serial.println("Motor de salida detenido al cambiar a modo INGRESAR.");
        }
        
        modoOperacion = MODE_INGRESAR;
        Serial.println("Modo seleccionado: INGRESAR");
        
        // La posición base para INGRESAR es 0°
        if (currentPositionSteps != 0) {
          Serial.println("Ajustando a posición 0° para modo INGRESAR...");
          stepper.setSpeed(12);
          long delta = -currentPositionSteps;
          stepper.step(delta);
          currentPositionSteps = 0;
          Serial.println("Stepper en posición 0° (base para INGRESAR)");
        } else {
          Serial.println("Stepper ya está en posición 0° (base para INGRESAR)");
        }
        
        definirPosiciones();
        Serial.println("Sistema listo para ingresar tapas.");
        stateMachine = IDLE;
        Serial.println("MODE_CONFIRMED");
      }
      else if (line == "RETIRAR") {
        modoOperacion = MODE_RETIRAR;
        Serial.println("Modo seleccionado: RETIRAR");
        
        const float DEG_TO_STEPS = 2048.0 / 360.0;
        long target180 = (long)(180 * DEG_TO_STEPS);
        
        // La posición base para RETIRAR es 180°
        if (currentPositionSteps != target180) {
          Serial.println("Moviendo a posición 180° (nuevo punto de referencia para RETIRAR)...");
          stepper.setSpeed(12);
          long delta = target180 - currentPositionSteps;
          stepper.step(delta);
          currentPositionSteps = target180;
          Serial.println("Stepper en posición 180° - Punto de referencia ajustado");
        } else {
          Serial.println("Stepper ya está en posición 180°");
        }
        
        // Definir posiciones ANTES de encender motor
        definirPosiciones();
        
        // ENCENDER motor de salida
        encenderMotorSalida();
        
        Serial.println("Sistema listo para retirar tapas.");
        Serial.println("Esperando selección de compartimiento...");
        
        // Ir directo a esperar selección de compartimiento
        stateMachine = WAIT_FOR_DESTINATION;
        
        Serial.println("MODE_CONFIRMED");
        Serial.println("READY");  // Habilitar botones INMEDIATAMENTE
      }
    }
    return;  // No hacer nada más hasta que se seleccione modo
  }

  // Leer sensores solo si ya se seleccionó modo
  distance1 = medirDistancia(TRIG1, ECHO1);
  
  // --- IDLE ---
  if (stateMachine == IDLE) {
    if (distance1 > 0 && distance1 < UMBRAL_SENSOR1) {
      // Solo en modo INGRESAR se usa el motor de entrada
      if (modoOperacion == MODE_INGRESAR) {
        encenderMotorEntrada();
        stateMachine = MOTOR_ON;
        Serial.println("Sensor1 detectó tapa -> Motor de entrada encendido (MOTOR_ON)");
        delay(100);
      }
      // En modo RETIRAR no se hace nada con el sensor 1
      // porque el motor de salida ya está encendido
    }
  }

  // --- MOTOR_ON ---
    else if (stateMachine == MOTOR_ON) {
      // Esperar 4 segundos con la banda encendida
      Serial.println("Motor de entrada encendido, transportando tapa...");
      delay(4000);  // Tiempo fijo para que la tapa llegue al paso a paso
      
      detenerMotorEntrada();
      stateMachine = WAIT_FOR_DESTINATION;
      Serial.println("Tiempo de transporte completado -> Motor de entrada detenido (WAIT_FOR_DESTINATION)");
      Serial.println("READY");
    }


  // --- WAIT_FOR_DESTINATION ---
  else if (stateMachine == WAIT_FOR_DESTINATION) {
    if (Serial.available() > 0) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.length() > 0) {
        int num = line.toInt();
        if (num >= 1 && num <= 4) {
          destino = num;
          Serial.print("Destino recibido: ");
          Serial.println(destino);
          stateMachine = MOVING;
        }
      }
    }
  }

  // --- MOVING ---
  else if (stateMachine == MOVING) {
    moverAPosicion(destino);
    Serial.println("Stepper en posición del compartimiento.");
    
    if (modoOperacion == MODE_INGRESAR) {
      Serial.println("Iniciando empuje (modo INGRESAR)...");
      stateMachine = PUSHING;
    } 
    else if (modoOperacion == MODE_RETIRAR) {
      // === SECUENCIA AUTOMÁTICA PARA RETIRAR ===
      Serial.println("Activando servo del compartimiento para retirar tapa...");
      empujarTapaRetirar(destino);  // Empuja la tapa con el servo
      Serial.println("Tapa empujada. Esperando caída...");
      delay(1500);  // Espera 1.5 segundos para que la tapa caiga (ajustable)
      
      Serial.println("Regresando stepper a posición base (180°)...");
      volverABase();
      Serial.println("Stepper regresó a base (180°).");
      
      // Activar el servo principal (pin 21) para empujar la tapa a la banda de salida
      Serial.println("Activando servo principal para empujar tapa a banda...");
      servoEmpujador.write(0);
      delay(600);
      servoEmpujador.write(180);
      delay(600);
      Serial.println("Servo principal completó el empuje a banda.");
      
      // ===== NUEVA FUNCIONALIDAD =====
      // Esperar 1 segundo más con la banda encendida para que la tapa se mueva
      Serial.println("Banda transportando tapa durante 1 segundo...");
      delay(30);
      
      // DETENER motor de salida
      detenerMotorSalida();
      Serial.println("Motor de salida DETENIDO - Tapa depositada correctamente.");
      // ===== FIN NUEVA FUNCIONALIDAD =====
      
      Serial.println("MOVE_DONE");
      
      // Fin de ciclo automático
      modoOperacion = MODE_NONE;
      stateMachine = WAITING_MODE;
      Serial.println("Ciclo RETIRAR completado. Esperando nuevo modo...");
      Serial.println("MODE_REQUEST");
    }
  }

  // --- PUSHING (solo para modo INGRESAR) ---
  else if (stateMachine == PUSHING) {
    empujarTapa();  // Usar servo principal
    Serial.println("MOVE_DONE");
    
    // IMPORTANTE: Regresar el stepper a su posición base ANTES de pedir nuevo modo
    Serial.println("Regresando stepper a posición base...");
    volverABase();
    Serial.println("Stepper en posición base.");
    
    // Volver a esperar selección de modo
    modoOperacion = MODE_NONE;
    stateMachine = WAITING_MODE;
    Serial.println("Ciclo completado. Esperando nuevo modo...");
    Serial.println("MODE_REQUEST");
  }

  delay(80);
}