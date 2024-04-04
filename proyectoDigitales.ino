#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "DHT.h"

#define DHTPIN 4
#define DHTTYPE DHT11   // DHT 11
#define WIFI_SSID "WiFi SSID"
#define WIFI_PASSWORD "contraseña WiFi"
#define BOT_TOKEN "api"
#define CHAT_ID "codigo"

#define VENTILADOR_PIN 12 // Pin para controlar el ventilador
#define BOMBA_AGUA_PIN 13 // Pin para controlar la bomba de agua
#define LDR_PIN 34         // Pin de la fotorresistencia
#define LED_PIN 2          // Pin del LED

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
DHT dht(DHTPIN, DHTTYPE);

const unsigned long SENSOR_INTERVAL = 2000; // Intervalo de lectura del sensor (2 segundos)
const unsigned long TELEGRAM_INTERVAL = 60000; // Intervalo de envío al chat de Telegram (60 segundos)
const int NUM_SAMPLES = TELEGRAM_INTERVAL / SENSOR_INTERVAL; // Número de muestras en un minuto
unsigned long last_sensor_time = 0;
unsigned long last_telegram_time = 0;
float temperatureC;
float humidity;
float lightIntensity;
float tempSum = 0.0;
float humiditySum = 0.0;
int sampleCount = 0;
bool ventilador_encendido = false; // Estado del ventilador
bool bomba_agua_encendida = false; // Estado de la bomba de agua
bool led_encendido = false; // Estado del LED

void setup() {
  Serial.begin(9600);
  Serial.println(F("DHTxx test!"));
  dht.begin();
  Serial.print("Conectando al WiFi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Agregar certificado raíz para api.telegram.org
  pinMode(VENTILADOR_PIN, OUTPUT);
  pinMode(BOMBA_AGUA_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi conectado, direccion IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long current_time = millis();

  // Leer el sensor cada 2 segundos
  if (current_time - last_sensor_time >= SENSOR_INTERVAL) {
    temperatureC = dht.readTemperature();
    humidity = dht.readHumidity();
    lightIntensity = analogRead(LDR_PIN);

    Serial.println(temperatureC);
    Serial.println(humidity);
    Serial.println(lightIntensity);

    tempSum += temperatureC;
    humiditySum += humidity;
    sampleCount++;
    last_sensor_time = current_time;
    
    // Controlar el ventilador según la temperatura
    if (temperatureC > 24.20) {
      digitalWrite(VENTILADOR_PIN, HIGH); // Encender el ventilador
      if (!ventilador_encendido) {
        sendAlertToTelegram();
        ventilador_encendido = true;
      }
    } else if (temperatureC < 24.20 && ventilador_encendido) {
      digitalWrite(VENTILADOR_PIN, LOW); // Apagar el ventilador
      sendVentiladorOffToTelegram();
      ventilador_encendido = false;
    }

    // Controlar la bomba de agua según la humedad
    if (humidity < 80 && !bomba_agua_encendida) {
      digitalWrite(BOMBA_AGUA_PIN, HIGH); // Encender la bomba de agua
      bomba_agua_encendida = true;
      sendBombaAguaEncendidaToTelegram();
    } else if (humidity >= 80 && bomba_agua_encendida) {
      digitalWrite(BOMBA_AGUA_PIN, LOW); // Apagar la bomba de agua
      bomba_agua_encendida = false;
      sendBombaAguaApagadaToTelegram();
    }

    // Controlar el LED según la intensidad de luz
    if (lightIntensity < 1000 && !led_encendido) { // Si la luz es baja
      digitalWrite(LED_PIN, HIGH); // Encender el LED
      sendLedOnToTelegram();
      led_encendido = true;
    } else if (lightIntensity >= 1000 && led_encendido) {
      digitalWrite(LED_PIN, LOW); // Apagar el LED
      sendLedOffToTelegram();
      led_encendido = false;
    }

    // Enviar datos al chat de Telegram al finalizar la lectura del sensor
    if (current_time - last_telegram_time >= TELEGRAM_INTERVAL) {
      float avgTemp = tempSum / sampleCount;
      float avgHumidity = humiditySum / sampleCount;
      // Eliminamos el promedio de luz
      sendSensorDataToTelegram(avgTemp, avgHumidity);
      // Reiniciar las sumas y el contador de muestras para el siguiente minuto
      tempSum = 0.0;
      humiditySum = 0.0;
      sampleCount = 0;
      last_telegram_time = current_time;
    }
  }
}

void sendSensorDataToTelegram(float avgTemp, float avgHumidity) {
  String message = "Actualizacion de estado:\n";
  message += "Temperatura: " + String(avgTemp) + "°C\n";
  message += "Humedad: " + String(avgHumidity) + "%";

  if (bot.sendMessage(CHAT_ID, message, "")) {
    Serial.println("Mensaje enviado correctamente");
  } else {
    Serial.println("Fallo al enviar el mensaje");
  }
}

void sendAlertToTelegram() {
  String alertMessage = "¡Alerta! La temperatura supera los 30 grados. Se ha encendido el ventilador.\n";
  alertMessage += "Temperatura: " + String(temperatureC) + "°C\n";
  if (bot.sendMessage(CHAT_ID, alertMessage, "")) {
    Serial.println("Mensaje ventilador encendido enviado");
  } else {
    Serial.println("Fallo al enviar el mensaje ventilador encendido");
  }
}

void sendVentiladorOffToTelegram() {
  String alertMessage = "La temperatura es menor a los 30 grados, el ventilador se ha apagado\n";
  alertMessage += "Temperatura: " + String(temperatureC) + "°C\n";
  if (bot.sendMessage(CHAT_ID, alertMessage, "")) {
    Serial.println("Mensaje ventilador apagado enviado");
  } else {
    Serial.println("Fallo al enviar mensaje ventilador apagado");
  }
}

void sendBombaAguaEncendidaToTelegram() {
  String alertMessage = "¡Alerta! La humedad es menor a 60%. Se ha encendido la bomba de agua.\n";
  alertMessage += "Humedad: " + String(humidity) + "%\n";
  if (bot.sendMessage(CHAT_ID, alertMessage, "")) {
    Serial.println("Mensaje bomba de agua encendida enviado");
  } else {
    Serial.println("Fallo al enviar el mensaje bomba de agua encendida");
  }
}

void sendBombaAguaApagadaToTelegram() {
  String alertMessage = "La humedad es mayor o igual a 70%. Se ha apagado la bomba de agua.\n";
  alertMessage += "Humedad: " + String(humidity) + "%\n";
  if (bot.sendMessage(CHAT_ID, alertMessage, "")) {
    Serial.println("Mensaje bomba de agua apagada enviado");
  } else {
    Serial.println("Fallo al enviar el mensaje bomba de agua apagada");
  }
}

void sendLedOnToTelegram() {
  if (bot.sendMessage(CHAT_ID, "El LED se ha encendido", "")) {
    Serial.println("Mensaje LED encendido enviado");
  } else {
    Serial.println("Fallo al enviar el mensaje LED encendido");
  }
}

void sendLedOffToTelegram() {
  if (bot.sendMessage(CHAT_ID, "El LED se ha apagado", "")) {
    Serial.println("Mensaje LED apagado enviado");
  } else {
    Serial.println("Fallo al enviar el mensaje LED apagado");
  }
}
