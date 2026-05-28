#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

#define DHTPIN 15
#define DHTTYPE DHT22
#define PIR_PIN 13
#define MQ135_PIN 34
#define WATER_PIN 35
#define RELE_LUZ 2
#define RELE_VENT 4
#define SERVO_PIN 18
#define BUZZER_PIN 19
#define RGB_R 25  
#define RGB_G 26
#define RGB_B 27
#define BTN_SEG 32
#define BTN_LUZ 33
#define BTN_SERVO 14

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";
const String prefijo = "udb/gj111587/"; 

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo cortina;
WiFiClient espClient;
PubSubClient client(espClient);

bool sistemaArmado = false;
bool alertaActiva = false;
unsigned long ultimoEnvio = 0;
const long intervaloEnvio = 3000; 

float luxes = 250.0; 

// 🔧 CONTROL DE ESTADOS LÓGICOS ESTABLES
bool cortinaAbierta = false; 

bool ultimoEstadoBtnSeg = HIGH;
bool ultimoEstadoBtnLuz = HIGH;
bool ultimoEstadoBtnServo = HIGH;

void setup() {
  Serial.begin(115200);
  
  pinMode(RELE_LUZ, OUTPUT);
  pinMode(RELE_VENT, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  
  pinMode(PIR_PIN, INPUT);
  pinMode(BTN_SEG, INPUT_PULLUP);
  pinMode(BTN_LUZ, INPUT_PULLUP);
  pinMode(BTN_SERVO, INPUT_PULLUP);

  dht.begin();
  cortina.attach(SERVO_PIN);
  cortina.write(0); 
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  lcd.clear();
  lcd.print("Wi-Fi OK");
  delay(500);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) { mensaje += (char)payload[i]; }
  
  String subLuz = prefijo + "domotica/sala/actuadores/iluminacion";
  String subVent = prefijo + "domotica/sala/actuadores/ventilacion";
  String subCort = prefijo + "domotica/sala/actuadores/cortina";

  if (String(topic) == subLuz) {
    digitalWrite(RELE_LUZ, mensaje == "ON" ? HIGH : LOW);
  }
  if (String(topic) == subVent) {
    digitalWrite(RELE_VENT, mensaje == "ON" ? HIGH : LOW);
  }
  if (String(topic) == subCort) {
    // Sincronizar la variable de estado cuando la orden viene desde la web
    cortinaAbierta = (mensaje == "OPEN");
    cortina.write(cortinaAbierta ? 90 : 0);
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-UDB-";
    clientId += String(random(0, 0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("¡CONECTADO CON ÉXITO!");
      
      String sub1 = prefijo + "domotica/sala/actuadores/iluminacion";
      String sub2 = prefijo + "domotica/sala/actuadores/ventilacion";
      String sub3 = prefijo + "domotica/sala/actuadores/cortina";
      String pubStatus = prefijo + "domotica/sala/seguridad_estado";

      client.subscribe(sub1.c_str());
      client.subscribe(sub2.c_str());
      client.subscribe(sub3.c_str());
      client.publish(pubStatus.c_str(), sistemaArmado ? "ARMADO" : "DESARMADO");
    } else {
      delay(3000);
    }
  }
}

void fijarColorRGB(int r, int g, int b) {
  analogWrite(RGB_R, r);
  analogWrite(RGB_G, g);
  analogWrite(RGB_B, b);
}

void loop() {
  if (!client.connected()) { reconnect(); }
  client.loop();

  // --- GESTIÓN DE BOTONES LOCALES ---
  bool estadoBtnSeg = digitalRead(BTN_SEG);
  if (estadoBtnSeg == LOW && ultimoEstadoBtnSeg == HIGH) {
    delay(20);
    if (digitalRead(BTN_SEG) == LOW) {
      sistemaArmado = !sistemaArmado;
      String pubStatus = prefijo + "domotica/sala/seguridad_estado";
      client.publish(pubStatus.c_str(), sistemaArmado ? "ARMADO" : "DESARMADO");
    }
  }
  ultimoEstadoBtnSeg = estadoBtnSeg;

  bool estadoBtnLuz = digitalRead(BTN_LUZ);
  if (estadoBtnLuz == LOW && ultimoEstadoBtnLuz == HIGH) {
    delay(20);
    if (digitalRead(BTN_LUZ) == LOW) {
      digitalWrite(RELE_LUZ, !digitalRead(RELE_LUZ));
      String t7 = prefijo + "domotica/sala/actuadores/iluminacion";
      client.publish(t7.c_str(), digitalRead(RELE_LUZ) ? "ON" : "OFF");
    }
  }
  ultimoEstadoBtnLuz = estadoBtnLuz;

  bool estadoBtnServo = digitalRead(BTN_SERVO);
  if (estadoBtnServo == LOW && ultimoEstadoBtnServo == HIGH) {
    delay(20);
    if (digitalRead(BTN_SERVO) == LOW) {
      // Toggle usando la variable de estado protegida
      cortinaAbierta = !cortinaAbierta;
      cortina.write(cortinaAbierta ? 90 : 0);
      
      // Publicar inmediatamente a internet el cambio real
      String t9 = prefijo + "domotica/sala/actuadores/cortina";
      client.publish(t9.c_str(), cortinaAbierta ? "OPEN" : "CLOSE");
      Serial.println(cortinaAbierta ? "Persiana: Abierta físicamente" : "Persiana: Cerrada físicamente");
    }
  }
  ultimoEstadoBtnServo = estadoBtnServo;

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int gasRaw = analogRead(MQ135_PIN);
  int aguaRaw = analogRead(WATER_PIN);
  int movimiento = digitalRead(PIR_PIN);

  if (isnan(h)) h = 0.0;
  if (isnan(t)) t = 0.0;

  if (t > 25.0 && t != 0.0) { digitalWrite(RELE_VENT, HIGH); } 
  else { digitalWrite(RELE_VENT, LOW); }

  if (gasRaw > 2000 || aguaRaw > 1500) {
    alertaActiva = true;
    fijarColorRGB(255, 0, 0); 
    digitalWrite(BUZZER_PIN, (millis() / 150) % 2); 
  } 
  else if (sistemaArmado && movimiento == HIGH) {
    alertaActiva = true;
    fijarColorRGB(0, 0, 255); 
    digitalWrite(BUZZER_PIN, HIGH); 
  } else {
    alertaActiva = false;
    fijarColorRGB(0, 255, 0); 
    digitalWrite(BUZZER_PIN, LOW);
  }

  lcd.setCursor(0, 0);
  lcd.print("T:" + String(t, 1) + "C H:" + String(h, 0) + "%   ");
  lcd.setCursor(0, 1);
  if (alertaActiva) { lcd.print("!! ALERTA !!    "); } 
  else { lcd.print(sistemaArmado ? "SEG: ARMADO     " : "SEG: DESARMADO  "); }

  // --- REPORTE PERIÓDICO GENERAL BLINDADO ---
  unsigned long ahora = millis();
  if (ahora - ultimoEnvio > intervaloEnvio) {
    ultimoEnvio = ahora;
    
    String t1 = prefijo + "domotica/sala/temperatura";
    String t2 = prefijo + "domotica/sala/humedad";
    String t3 = prefijo + "domotica/sala/fuga_agua";
    String t4 = prefijo + "domotica/sala/calidad_aire";
    String t5 = prefijo + "domotica/sala/movimiento";
    String t6 = prefijo + "domotica/sala/seguridad_estado";
    String t7 = prefijo + "domotica/sala/actuadores/iluminacion";
    String t8 = prefijo + "domotica/sala/actuadores/ventilacion";
    String t9 = prefijo + "domotica/sala/actuadores/cortina";

    client.publish(t1.c_str(), String(t).c_str());
    client.publish(t2.c_str(), String(h).c_str());
    client.publish(t3.c_str(), String(aguaRaw).c_str());
    client.publish(t4.c_str(), String(gasRaw).c_str());
    client.publish(t5.c_str(), movimiento == HIGH ? "1" : "0");
    client.publish(t6.c_str(), sistemaArmado ? "ARMADO" : "DESARMADO");
    client.publish(t7.c_str(), digitalRead(RELE_LUZ) ? "ON" : "OFF");
    client.publish(t8.c_str(), digitalRead(RELE_VENT) ? "ON" : "OFF");
    
    // Envío del estado lógico real blindado
    client.publish(t9.c_str(), cortinaAbierta ? "OPEN" : "CLOSE");
  }
}
