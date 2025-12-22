#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

static const char* BT_NAME = "ESP32-ROBO";

// ====== CONTROLES DE MODO ======
// true  -> simula ADC (0..1023 aleatório)
// false -> mede ADC real no pino ADC_PIN
static const bool SIMULAR_ADC = true;

// Pino do ADC real (ajuste se SIMULAR_ADC = false)
// GPIO34 é somente entrada e bom para ADC
static const int ADC_PIN = 34;

// ADC 10 bits (0..1023)
static const int ADC_BITS = 10;
static const int ADC_MAX  = (1 << ADC_BITS) - 1; // 1023

// ====== PINOS DOS LEDs / DIREÇÕES (conforme você informou) ======
static const int PIN_UP    = 23; // VD (Up)
static const int PIN_RIGHT = 22; // AZ (Right)
static const int PIN_DOWN  = 32; // AM (Down)
static const int PIN_LEFT  = 33; // VM (Left)

// true para acender LEDs agora, ou false para só ver comandos no Serial
static const bool CONTROLAR_PINS = true;

// Temporização do envio do ADC
unsigned long lastAdcSend = 0;
static const unsigned long ADC_PERIOD_MS = 2000;

String line;

void allOff() {
  if (!CONTROLAR_PINS) return;
  digitalWrite(PIN_UP, LOW);
  digitalWrite(PIN_RIGHT, LOW);
  digitalWrite(PIN_DOWN, LOW);
  digitalWrite(PIN_LEFT, LOW);
}

void cmdOn(char c) {
  if (!CONTROLAR_PINS) return;

  allOff();
  switch (c) {
    case 'U': digitalWrite(PIN_UP, HIGH);    break;
    case 'R': digitalWrite(PIN_RIGHT, HIGH); break;
    case 'D': digitalWrite(PIN_DOWN, HIGH);  break;
    case 'L': digitalWrite(PIN_LEFT, HIGH);  break;
    default: break;
  }
}

void handleCommand(const String& cmd) {
  if (cmd.length() == 0) return;

  Serial.print("CMD: ");
  Serial.println(cmd);

  char c = cmd.charAt(0);

  if (c == 'S') {  // Stop
    allOff();
    return;
  }

  if (c == 'U' || c == 'R' || c == 'D' || c == 'L') {
    cmdOn(c);
  }
}

int readAdc10bit() {
  int adc = 0;

  if (SIMULAR_ADC) {
    // Simulação (bom para testar app e comunicação)
    adc = (int)random(0, 1024); // 0..1023
  } else {
    // Medição real
    adc = analogRead(ADC_PIN);  // 0..1023 com resolução setada
  }

  if (adc < 0) adc = 0;
  if (adc > ADC_MAX) adc = ADC_MAX;
  return adc;
}

void sendAdc() {
  int adc = readAdc10bit();

  // Envia para o app (somente se houver cliente conectado)
  if (SerialBT.hasClient()) {
    SerialBT.print("ADC=");
    SerialBT.print(adc);
    SerialBT.print("\n");
  }

  // Debug no Serial Monitor
  Serial.print("TX ADC=");
  Serial.print(adc);
  Serial.print(" (");
  Serial.print(SIMULAR_ADC ? "SIMULADO" : "REAL");
  Serial.println(")");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Semente para simulação aleatória
  randomSeed((uint32_t)esp_random());

  // LEDs
  if (CONTROLAR_PINS) {
    pinMode(PIN_UP, OUTPUT);
    pinMode(PIN_RIGHT, OUTPUT);
    pinMode(PIN_DOWN, OUTPUT);
    pinMode(PIN_LEFT, OUTPUT);
    allOff();
  }

  // ADC real (somente relevante se SIMULAR_ADC = false)
  analogReadResolution(ADC_BITS);
  // Opcional para leitura até ~3.3V (varia por placa/sensor):
  // analogSetAttenuation(ADC_11db);

  // Bluetooth
  SerialBT.begin(BT_NAME);

  Serial.println("====================================");
  Serial.print("Bluetooth SPP ativo. Nome: ");
  Serial.println(BT_NAME);
  Serial.println("Comandos (1 por linha): U R D L S");
  Serial.println("U=Up(GPIO23), R=Right(GPIO22), D=Down(GPIO32), L=Left(GPIO33), S=Stop");
  Serial.print("Modo ADC: ");
  Serial.println(SIMULAR_ADC ? "SIMULADO (random 0..1023)" : "REAL (analogRead)");
  Serial.print("ADC real pino: GPIO");
  Serial.println(ADC_PIN);
  Serial.print("Enviando ADC a cada ");
  Serial.print(ADC_PERIOD_MS);
  Serial.println(" ms no formato: ADC=xxx");
  Serial.println("====================================");
}

void loop() {
  // 1) Receber comandos
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();
    if (c == '\r') continue;

    if (c == '\n') {
      handleCommand(line);
      line = "";
    } else {
      if (line.length() < 16) line += c;
    }
  }

  // 2) Enviar ADC a cada 2 s
  unsigned long now = millis();
  if (now - lastAdcSend >= ADC_PERIOD_MS) {
    lastAdcSend = now;
    sendAdc();
  }
}
