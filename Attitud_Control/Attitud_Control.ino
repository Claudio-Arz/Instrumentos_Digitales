/*
2026-02-24 21:27:45
Versión 4.0
Claudio Arzamendia Systems
Tablero completo con instrumental aeronáutico
para simular un avión Cessna 172.






*/

#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <time.h>  // Para NTP (hora de internet)
#include <ESP_Mail_Client.h>  // Para enviar emails

// Configuración SMTP para enviar email con la IP
// IMPORTANTE: Genera una "App Password" en Yahoo (Configuración -> Seguridad -> Generar contraseña de app)
#define SMTP_HOST "smtp.mail.yahoo.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "claudio_arz@yahoo.com"  // Tu email de Yahoo
#define RECIPIENT_EMAIL_1 "claudio_arz@yahoo.com" // Destinatarios del email 
// #define RECIPIENT_EMAIL_2 "luis_arz@yahoo.com.ar" // Destinatarios del email 
#define AUTHOR_PASSWORD "cmfo zouz fdjp qard" // App Password de Yahoo (NO tu contraseña normal)

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -3 * 3600;  // UTC-3 (Paraguay). Ajusta según tu zona horaria
const int   daylightOffset_sec = 0;     // Sin horario de verano (0) o con horario de verano (3600)

// Objeto SMTP para enviar emails
SMTPSession smtp;

// Función para enviar email con la IP del ESP32
void enviarEmailConIP(String ipAddress) {
  Serial.println("Enviando email con la IP...");
  
  // Configurar sesión SMTP
  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.time.ntp_server = F("pool.ntp.org");
  config.time.gmt_offset = -3;  // UTC-3 (Paraguay)
  config.time.day_light_offset = 0;
  
  // Preparar mensaje
  SMTP_Message message;
  message.sender.name = F("AeroDeck ESP32");
  message.sender.email = AUTHOR_EMAIL;
  message.subject = F("AeroDeck - IP del Tablero de Instrumentos");
  message.addRecipient(F("Claudio"), RECIPIENT_EMAIL_1);
  // message.addRecipient(F("Luis"), RECIPIENT_EMAIL_2);
  
  // Contenido del email
  String htmlContent = "<h2>AeroDeck Tablero de Instrumentos</h2>";
  htmlContent += "<p>El sistema se ha conectado exitosamente a WiFi.</p>";
  htmlContent += "<p><strong>IP asignada:</strong> <a href='http://" + ipAddress + "'>" + ipAddress + "</a></p>";
  htmlContent += "<p>Haz clic en el enlace para acceder al tablero.</p>";
  htmlContent += "<hr><p><small>Claudio Arzamendia Systems - AeroDeck v4.00</small></p>";
  htmlContent += "<img src='https://claudio-arz.github.io/AeroDeck-HTML/Images/ClaudioArzamendiaSystems.png' alt='Claudio Arzamendia Systems Ver 4.00' style='width:350px; height:auto; margin-top:10px;'>";
  
  message.html.content = htmlContent.c_str();
  message.html.charSet = F("utf-8");
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  
  // Conectar y enviar
  if (!smtp.connect(&config)) {
    Serial.println("Error conectando al servidor SMTP: " + smtp.errorReason());
    return;
  }
  
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error enviando email: " + smtp.errorReason());
  } else {
    Serial.println("Email enviado exitosamente!");
  }
  
  smtp.closeSession();
}

// ===== HTML COMPLETO EMBEBIDO =====
#include "HTML/mainHTML.cpp"
// ===== OBJETOS =====
WebServer server(80);
WebSocketsServer ws(81);
DNSServer dnsServer;

// ===== VARIABLES GLOBALES =====
float verSpeedValue = 0.0f; // Velocidad vertical en pies por minuto mostrada por el variómetro
float altitudValue = 0.0f; // Altitud en pies mostrada por el altímetro
bool bandera_off = false; // Indica si el altímetro NO debe mostrar bandera_off
bool usePitchDrivenVerticalSpeed = true; // true: Attitude Control (pitch) gobierna el variómetro
float atmosphericPressureHpa = 1000.0f; // Presión atmosférica (Kollsman) en hPa
const float SEA_LEVEL_PRESSURE_HPA = 1013.25f; // Presión estándar a nivel del mar (ISA)
float RPMValue = 0.0f; // Valor de RPM mostrado por el instrumento RPM
float RPMBase = 0.0f; // Valor de RPM sin ruido, para cálculos internos
float throttleValue = 0.0f; // Posición del throttle 0-100%
bool throttleControlEnabled = false; // Se activa cuando llega valor de throttle desde UI
bool RPMNoise = false; // Indica si el instrumento RPM debe mostrar ruido (noise) o no
bool RPMStarted = false; // Indica si el motor está encendido (RPM > 0)
bool useSimulatedRPM = false; // true: bloquea control manual de RPM desde UI
bool guardoRPMBase = false; // Indica si el valor base de RPM ha sido guardado
bool startRoutine = false; // Indica si la rutina automática de arranque está activa
unsigned long routineStart = 0; // Tiempo de inicio de la rutina
int routineStep = 0; // Paso actual de la rutina
float routineInitial = 0.0f; // Valor inicial de RPM para la rutina
float varRPM = 0.0f; // Valor variable de RPM durante la rutina
// Variables globales para Horizonte Artificial
float pitchValue = 0.0f; // Valor de pitch mostrado por el horizonte artificial
float rollValue = 0.0f; // Valor de roll mostrado por el horizonte artificial

// Variables para contador de horas de funcionamiento (Drum-Roll)
unsigned long horasStartMillis = 0; // Tiempo de inicio del tablero
int horasFuncionamiento = 0; // Horas acumuladas (0-999)
int minutosFuncionamiento = 0; // Minutos acumulados (0-59)

// Variables para manejo de Airspeed
float airspeedValue = 40.0f; // Valor de velocidad aérea mostrado por el instrumento de Air Speed
float storedAirspeedValue = 0.0f; // Airspeed guardado cuando frenos activados
bool useSimulatedAirspeed = false; // true: bloquea control manual de Air Speed desde UI

// Variables para manejo de Gyro
float gyroValue = 0.0f; // Valor de heading mostrado por el instrumento Gyro
bool useSimulatedGyro = true; // true: calcula Gyro en el ESP32, false: usa gyroValue recibido por websocket
unsigned long lastGyroMillis = 0; // Tiempo de última actualización del Gyro
bool brakeOn = false; // true: frenos activos, no se actualiza heading
bool lastBrakeOnState = false; // Estado previo de frenos para detectar cambios
const float TAXI_RPM_THRESHOLD = 800.0f; // RPM minimo para carretear
const float TAXI_IAS_MAX = 5.0f; // Kts maximos para considerar rodaje
const float RPM_MAX_FIXED_PITCH = 2700.0f; // RPM max para helice de paso fijo
const float AIRSPEED_TAKEOFF_MAX = 40.0f; // Kts max mientras potencia 25-35%
const float AIRSPEED_FLIGHT_MIN = 40.0f; // Kts min a partir de 75% potencia
const float AIRSPEED_FLIGHT_MAX = 120.0f; // Kts objetivo max a 100% potencia
const float AIRSPEED_RAMP_ALPHA = 0.05f; // Suavizado de rampa
const float AIRSPEED_GLIDE_MIN = 45.0f; // Kts min de planeo con motor apagado
const float AIRSPEED_GLIDE_MAX = 85.0f; // Kts max de planeo con morro abajo

// Variables para manejo de Turn Coordinator
float turnCoordPitch = 0.0f; // Valor de pitch mostrado por el Turn Coordinator (rudder input)
float tcBallValue = 0.0f; // Slip/Skid ball: posición visual [-30..30]
float tcBallNormalized = 0.0f; // Estado interno: aceleración lateral normalizada [-1..1]
float tcYawRateStateRadSec = 0.0f; // Estado dinámico de yaw rate para simular inercia
float tcPrevBankRad = 0.0f; // Estado previo de bank para estimar tasa de alabeo
float tcRollRateStateRadSec = 0.0f; // Estado dinámico de roll rate (inercia lateral)
unsigned long lastTCBallMillis = 0; // Tiempo de última actualización del ball
// NOTA: turnCoordRoll eliminado - ahora comparte rollValue con Attitude Coordinator

// Constantes para cálculo del slip/skid ball (física real)
const float TC_BALL_SENSITIVITY = 1.2f; // Sensibilidad del instrumento (k)
const float TC_BALL_DAMPING = 4.0f; // Amortiguación del ball (ajustable para suavidad)
const float GRAVITY = 9.81f; // Aceleración gravitacional (m/s²)
const float GRAVITY_CORRECTION = 0.3048f; // Conversión de pies/s² a m/s² (1 pie = 0.3048m)

// Variables para manejo de Fuel Flow
float fuelFlowValue = 0.0f; // Valor de Fuel Flow mostrado por el instrumento de Fuel Flow
bool useSimulatedFuelFlow = true; // true: calcula Fuel Flow en el ESP32, false: usa fuelFlowValue recibido
unsigned long lastFuelFlowMillis = 0;

// Coeficientes Fuel Flow (modelo en GPH)
const float FF_C0 = 1.0f;
const float FF_C1 = 0.005f;
const float FF_C2 = 2.5f;
const float FF_TEMP_COEFF = 0.02f;
const float FF_IAS_COEFF = -0.02f;
const float FF_ALPHA = 0.05f;
const float FF_MIN = 0.0f;
const float FF_MAX = 20.0f;

// Variables para manejo de Fuel Flow
float manifoldValue = 10.0f; // Valor de Manifold Pressure mostrado por el instrumento de Manifold Pressure
bool useSimulatedManifold = true; // true: calcula Manifold en el ESP32, false: usa manifold recibido por websocket

// Variables para manejo de Oil Pressure
float oilPressValue = 0.0f; // Valor de Oil Pressure mostrado por el instrumento de Oil Pressure
bool useSimulatedOilPress = true; // true: calcula Oil Press en el ESP32, false: usa oilPressValue recibido
unsigned long lastOilPressMillis = 0;

// Coeficientes Oil Pressure (modelo en PSI)
const float OIL_PRESS_C0 = 5.0f;
const float OIL_PRESS_C1 = 0.025f;
const float OIL_PRESS_TEMP_COEFF = -0.15f;
const float OIL_PRESS_ALPHA = 0.08f;
const float OIL_PRESS_MIN = 0.0f;
const float OIL_PRESS_MAX = 120.0f;

// Variables para manejo de Oil Temperature
float oilTempValue = 0.0f; // Valor de Oil Temperature mostrado por el instrumento de Oil Temperature
bool useSimulatedOilTemp = true; // true: calcula Oil Temp en el ESP32, false: usa oilTempValue recibido
unsigned long lastOilTempMillis = 0;

// Coeficientes Oil Temperature (modelo en °F e IAS en knots)
const float OIL_C0_F = 50.0f;
const float OIL_C1_F = 95.0f;
const float OIL_C2_F = 25.0f;
const float OIL_C3_F_PER_KT = 0.25f;
const float OIL_ALPHA = 0.015f;
const float OIL_MIN_F = 0.0f;
const float OIL_MAX_F = 250.0f;

// Variables para manejo de CHT
float chtValue = 0.0f; // Valor de CHT mostrado por el instrumento de CHT
bool useSimulatedCHT = true; // true: calcula CHT en el ESP32, false: usa chtValue recibido por websocket
float chtMixture = 0.45f; // Mezcla normalizada [0..1], 0.5 ~= peak
unsigned long lastCHTMillis = 0;

// Coeficientes CHT (modelo en °F y IAS en knots)
const float CHT_C0_F = 230.0f;
const float CHT_C1_F = 171.0f;
const float CHT_C2_F = 45.0f;
const float CHT_C3_F_PER_KT = 0.396f;
const float CHT_ALPHA = 0.03f;
const float CHT_MIN_F = 0.0f;
const float CHT_MAX_F = 500.0f;

// Variables para manejo de FUEL
float fuelValueLeft = 25.0f; // Valor de FUEL mostrado por el instrumento de FUEL Left (inicia a full)
float fuelValueRight = 25.0f; // Valor de FUEL mostrado por el instrumento de FUEL Right (inicia a full)
bool useSimulatedFuel = true; // true: consume combustible automáticamente
unsigned long lastFuelMillis = 0;
unsigned long lastTankSwitchMillis = 0;
const unsigned long TANK_SWITCH_INTERVAL = 600000; // 10 minutos en milisegundos
int activeTank = 1; // 1 = Left, 2 = Right
const float FUEL_TANK_CAPACITY = 25.0f; // Capacidad máxima de cada tanque en galones

// Variables para manejo de Reloj
int relojHoras = 0; // Horas mostradas por el instrumento Reloj
int relojMinutos = 0; // Minutos mostrados por el instrumento Reloj
int relojSegundos = 0; // Segundos mostrados por el instrumento Reloj

// Variables para manejo de Volt/Amp
float voltAmpValueLeft = 0.0f; // Valor de Volt/Amp mostrado (Voltaje)
float voltAmpValueRight = 0.0f; // Valor de Volt/Amp mostrado (Amperaje)
bool useSimulatedVoltage = true; // true: calcula Voltaje en el ESP32, false: usa voltAmpValueLeft recibido
unsigned long lastVoltageMillis = 0;

// Coeficientes Voltaje (modelo en Volts - rango 10-16V)
const float VOLT_BASE = 12.5f;           // Voltaje base (batería nominal)
const float VOLT_RPM_COEFF = 2.3f;       // Incremento por alternador a RPM alto
const float VOLT_AMP_COEFF = 0.008f;     // Caída por carga eléctrica (0.5V @ 60A)
const float VOLT_ALPHA = 0.1f;           // Constante de respuesta (moderada)
const float VOLT_MIN = 10.0f;            // Voltaje mínimo del instrumento
const float VOLT_MAX = 16.0f;            // Voltaje máximo del instrumento

// Coeficientes de throttle (C172 hélice fija)
const float THROTTLE_IDLE_RPM = 456.0f;
const float THROTTLE_MAX_RPM = 2700.0f;
const float THROTTLE_MAP_MIN = 10.0f;
const float THROTTLE_MAP_MAX = 29.0f;
const float THROTTLE_ALPHA = 0.08f;

// EGT coeficientes (modelo en °F e IAS en knots)
float egtValue = 950.0f; // Valor de EGT mostrado por el instrumento de EGT
float egtBugValue = 1450.0f; // Referencia móvil (bug) para ajuste de peak EGT
const float EGT_C0_F = 400.0f;
const float EGT_C1_F = 150.0f;
const float EGT_C2_F = 50.0f;
const float EGT_C3_F_PER_KT = 0.5f;
const float EGT_ALPHA = 0.02f;
const float EGT_MIN_F = 800.0f;
const float EGT_MAX_F = 1600.0f;
const float EGT_PEAK_MIXTURE = 0.45f; // Mezcla para peak EGT (modelo simple)
const float EGT_LEAN_RPM_THRESHOLD = 1500.0f; // RPM mínimo para que el motor pueda estar en mezcla lean
const float EGT_RICH_MIXTURE = 0.35f; // Mezcla considerada rica para cálculos de EGT
const float EGT_LEAN_MIXTURE = 0.55f; // Mezcla considerada lean para cálculos de EGT
const float EGT_RICH_TEMP = 1200.0f; // EGT aproximada a mezcla rica
const float EGT_LEAN_TEMP = 1400.0f; // EGT aproximada a mezcla lean
const float EGT_RPM_EFFECT_COEFF = 0.1f; // Incremento de EGT por RPM alto (modelo simple)
const float EGT_MIXTURE_EFFECT_COEFF = 200.0f; // Incremento de EGT por mezcla lean (modelo simple)
const float EGT_IASEFFECT_COEFF = 0.3f; // Incremento de EGT por IAS alta (modelo simple)
const float EGT_TEMP_EFFECT_COEFF = 0.5f; // Incremento de EGT por temperatura ambiente alta (modelo simple)
const float EGT_PEAK_WIDTH = 0.1f; // Ancho de la curva de peak EGT (modelo simple)
const float EGT_MIN_MIXTURE = 0.25f; // Mezcla mínima para cálculos de EGT (previene valores absurdos)
const float EGT_MAX_MIXTURE = 0.65f; // Mezcla máxima para cálculos de EGT (previene valores absurdos)
const float EGT_MIN_IAS = 0.0f; // IAS mínima para cálculos de EGT
const float EGT_MAX_IAS = 120.0f; // IAS máxima para cálculos de EGT
const float EGT_MIN_TEMP = -20.0f; // Temperatura mínima para cálculos de EGT
const float EGT_MAX_TEMP = 40.0f; // Temperatura máxima para cálculos de EGT
const float EGT_IDLE_TEMP = 300.0f; // EGT aproximada en ralentí (modelo simple)
const float EGT_IDLE_RPM_THRESHOLD = 500.0f; // RPM máximo para considerar que el motor está en ralentí
const float EGT_IDLE_TEMP_COEFF = 0.5f; // Incremento de EGT en ralentí (modelo simple)
const float EGT_RPM_EFFECT_IDLE_COEFF = 0.2f; // Incremento de EGT por RPM en ralentí (modelo simple)
const float EGT_MIXTURE_EFFECT_IDLE_COEFF = 100.0f; // Incremento de EGT por mezcla en ralentí (modelo simple)
const float EGT_TEMP_EFFECT_IDLE_COEFF = 0.3f; // Incremento de EGT por temperatura en ralentí (modelo simple)  
const float EGT_IASEFFECT_IDLE_COEFF = 0.1f; // Incremento de EGT por IAS en ralentí (modelo simple)
const float EGT_NOICE_AMPLITUDE = 15.0f; // Amplitud del ruido en EGT cuando RPMNoise está activo
const float EGT_NOICE_FREQUENCY = 0.5f; // Frecuencia del ruido en EGT cuando RPMNoise está activo (Hz)
const float EGT_NOICE_RPM_THRESHOLD = 1200.0f; // RPM mínimo para que el ruido en EGT esté activo
const float EGT_NOICE_MIXTURE_THRESHOLD = 0.4f; // Mezcla mínima para que el ruido en EGT esté activo
const float EGT_NOICE_IASEFFECT_THRESHOLD = 60.0f; // IAS mínima para que el ruido en EGT esté activo
const float EGT_NOICE_TEMP_EFFECT_THRESHOLD = 20.0f; // Temperatura mínima para que el ruido en EGT esté activo
const float EGT_NOICE_EFFECT_COEFF = 0.5f; // Incremento de la amplitud del ruido en EGT por encima de los umbrales (modelo simple)
const float EGT_NOICE_MAX_AMPLITUDE = 30.0f; // Amplitud máxima del ruido en EGT (modelo simple)
const float EGT_NOICE_BASE_FREQUENCY = 0.5f; // Frecuencia base del ruido en EGT (modelo simple)
const float EGT_NOICE_MAX_FREQUENCY = 2.0f; // Frecuencia máxima del ruido en EGT (modelo simple)
const float EGT_NOICE_FREQUENCY_COEFF = 0.05f; // Incremento de la frecuencia del ruido en EGT por encima de los umbrales (modelo simple)
const float EGT_NOICE_RPM_EFFECT_COEFF = 0.02f; // Incremento de la amplitud del ruido en EGT por RPM alto (modelo simple)
const float EGT_NOICE_MIXTURE_EFFECT_COEFF = 20.0f; // Incremento de la amplitud del ruido en EGT por mezcla lean (modelo simple)
const float EGT_NOICE_IASEFFECT_COEFF = 0.3f; // Incremento de la amplitud del ruido en EGT por IAS alta (modelo simple)
const float EGT_NOICE_TEMP_EFFECT_COEFF = 0.5f; // Incremento de la amplitud del ruido en EGT por temperatura ambiente alta (modelo simple)
bool useSimulatedEGT = true; // true: calcula EGT en el ESP32, false: usa egtValue recibido por websocket
unsigned long lastEGTMillis = 0;






void resetEngineRelatedInstruments() {
  RPMStarted = false;
  startRoutine = false;
  guardoRPMBase = false;
  routineStep = 0;
  routineInitial = 0.0f;
  varRPM = 0.0f;
  RPMValue = 0.0f;
  RPMBase = 0.0f;
  RPMNoise = false;

  throttleValue = 0.0f;
  throttleControlEnabled = true;

  manifoldValue = THROTTLE_MAP_MIN;
  fuelFlowValue = 0.0f;
  oilPressValue = 0.0f;
  oilTempValue = 0.0f;
  chtValue = 0.0f;
  egtValue = EGT_MIN_F;
}


// ===== FUNCIONES WEBSOCKET =====
void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  
  if (type == WStype_TEXT) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
      // Si no es JSON válido, ignorar
      return;
    }
    
    if (doc["tc-rollValue"].is<float>() || doc["tc-rollValue"].is<int>()) {
      if (!brakeOn) {
        rollValue = doc["tc-rollValue"].as<float>(); // Comparte rollValue con Attitude Coordinator
      }
    }
    
    if (doc["tc-pitchValue"].is<float>() || doc["tc-pitchValue"].is<int>()) {
      if (!brakeOn) {
        turnCoordPitch = doc["tc-pitchValue"].as<float>();
        turnCoordPitch = clampf(turnCoordPitch, -30.0f, 30.0f); // Limitar rudder a ±30
      }
    }
    
    if (doc["useSimulatedGyro"].is<bool>()) {
      useSimulatedGyro = doc["useSimulatedGyro"].as<bool>();
    } else if (doc["useSimulatedGyro"].is<int>()) {
      useSimulatedGyro = (doc["useSimulatedGyro"].as<int>() != 0);
    }

    if (!useSimulatedGyro && (doc["gyroValue"].is<float>() || doc["gyroValue"].is<int>())) {
      gyroValue = doc["gyroValue"].as<float>();
    }

    if (doc["brakeOn"].is<bool>()) {
      brakeOn = doc["brakeOn"].as<bool>();
    } else if (doc["brakeOn"].is<int>()) {
      brakeOn = (doc["brakeOn"].as<int>() != 0);
    }
    
    if (doc["usePitchDrivenVerticalSpeed"].is<bool>()) {
      usePitchDrivenVerticalSpeed = doc["usePitchDrivenVerticalSpeed"].as<bool>();
    } else if (doc["usePitchDrivenVerticalSpeed"].is<int>()) {
      usePitchDrivenVerticalSpeed = (doc["usePitchDrivenVerticalSpeed"].as<int>() != 0);
    }

    if (!usePitchDrivenVerticalSpeed && (doc["verticalSpeed"].is<float>() || doc["verticalSpeed"].is<int>())) {
      verSpeedValue = doc["verticalSpeed"].as<float>();
    }

    if (doc["useSimulatedRPM"].is<bool>()) {
      useSimulatedRPM = doc["useSimulatedRPM"].as<bool>();
    } else if (doc["useSimulatedRPM"].is<int>()) {
      useSimulatedRPM = (doc["useSimulatedRPM"].as<int>() != 0);
    }
    if (!useSimulatedRPM && (doc["rpmSlider"].is<float>() || doc["rpmSlider"].is<int>())) {
      RPMValue = doc["rpmSlider"].as<float>();
    }
    if (doc["throttleValue"].is<float>() || doc["throttleValue"].is<int>()) {
      throttleValue = clampf(doc["throttleValue"].as<float>(), 0.0f, 100.0f);
      throttleControlEnabled = true;
    }
    if (doc["noiceBtnRPM"].is<bool>()) {
      RPMNoise = doc["noiceBtnRPM"].as<bool>();
    } else if (doc["noiceBtnRPM"].is<int>()) {
      RPMNoise = (doc["noiceBtnRPM"].as<int>() != 0);
    }
    if (!useSimulatedRPM && (doc["startBtnRPM"].is<bool>() || doc["startBtnRPM"].is<int>())) {
      bool startCommand = doc["startBtnRPM"].is<bool>()
        ? doc["startBtnRPM"].as<bool>()
        : (doc["startBtnRPM"].as<int>() != 0);

      if (startCommand) {
        RPMStarted = true;
      }

      if (RPMValue == 0.0f && startCommand) {
        // Serial.println("Iniciando rutina de arranque RPM...");
        RPMStarted = true;
        startRoutine = true;
        routineStart = millis();
        routineStep = 0;
        routineInitial = RPMValue;
      } else if (!startCommand) {
        // Serial.println("Deteniendo motor y reiniciando instrumentos relacionados...");
        resetEngineRelatedInstruments();
      }
    }
    if (doc["pitchValue"].is<float>() || doc["pitchValue"].is<int>()) {
      if (!brakeOn) {
        pitchValue = doc["pitchValue"].as<float>();
      }
    }
    if (doc["rollValue"].is<float>() || doc["rollValue"].is<int>()) {
      if (!brakeOn) {
        rollValue = doc["rollValue"].as<float>();
      }
    }
    if (doc["useSimulatedAirspeed"].is<bool>()) {
      useSimulatedAirspeed = doc["useSimulatedAirspeed"].as<bool>();
    } else if (doc["useSimulatedAirspeed"].is<int>()) {
      useSimulatedAirspeed = (doc["useSimulatedAirspeed"].as<int>() != 0);
    }

    if (!useSimulatedAirspeed && (doc["airspeedValue"].is<float>() || doc["airspeedValue"].is<int>())) {
      if (!brakeOn) {
        airspeedValue = doc["airspeedValue"].as<float>();
      }
    }
    if (doc["fuelFlowValue"].is<float>() || doc["fuelFlowValue"].is<int>()) {
      fuelFlowValue = doc["fuelFlowValue"].as<float>();
    }
    if (doc["useSimulatedManifold"].is<bool>()) {
      useSimulatedManifold = doc["useSimulatedManifold"].as<bool>();
    } else if (doc["useSimulatedManifold"].is<int>()) {
      useSimulatedManifold = (doc["useSimulatedManifold"].as<int>() != 0);
    }
    if (!useSimulatedManifold && (doc["manifold"].is<float>() || doc["manifold"].is<int>())) {
      manifoldValue = doc["manifold"].as<float>();
    }

    if (doc["oilPress"].is<float>() || doc["oilPress"].is<int>()) {
      oilPressValue = doc["oilPress"].as<float>();
    }

    if (doc["oilTemp"].is<float>() || doc["oilTemp"].is<int>()) {
      oilTempValue = doc["oilTemp"].as<float>();
    }
    if (doc["useSimulatedCHT"].is<bool>()) {
      bool newMode = doc["useSimulatedCHT"].as<bool>();
      if (newMode != useSimulatedCHT) {
        useSimulatedCHT = newMode;
        lastCHTMillis = millis();
      }
    } else if (doc["useSimulatedCHT"].is<int>()) {
      bool newMode = (doc["useSimulatedCHT"].as<int>() != 0);
      if (newMode != useSimulatedCHT) {
        useSimulatedCHT = newMode;
        lastCHTMillis = millis();
      }
    }
    if (doc["mixtureValue"].is<float>() || doc["mixtureValue"].is<int>()) {
      chtMixture = clampf(doc["mixtureValue"].as<float>(), 0.0f, 1.0f);
    } else if (doc["mixture"].is<float>() || doc["mixture"].is<int>()) {
      chtMixture = clampf(doc["mixture"].as<float>(), 0.0f, 1.0f);
    }
    if (!useSimulatedCHT && (doc["chtValue"].is<float>() || doc["chtValue"].is<int>())) {
      chtValue = doc["chtValue"].as<float>();
    }
    if (doc["useSimulatedOilTemp"].is<bool>()) {
      bool newMode = doc["useSimulatedOilTemp"].as<bool>();
      if (newMode != useSimulatedOilTemp) {
        useSimulatedOilTemp = newMode;
        lastOilTempMillis = millis();
      }
    } else if (doc["useSimulatedOilTemp"].is<int>()) {
      bool newMode = (doc["useSimulatedOilTemp"].as<int>() != 0);
      if (newMode != useSimulatedOilTemp) {
        useSimulatedOilTemp = newMode;
        lastOilTempMillis = millis();
      }
    }
    if (!useSimulatedOilTemp && (doc["oilTemp"].is<float>() || doc["oilTemp"].is<int>())) {
      oilTempValue = doc["oilTemp"].as<float>();
    }
    if (doc["useSimulatedOilPress"].is<bool>()) {
      bool newMode = doc["useSimulatedOilPress"].as<bool>();
      if (newMode != useSimulatedOilPress) {
        useSimulatedOilPress = newMode;
        lastOilPressMillis = millis();
      }
    } else if (doc["useSimulatedOilPress"].is<int>()) {
      bool newMode = (doc["useSimulatedOilPress"].as<int>() != 0);
      if (newMode != useSimulatedOilPress) {
        useSimulatedOilPress = newMode;
        lastOilPressMillis = millis();
      }
    }
    if (!useSimulatedOilPress && (doc["oilPress"].is<float>() || doc["oilPress"].is<int>())) {
      oilPressValue = doc["oilPress"].as<float>();
    }
    if (doc["useSimulatedFuelFlow"].is<bool>()) {
      bool newMode = doc["useSimulatedFuelFlow"].as<bool>();
      if (newMode != useSimulatedFuelFlow) {
        useSimulatedFuelFlow = newMode;
        lastFuelFlowMillis = millis();
      }
    } else if (doc["useSimulatedFuelFlow"].is<int>()) {
      bool newMode = (doc["useSimulatedFuelFlow"].as<int>() != 0);
      if (newMode != useSimulatedFuelFlow) {
        useSimulatedFuelFlow = newMode;
        lastFuelFlowMillis = millis();
      }
    }
    if (!useSimulatedFuelFlow && (doc["fuelFlowValue"].is<float>() || doc["fuelFlowValue"].is<int>())) {
      fuelFlowValue = doc["fuelFlowValue"].as<float>();
    }

    // Manejo de modo simulado/manual para Fuel
    if (doc["useSimulatedFuel"].is<bool>()) {
      bool newMode = doc["useSimulatedFuel"].as<bool>();
      if (newMode != useSimulatedFuel) {
        useSimulatedFuel = newMode;
        lastFuelMillis = millis();
      }
    } else if (doc["useSimulatedFuel"].is<int>()) {
      bool newMode = (doc["useSimulatedFuel"].as<int>() != 0);
      if (newMode != useSimulatedFuel) {
        useSimulatedFuel = newMode;
        lastFuelMillis = millis();
      }
    }
    if (!useSimulatedFuel && (doc["fuelValueLeft"].is<float>() || doc["fuelValueLeft"].is<int>())) {
      fuelValueLeft = doc["fuelValueLeft"].as<float>();
    }
    if (!useSimulatedFuel && (doc["fuelValueRight"].is<float>() || doc["fuelValueRight"].is<int>())) {
      fuelValueRight = doc["fuelValueRight"].as<float>();
    }

    // Manejo de modo simulado/manual para Voltaje
    if (doc["useSimulatedVoltage"].is<bool>()) {
      bool newMode = doc["useSimulatedVoltage"].as<bool>();
      if (newMode != useSimulatedVoltage) {
        useSimulatedVoltage = newMode;
        lastVoltageMillis = millis();
      }
    } else if (doc["useSimulatedVoltage"].is<int>()) {
      bool newMode = (doc["useSimulatedVoltage"].as<int>() != 0);
      if (newMode != useSimulatedVoltage) {
        useSimulatedVoltage = newMode;
        lastVoltageMillis = millis();
      }
    }
    if (!useSimulatedVoltage && (doc["voltAmpValueLeft"].is<float>() || doc["voltAmpValueLeft"].is<int>())) {
      voltAmpValueLeft = doc["voltAmpValueLeft"].as<float>();
    }

    if (doc["voltAmpValueRight"].is<float>() || doc["voltAmpValueRight"].is<int>()) {
      voltAmpValueRight = doc["voltAmpValueRight"].as<float>();
    }
    if (!useSimulatedEGT && (doc["egtValue"].is<float>() || doc["egtValue"].is<int>())) {
      egtValue = clampf(doc["egtValue"].as<float>(), EGT_MIN_F, EGT_MAX_F);
    }

    if (doc["egtBugValue"].is<float>() || doc["egtBugValue"].is<int>()) {
      egtBugValue = clampf(doc["egtBugValue"].as<float>(), EGT_MIN_F, EGT_MAX_F);
    }

    if (doc["useSimulatedEGT"].is<bool>()) {  
      // Cambiar el modo de EGT y resetear el timer para que no haya un salto brusco en la simulación
      bool newMode = doc["useSimulatedEGT"].as<bool>();
      if (newMode != useSimulatedEGT) {
        useSimulatedEGT = newMode;
        lastEGTMillis = 0;
      }
    } else if (doc["useSimulatedEGT"].is<int>()) {
      bool newMode = (doc["useSimulatedEGT"].as<int>() != 0);
      if (newMode != useSimulatedEGT) {
        useSimulatedEGT = newMode;
        lastEGTMillis = 0;
      }
    }


  }
}


// ===== CONFIGURAR ACCESS POINT =====
const char* ap_ssid = "Intrumentos-ESP32";  // Nombre del AP servidor
const char* ap_pass = "12345678";   // mínimo 8 caracteres

// Puerto DNS estándar
const byte DNS_PORT = 53;

// IP fija del AP (típica 192.168.4.1)
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);



// ===== HANDLERS DEL SERVIDOR HTTP =====

// Página principal
void handleRoot() {
  String page = MAIN_page;
  page.replace("Cargando...", WiFi.localIP().toString());
  server.send(200, "text/html", page);
}


// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  WiFiManager wifiManager;
  // wifiManager.resetSettings(); // Descomenta para forzar portal cada vez
  // Serial.println("Iniciando WiFiManager...");
  if (!wifiManager.autoConnect("Instrumentos-ESP32")) {
    // Serial.println("No se pudo conectar a WiFi. Reiniciando...");
    delay(3000);
    ESP.restart();
  }
  // Serial.println("Conectado a WiFi!");
  // Serial.print("IP asignada: ");
  // Serial.println(WiFi.localIP());
  
  // Enviar email con la IP (esperar un poco para que NTP sincronice)
  delay(2000);
  enviarEmailConIP(WiFi.localIP().toString());
  
  // ===== HTTP SERVER =====
  
  server.on("/", handleRoot);
  // server.onNotFound(handleNotFound); // Comentado porque handleNotFound está comentada
  server.begin();
  
  // ===== WEBSOCKET =====
  ws.begin();
  ws.onEvent(onWsEvent);
  
  // Inicializar contador de horas de funcionamiento
  horasStartMillis = millis();

  // Configurar NTP para obtener hora de internet
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // Serial.println("Sincronizando hora con NTP...");

}


static unsigned long lastUpdate = 0; // usada en variometroAltimetro
float cuentaTiempo = 0.0f;

float clampf(float value, float minValue, float maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

float mixtureEffect(float phi) {
  float phiClamped = clampf(phi, 0.0f, 1.0f);
  float d = phiClamped - 0.5f;
  return 1.0f - 4.0f * (d * d);
}

void manejoFuelFlow() {
  if (!useSimulatedFuelFlow) return;

  unsigned long now = millis();
  if (lastFuelFlowMillis == 0) {
    lastFuelFlowMillis = now;
    return;
  }

  if (RPMValue <= 0.0f) {
    fuelFlowValue = 0.0f;
    lastFuelFlowMillis = now;
    return;
  }

  float dt = (now - lastFuelFlowMillis) / 1000.0f;
  lastFuelFlowMillis = now;

  float dtClamped = clampf(dt, 0.0f, 0.2f);
  float rpmNorm = clampf(RPMValue / 2700.0f, 0.0f, 1.0f);
  float mapNorm = clampf((manifoldValue - 10.0f) / 19.0f, 0.0f, 1.0f);
  float tempEffect = clampf((oilTempValue - 100.0f) / 100.0f, 0.0f, 2.0f);
  float iasFactor = clampf((220.0f - airspeedValue) / 220.0f, 0.0f, 1.0f);
  
  float fuelFlowTarget =
    FF_C0 +
    (FF_C1 * RPMValue) +
    (FF_C2 * mapNorm) +
    (FF_TEMP_COEFF * tempEffect) +
    (FF_IAS_COEFF * airspeedValue);

  fuelFlowValue += (fuelFlowTarget - fuelFlowValue) * FF_ALPHA * dtClamped;
  fuelFlowValue = clampf(fuelFlowValue, FF_MIN, FF_MAX);
}

void manejoFuelConsumption() {
  if (!useSimulatedFuel) return;

  unsigned long now = millis();
  
  // Inicializar timers en el primer ciclo
  if (lastFuelMillis == 0) {
    lastFuelMillis = now;
    lastTankSwitchMillis = now;
    return;
  }

  // Calcular delta de tiempo
  float dt = (now - lastFuelMillis) / 1000.0f; // Convertir a segundos
  lastFuelMillis = now;

  // Si el motor está encendido (RPM > 0) y hay fuelFlow
  if (RPMValue > 0.0f && fuelFlowValue > 0.0f) {
    // Consumir combustible del tanque activo
    // fuelFlowValue está en GPH (galones por hora)
    // dt está en segundos, entonces consumo = (GPH / 3600) * dt
    float fuelConsumed = (fuelFlowValue / 3600.0f) * dt;
    
    if (activeTank == 1) {
      // Consumir del tanque izquierdo
      fuelValueLeft -= fuelConsumed;
      if (fuelValueLeft < 0.0f) fuelValueLeft = 0.0f;
    } else {
      // Consumir del tanque derecho
      fuelValueRight -= fuelConsumed;
      if (fuelValueRight < 0.0f) fuelValueRight = 0.0f;
    }
  }

  // Cambiar de tanque cada 10 minutos
  if (now - lastTankSwitchMillis >= TANK_SWITCH_INTERVAL) {
    // Alternar tanque
    if (activeTank == 1) {
      activeTank = 2; // Cambiar a tanque derecho
    } else {
      activeTank = 1; // Cambiar a tanque izquierdo
    }
    lastTankSwitchMillis = now;
  }

  // Verificar si el tanque activo está vacío
  bool tankEmpty = false;
  if (activeTank == 1 && fuelValueLeft <= 0.0f) {
    tankEmpty = true;
  } else if (activeTank == 2 && fuelValueRight <= 0.0f) {
    tankEmpty = true;
  }

  // Si el tanque activo está vacío, intentar cambiar al otro tanque
  if (tankEmpty) {
    if (activeTank == 1 && fuelValueRight > 0.0f) {
      activeTank = 2; // Cambiar a tanque derecho
      lastTankSwitchMillis = now;
      tankEmpty = false;
    } else if (activeTank == 2 && fuelValueLeft > 0.0f) {
      activeTank = 1; // Cambiar a tanque izquierdo
      lastTankSwitchMillis = now;
      tankEmpty = false;
    }
  }

  // Si ambos tanques están vacíos, apagar el motor
  if (fuelValueLeft <= 0.0f && fuelValueRight <= 0.0f) {
    // Apagar motor gradualmente
    RPMValue *= 0.95f; // Reducir RPM progresivamente
    if (RPMValue < 50.0f) {
      RPMValue = 0.0f;
    }
  }
}

void manejoVoltage() {
  if (!useSimulatedVoltage) return;

  unsigned long now = millis();
  if (lastVoltageMillis == 0) {
    lastVoltageMillis = now;
    voltAmpValueLeft = VOLT_BASE; // Inicializar con voltaje de batería
    return;
  }

  float dt = (now - lastVoltageMillis) / 1000.0f;
  lastVoltageMillis = now;

  float dtClamped = clampf(dt, 0.0f, 0.2f);
  
  // Calcular voltaje objetivo basado en RPM y carga
  float rpmNorm = clampf(RPMValue / 2700.0f, 0.0f, 1.0f);
  
  // Carga eléctrica (amperaje absoluto) reduce voltaje
  float ampLoad = fabsf(voltAmpValueRight); // Valor absoluto del amperaje
  
  // Voltaje = Base + (incremento por alternador) - (caída por carga)
  float voltageTarget = VOLT_BASE + (VOLT_RPM_COEFF * rpmNorm) - (VOLT_AMP_COEFF * ampLoad);
  
  // Aplicar integración exponencial
  voltAmpValueLeft += (voltageTarget - voltAmpValueLeft) * VOLT_ALPHA * dtClamped;
  
  // Limitar al rango del instrumento
  voltAmpValueLeft = clampf(voltAmpValueLeft, VOLT_MIN, VOLT_MAX);
}

void manejoOilPress() {
  if (!useSimulatedOilPress) return;

  unsigned long now = millis();
  if (lastOilPressMillis == 0) {
    lastOilPressMillis = now;
    return;
  }

  if (RPMValue <= 0.0f) {
    oilPressValue = 0.0f;
    lastOilPressMillis = now;
    return;
  }

  float dt = (now - lastOilPressMillis) / 1000.0f;
  lastOilPressMillis = now;

  float dtClamped = clampf(dt, 0.0f, 0.2f);
  float rpmNorm = clampf(RPMValue / 2700.0f, 0.0f, 1.0f);
  float tempEffect = clampf((oilTempValue - 100.0f) / 150.0f, -1.0f, 1.0f);
  
  float oilPressTarget =
    OIL_PRESS_C0 +
    (OIL_PRESS_C1 * RPMValue) +
    (OIL_PRESS_TEMP_COEFF * tempEffect);

  oilPressValue += (oilPressTarget - oilPressValue) * OIL_PRESS_ALPHA * dtClamped;
  oilPressValue = clampf(oilPressValue, OIL_PRESS_MIN, OIL_PRESS_MAX);
}

void manejoOilTemp() {
  if (!useSimulatedOilTemp) return;

  unsigned long now = millis();
  if (lastOilTempMillis == 0) {
    lastOilTempMillis = now;
    return;
  }

  if (RPMValue <= 0.0f && oilPressValue <= 0.0f && manifoldValue <= 10.0f) {
    oilTempValue = 0.0f;
    lastOilTempMillis = now;
    return;
  }

  float dt = (now - lastOilTempMillis) / 1000.0f;
  lastOilTempMillis = now;

  float dtClamped = clampf(dt, 0.0f, 0.2f);
  float rpmNorm = clampf(RPMValue / 2700.0f, 0.0f, 1.0f);
  float mapNorm = clampf((manifoldValue - 10.0f) / 19.0f, 0.0f, 1.0f);
  float presNorm = clampf(oilPressValue / 100.0f, 0.0f, 1.0f);
  float pctPower = 0.50f * rpmNorm + 0.35f * mapNorm + 0.15f * presNorm;
  float iasKt = clampf(airspeedValue, 0.0f, 220.0f);

  float oilTempTarget =
    OIL_C0_F +
    (OIL_C1_F * pctPower) +
    (OIL_C2_F * presNorm) -
    (OIL_C3_F_PER_KT * iasKt);

  oilTempValue += (oilTempTarget - oilTempValue) * OIL_ALPHA * dtClamped;
  oilTempValue = clampf(oilTempValue, OIL_MIN_F, OIL_MAX_F);
}

void manejoCHT() {
  if (!useSimulatedCHT) return;

  unsigned long now = millis();
  if (lastCHTMillis == 0) {
    lastCHTMillis = now;
    return;
  }

  if (RPMValue <= 0.0f && fuelFlowValue <= 0.0f && manifoldValue <= 10.0f) {
    chtValue = 0.0f;
    lastCHTMillis = now;
    return;
  }

  float dt = (now - lastCHTMillis) / 1000.0f;
  lastCHTMillis = now;

  float dtClamped = clampf(dt, 0.0f, 0.2f);
  float rpmNorm = clampf(RPMValue / 2700.0f, 0.0f, 1.0f);
  float mapNorm = clampf((manifoldValue - 10.0f) / 19.0f, 0.0f, 1.0f);
  float ffNorm = clampf(fuelFlowValue / 16.0f, 0.0f, 1.0f);
  float pctPower = 0.50f * rpmNorm + 0.35f * mapNorm + 0.15f * ffNorm;
  float iasKt = clampf(airspeedValue, 0.0f, 220.0f);
  float mixture = 1.0f - ffNorm;
  float g = mixtureEffect(mixture);

  float chtTarget =
    CHT_C0_F +
    (CHT_C1_F * pctPower) +
    (CHT_C2_F * g) -
    (CHT_C3_F_PER_KT * iasKt);

  chtValue += (chtTarget - chtValue) * CHT_ALPHA * dtClamped;
  chtValue = clampf(chtValue, CHT_MIN_F, CHT_MAX_F);
}

// ===== MANEJO DE GYRO =====
// Calcula el heading basado en el roll o el rudder durante el rodaje
// Si roll = ±30°, el avión da vuelta completa (360°) en 2 minutos = 3°/segundo
// Para un buen giro: ángulo_óptimo = airspeed/10 + 7, que también produce 3°/segundo
void manejoGyro() {
  unsigned long now = millis();

  if (!useSimulatedGyro) {
    lastGyroMillis = now;
    return;
  }
  
  // En la primera llamada
  if (lastGyroMillis == 0) {
    lastGyroMillis = now;
    return;
  }

  // Si frenos activos, no actualizar heading
  if (brakeOn) {
    lastGyroMillis = now;
    return;
  }
  
  // Calcular delta time en segundos
  float dt = (now - lastGyroMillis) / 1000.0f;
  lastGyroMillis = now;
  
  // Limitar dt a máximo 0.2 segundos para evitar saltos
  dt = clampf(dt, 0.0f, 0.2f);
  
  // Calcular ángulo óptimo para buen giro: airspeed/10 + 7
  float goodTurnAngle = airspeedValue / 10.0f + 7.0f;
  
  // Calcular tasa de giro en °/segundo
  // Si roll = ±30°, tasa = ±3°/seg (360° en 120 segundos)
  // Si roll = 0°, tasa = 0°/seg
  // Interpolación lineal: tasa = (roll / 30) * 3
  float steeringInput = rollValue;
  if (RPMValue >= TAXI_RPM_THRESHOLD && airspeedValue <= TAXI_IAS_MAX) {
    steeringInput = turnCoordPitch; // Rudder para orientar durante rodaje
  }
  float turnRate = (steeringInput / 30.0f) * 3.0f;  // en grados por segundo
  
  // Actualizar heading sumando la rotación del giro
  float headingDelta = turnRate * dt;  // Change en grados
  gyroValue += headingDelta;
  
  // Mantener heading en rango 0-360°
  while (gyroValue < 0.0f) {
    gyroValue += 360.0f;
  }
  while (gyroValue >= 360.0f) {
    gyroValue -= 360.0f;
  }
}

// ===== MANEJO DEL SLIP/SKID BALL =====
// Calcula la posición del péndulo (ball) basado en aceleración lateral real
// Fórmula física: a_lateral = TAS * yawRate - g * tan(bank)
void updateSlipSkidBall() {
  unsigned long now = millis();
  
  // En la primera llamada
  if (lastTCBallMillis == 0) {
    lastTCBallMillis = now;
    tcBallNormalized = 0.0f;
    tcBallValue = 0.0f;
    tcYawRateStateRadSec = 0.0f;
    tcPrevBankRad = rollValue * (M_PI / 180.0f);
    tcRollRateStateRadSec = 0.0f;
    return;
  }
  
  // Si frenos activos, ball centrada
  if (brakeOn) {
    tcBallNormalized = 0.0f;
    tcBallValue = 0.0f;
    tcYawRateStateRadSec = 0.0f;
    tcPrevBankRad = rollValue * (M_PI / 180.0f);
    tcRollRateStateRadSec = 0.0f;
    lastTCBallMillis = now;
    return;
  }
  
  // Calcular delta time en segundos
  float dt = (now - lastTCBallMillis) / 1000.0f;
  lastTCBallMillis = now;
  dt = clampf(dt, 0.0f, 0.2f);
  
  // Convertir airspeed de kts a m/s (1 kt = 0.51444 m/s)
  float tasMs = airspeedValue * 0.51444f;
  
  // Proteger contra velocidad muy baja (evitar NaN y respuestas extremas)
  if (tasMs < 0.5f) tasMs = 0.5f;
  
  // Convertir roll de grados a radianes
  float bankRad = rollValue * (M_PI / 180.0f);

  // Componente inercial por tasa de alabeo (dinámica transitoria de la bola)
  float rawRollRate = (bankRad - tcPrevBankRad) / fmaxf(dt, 0.01f); // rad/s
  tcPrevBankRad = bankRad;
  rawRollRate = clampf(rawRollRate, -1.5f, 1.5f);

  float rollRateTau = 0.18f;
  float rollRateAlpha = clampf(dt / rollRateTau, 0.0f, 1.0f);
  tcRollRateStateRadSec += (rawRollRate - tcRollRateStateRadSec) * rollRateAlpha;
  
  // Modelo más realista:
  // 1) Yaw coordinado esperado por bank + airspeed
  // 2) Aporte del rudder
  // 3) Inercia en la respuesta de yaw
  float coordYawRateIdeal = (GRAVITY * tan(bankRad)) / tasMs; // rad/s ideal en giro coordinado

  // A baja IAS, el avión no sostiene coordinación perfecta: aparece deslizamiento hacia el centro del giro.
  // 40 kt -> ~0.05 (muy poca coordinación), 80 kt o más -> 1.00 (coordinación completa)
  float coordinationEfficiency = 0.05f + 0.95f * clampf((airspeedValue - 40.0f) / 40.0f, 0.0f, 1.0f);
  float coordYawRate = coordYawRateIdeal * coordinationEfficiency;

  float baseRudderYawRate = 3.0f * (M_PI / 180.0f); // ±3°/s base por pedal
  float rudderAuthority = baseRudderYawRate + 0.40f * fabsf(coordYawRate);
  float rudderYawRate = -(turnCoordPitch / 30.0f) * rudderAuthority;

  float targetYawRateRadSec = coordYawRate + rudderYawRate;

  // A mayor TAS, más "inercia" direccional (respuesta menos instantánea)
  float tauYaw = 0.20f + 0.80f * clampf(tasMs / 60.0f, 0.0f, 1.0f); // s
  float yawAlpha = clampf(dt / tauYaw, 0.0f, 1.0f);
  tcYawRateStateRadSec += (targetYawRateRadSec - tcYawRateStateRadSec) * yawAlpha;
  
  // Calcular aceleración lateral real (física)
  // a_lateral = TAS * yawRate - g * tan(bank)
  float a_roll_inertia = -0.25f * tasMs * tcRollRateStateRadSec;
  float a_lateral = tasMs * tcYawRateStateRadSec - GRAVITY * tan(bankRad) + a_roll_inertia;
  
  // Normalizar a [-1..1]
  float target = TC_BALL_SENSITIVITY * (a_lateral / GRAVITY);
  
  // Limitar al rango [-1..1]
  target = clampf(target, -1.0f, 1.0f);
  
  // Aplicar amortiguación exponencial al valor normalizado
  float alpha = TC_BALL_DAMPING * dt;
  alpha = clampf(alpha, 0.0f, 1.0f);
  
  tcBallNormalized += (target - tcBallNormalized) * alpha;
  
  // Convertir a rango visual [-30..30] para el instrumento
  tcBallValue = tcBallNormalized * 30.0f;
  tcBallValue = clampf(tcBallValue, -30.0f, 30.0f);
}


// ===== LOOP PRINCIPAL ======
void loop() {
  // Gestionar frenos: sin velocidad con frenos ON
  if (brakeOn != lastBrakeOnState) {
    if (brakeOn) {
      storedAirspeedValue = airspeedValue;
      airspeedValue = 0.0f;
    } else {
      airspeedValue = storedAirspeedValue;
    }
    lastBrakeOnState = brakeOn;
  }

  // Con 25-35% de potencia, airspeed debe estar por debajo de 40 kts
  float rpmPercent = clampf(RPMValue / RPM_MAX_FIXED_PITCH, 0.0f, 1.0f);
  if (!brakeOn && rpmPercent >= 0.25f && rpmPercent <= 0.35f) {
    if (airspeedValue > AIRSPEED_TAKEOFF_MAX) {
      airspeedValue = AIRSPEED_TAKEOFF_MAX;
    }
  }

  // Modelo de AirSpeed:
  // - Motor encendido: objetivo por potencia (RPM)
  // - Motor apagado y en vuelo: objetivo de planeo (pitch + descenso)
  if (!brakeOn) {
    float targetIas = 0.0f;

    if (RPMStarted && RPMValue > 50.0f) {
      if (rpmPercent >= 0.75f) {
        // 75%-100%: 40 -> 120 kts
        float ramp = (rpmPercent - 0.75f) / 0.25f; // 0..1
        targetIas = AIRSPEED_FLIGHT_MIN + (AIRSPEED_FLIGHT_MAX - AIRSPEED_FLIGHT_MIN) * ramp;
      } else {
        // 0%-75%: 0 -> 40 kts
        targetIas = AIRSPEED_TAKEOFF_MAX * (rpmPercent / 0.75f);
        targetIas = clampf(targetIas, 0.0f, AIRSPEED_TAKEOFF_MAX);
      }
    } else {
      bool likelyFlying = (altitudValue > 100.0f) || (airspeedValue > 45.0f) || (fabsf(verSpeedValue) > 150.0f);

      if (likelyFlying) {
        float descentFpm = clampf(-verSpeedValue, 0.0f, 1800.0f);
        float descentFactor = descentFpm / 1800.0f; // 0..1

        float noseDownFactor = clampf((-pitchValue + 2.0f) / 14.0f, 0.0f, 1.0f); // morro abajo => más IAS
        float glideMix = 0.60f * noseDownFactor + 0.40f * descentFactor;

        targetIas = AIRSPEED_GLIDE_MIN + (AIRSPEED_GLIDE_MAX - AIRSPEED_GLIDE_MIN) * glideMix;
      } else {
        targetIas = 0.0f;
      }
    }

    airspeedValue += (targetIas - airspeedValue) * AIRSPEED_RAMP_ALPHA;
  }

  // Actualizar slip/skid ball con física real (aceleración lateral)
  updateSlipSkidBall();

  // Attitude Control -> Variómetro (ascenso/descenso automático por pitch)
  // Calibración: +9° de pitch a potencia/velocidad de ascenso ≈ +700 fpm
  if (usePitchDrivenVerticalSpeed) {
    if (brakeOn) {
      verSpeedValue = 0.0f;
    } else {
      float climbBaseFpmPerDeg = 78.0f; // 9° -> ~702 fpm en condiciones ideales
      float airspeedFactor = clampf(airspeedValue / 74.0f, 0.0f, 1.2f);
      float powerFactor = clampf((rpmPercent - 0.20f) / 0.80f, 0.0f, 1.0f);

      float responseFactor = 1.0f;
      if (pitchValue >= 0.0f) {
        // Para ascenso, exigir potencia/velocidad para sostener tasa alta
        responseFactor = airspeedFactor * (0.35f + 0.65f * powerFactor);
      } else {
        // Para descenso, permitir mayor respuesta incluso con baja potencia
        responseFactor = 0.60f + 0.40f * airspeedFactor;
      }

      float targetVerticalSpeed = pitchValue * climbBaseFpmPerDeg * responseFactor;

      // Por debajo de 55 kts no se puede sostener vuelo nivelado: aparece descenso base.
      // 55 kts -> 0 fpm adicional, 40 kts o menos -> ~380 fpm de descenso base.
      float lowSpeedDeficit = clampf((55.0f - airspeedValue) / 15.0f, 0.0f, 1.0f);
      float lowSpeedSinkFpm = 380.0f * lowSpeedDeficit;
      targetVerticalSpeed -= lowSpeedSinkFpm;

      // Penalización por vuelo no coordinado (slip/skid): más bola desviada => más descenso.
      // Esto hace que el variómetro refleje pérdidas de altitud por descoordinación.
      float ballNorm = clampf(fabsf(tcBallValue) / 30.0f, 0.0f, 1.0f);
      float ballDeadzone = 0.12f; // evitar sensibilidad excesiva cerca del centro
      float uncoord = clampf((ballNorm - ballDeadzone) / (1.0f - ballDeadzone), 0.0f, 1.0f);
      float slipSinkFpm = 250.0f * uncoord; // hasta ~250 fpm de descenso extra con bola al tope
      targetVerticalSpeed -= slipSinkFpm;

      targetVerticalSpeed = clampf(targetVerticalSpeed, -1500.0f, 1500.0f);

      // Suavizado para evitar saltos bruscos de aguja
      verSpeedValue += (targetVerticalSpeed - verSpeedValue) * 0.08f;
    }
  }

  // Aplicar throttle como mando maestro de potencia (RPM + manifold)
  if (throttleControlEnabled) {
    float throttleNorm = clampf(throttleValue / 100.0f, 0.0f, 1.0f);

    bool outOfFuel = useSimulatedFuel && fuelValueLeft <= 0.0f && fuelValueRight <= 0.0f;

    if (useSimulatedManifold) {
      float targetManifold = THROTTLE_MAP_MIN + (THROTTLE_MAP_MAX - THROTTLE_MAP_MIN) * throttleNorm;
      if (!RPMStarted || outOfFuel) {
        targetManifold = THROTTLE_MAP_MIN;
      }
      manifoldValue += (targetManifold - manifoldValue) * THROTTLE_ALPHA;
      manifoldValue = clampf(manifoldValue, THROTTLE_MAP_MIN, 50.0f);
    }

    if (useSimulatedRPM && !startRoutine) {
      float targetRPM = 0.0f;
      if (RPMStarted && !outOfFuel) {
        targetRPM = THROTTLE_IDLE_RPM + (THROTTLE_MAX_RPM - THROTTLE_IDLE_RPM) * throttleNorm;
      }
      RPMValue += (targetRPM - RPMValue) * THROTTLE_ALPHA;
      RPMValue = clampf(RPMValue, 0.0f, 3000.0f);
    }
  }

  // Manejo de variómetro y altímetro
  variometroAltimetro();
  // Manejo de RPM
  manejoRPM();  // Calcular horas de funcionamiento
  // Manejo de Fuel Flow simulado
  manejoFuelFlow();
  // Manejo de consumo de combustible
  manejoFuelConsumption();
  // Manejo de Voltaje simulado
  manejoVoltage();
  // Manejo de Oil Pressure simulado
  manejoOilPress();
  // Manejo de Oil Temperature simulado
  manejoOilTemp();
  // Manejo de CHT simulado
  manejoCHT();
  // Manejo de Gyro simulado
  manejoGyro();
  calcularHorasFuncionamiento();  
  // Manejo de Reloj
  manejoReloj();

  // Manejo de Turn Coordinator (coordinación + slip/skid ball)
  // manejoTurnCoordinator();

  // Manejo de frenos
  // (Se maneja al inicio del loop para cortar potencia y velocidad instantáneamente al
  // activar frenos, y restaurar velocidad al desactivarlos)
  // manejoFrenos();

  // Manejo de EGT simulado
  manejoEGT();

  // Procesar DNS (muy importante para el portal cautivo)
  dnsServer.processNextRequest();
  // HTTP server
  server.handleClient();
  // WebSocket
  ws.loop();

  
  // Enviar valores de los dos instrumentos activos
  StaticJsonDocument<1000> doc;
  doc["verticalSpeed"] = verSpeedValue;
  doc["usePitchDrivenVerticalSpeed"] = usePitchDrivenVerticalSpeed;
  doc["altitudValue"] = altitudValue;
  doc["atmosphericPressureHpa"] = atmosphericPressureHpa;
  doc["bandera_off"] = bandera_off;
  doc["RPMValue"] = RPMValue;
  doc["varRPM"] = varRPM;
  doc["RPMStarted"] = RPMStarted;
  doc["throttleValue"] = throttleValue;
  doc["RPMNoise"] = RPMNoise;
  doc["useSimulatedRPM"] = useSimulatedRPM;
  doc["horasFuncionamiento"] = horasFuncionamiento;
  doc["minutosFuncionamiento"] = minutosFuncionamiento;
  doc["pitchValue"] = pitchValue;
  doc["rollValue"] = rollValue;
  doc["airspeedValue"] = airspeedValue;
  doc["useSimulatedAirspeed"] = useSimulatedAirspeed;
  doc["gyroValue"] = gyroValue;
  doc["useSimulatedGyro"] = useSimulatedGyro;
  doc["brakeOn"] = brakeOn;
  doc["tc-pitchValue"] = turnCoordPitch;
  doc["tc-rollValue"] = rollValue; // Comparte el mismo valor con Attitude Coordinator
  doc["tcBallValue"] = tcBallValue; // Slip/Skid ball: coordinación roll-rudder
  doc["fuelFlowValue"] = fuelFlowValue;
  doc["useSimulatedFuelFlow"] = useSimulatedFuelFlow;
  doc["manifold"] = manifoldValue;
  doc["useSimulatedManifold"] = useSimulatedManifold;
  doc["oilPress"] = oilPressValue;
  doc["useSimulatedOilPress"] = useSimulatedOilPress;
  doc["oilTemp"] = oilTempValue;
  doc["useSimulatedOilTemp"] = useSimulatedOilTemp;
  doc["chtValue"] = chtValue;
  doc["useSimulatedCHT"] = useSimulatedCHT;
  doc["mixtureValue"] = chtMixture;
  doc["fuelValueLeft"] = fuelValueLeft;
  doc["fuelValueRight"] = fuelValueRight;
  doc["useSimulatedFuel"] = useSimulatedFuel;
  doc["activeTank"] = activeTank;
  doc["relojValue"] = String(relojHoras) + ":" + String(relojMinutos) + ":" + String(relojSegundos);
  doc["voltAmpValueLeft"] = voltAmpValueLeft;
  doc["useSimulatedVoltage"] = useSimulatedVoltage;
  doc["voltAmpValueRight"] = voltAmpValueRight;
  doc["egtValue"] = egtValue;
  doc["egtBugValue"] = egtBugValue;
  doc["useSimulatedEGT"] = useSimulatedEGT;

  String output;
  serializeJson(doc, output);
  ws.broadcastTXT(output);
  
  
  cuentaTiempo += 0.05f; // Aproximadamente cada 50 ms
  if (cuentaTiempo >= 5.0f) {
    cuentaTiempo = 0.0f; // Reiniciar contador
    // Cuenta 5 segundos y puede hacer algo.
    // Serial.println("Enviando datos: " + output);
    // Serial.println("==============================");
    // Serial.println("Estado actual:");
    // Serial.print("Velocidad vertical: ");Serial.print(verSpeedValue);Serial.println(" ft/min");
    // Serial.print("Altitud: ");Serial.print(altitudValue );Serial.println(" ft");
    // Serial.print("Bandera off: ");Serial.println(bandera_off ? "Sí" : "No");
    // Serial.print("varRPM: ");Serial.print(varRPM);Serial.println(" Solo ruido");  
    // Serial.print("RPMValue: ");Serial.print(RPMValue);Serial.println(" rpm del slider");  
    // Serial.print("Ruido en RPM: ");Serial.println(RPMNoise ? "Sí" : "No");
    // Serial.print("Motor encendido: ");Serial.println(RPMStarted ? "Sí" : "No");
    // Serial.print("Pitch: ");Serial.print(pitchValue);Serial.println(" °");
    // Serial.print("Roll: ");Serial.print(rollValue);Serial.println(" °");
    // Serial.print("Air Speed: ");Serial.print(airspeedValue);Serial.println(" kts  ");
    // Serial.print("Gyro: ");Serial.print(gyroValue);Serial.println(" °");
    // Serial.print("Turn Coordinator - Pitch: ");Serial.print(turnCoordPitch);Serial.println(" °");
    // Serial.print("Turn Coordinator - Roll: ");Serial.print(turnCoordRoll);Serial.println(" °");
    // Serial.print("Fuel Flow: ");Serial.print(fuelFlowValue);Serial.println(" GPH");
    // Serial.print("Manifold: ");Serial.print(manifoldValue);Serial.println(" IN Hg ALg");
    // Serial.print("Oil Press: ");Serial.print(oilPressValue);Serial.println(" PSI");
    // Serial.print("Oil Temp: ");Serial.print(oilTempValue);Serial.println(" °C");
    // Serial.print("CHT: ");Serial.print(chtValue);Serial.println(" °C");
    // Serial.println("==============================");
  }
  
  delay(50);
  
}

// Función para manejar variómetro y altímetro

void variometroAltimetro() {
  if (altitudValue < 19000.0f) {
    bandera_off = false; // Reiniciar bandera_off si altitud es menor a 19000 pies
  } else {
    bandera_off = true; // Activar bandera_off si altitud es 19000 pies o más
  }
  
  if (verSpeedValue != 0 ) {
    unsigned long now = millis();
    if (lastUpdate == 0) lastUpdate = now;
    float dt = (now - lastUpdate) / 60000.0f; // minutos
    lastUpdate = now;
    if (dt > 0) {
      altitudValue += verSpeedValue * dt;
    }
    if (altitudValue <= 0.0f) altitudValue = 0.0f;
  }

  // Cálculo automático de presión atmosférica (ISA) para escala Kolsman en hPa
  // Fórmula en pies: P = P0 * (1 - 6.87535e-6 * h) ^ 5.2559
  float hFt = clampf(altitudValue, 0.0f, 60000.0f);
  float ratio = 1.0f - (6.87535e-6f * hFt);
  ratio = clampf(ratio, 0.05f, 1.0f);
  atmosphericPressureHpa = SEA_LEVEL_PRESSURE_HPA * powf(ratio, 5.2559f);
}

// Función para manejar lógica del instrumento RPM
void manejoRPM() {
  
  // Rutina automática de arranque
  if (startRoutine) {
    unsigned long t = millis() - routineStart;
    if (routineStep == 0) { // Subir a 1000 rpm en 1.5s
      float frac = min(1.0f, t / 1500.0f);
      RPMValue = routineInitial + (1000 - routineInitial) * frac;
      if (frac >= 1.0f) {
        routineStep = 1;
        routineStart = millis();
      }
    } else if (routineStep == 1) { // Mantener 3s
      RPMValue = 1000;
      if (t >= 3000) {
        routineStep = 2;
        routineStart = millis();
      }
    } else if (routineStep == 2) { // Bajar a 456 rpm en 3s
      float frac = min(1.0f, t / 3000.0f);
      RPMValue = 1000 + (456 - 1000) * frac;
      if (frac >= 1.0f) {
        RPMValue = 456;
        startRoutine = false;
      }
    }
  }

  // varRPM según el estado de RPMNoise:
  // - Si RPMNoise está activo: varRPM = RPMValue + ruido
  // - Si RPMNoise está apagado: varRPM = RPMValue (sin ruido)
  if (RPMNoise && RPMValue > 0.0f) {
    int ruido = random(-5, 5); // de -5 a +5
    varRPM = RPMValue + ruido;
    varRPM = max(0.0f, varRPM); // Asegurar que no sea menor a 0
    varRPM = min(3000.0f, varRPM); // Asegurar que no sea mayor a 3000
  } else {
    varRPM = RPMValue; // Sin ruido, mostrar RPMValue directamente
  }
}

// Función para calcular horas y minutos de funcionamiento del tablero
void calcularHorasFuncionamiento() {
  unsigned long tiempoTranscurrido = millis() - horasStartMillis;
  
  // Convertir milisegundos a minutos totales
  unsigned long minutosTotales = tiempoTranscurrido / 60000;
  
  // Calcular horas y minutos
  horasFuncionamiento = (minutosTotales / 60) % 1000; // 0-999 horas
  minutosFuncionamiento = minutosTotales % 60; // 0-59 minutos
  
  // Cuando llega a 999:59, vuelve a 0:00
  // Esto se maneja automáticamente con el módulo 1000 en horas
}
  
// Función para manejar lógica del instrumento Reloj (hora de internet via NTP)
void manejoReloj() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Hora real de internet (formato 24h para soporte de zonas horarias)
    relojHoras = timeinfo.tm_hour;        // 0-23 (formato 24h)
    relojMinutos = timeinfo.tm_min;       // 0-59
    relojSegundos = timeinfo.tm_sec;      // 0-59
  } else {
    // Fallback: usar millis() si no hay conexión NTP
    relojHoras = (millis() / 3600000) % 24;
    relojMinutos = (millis() / 60000) % 60;
    relojSegundos = (millis() / 1000) % 60;
  }
}

// Función para manejar EGT simulado
void manejoEGT() {
  if (!useSimulatedEGT) return;

  unsigned long now = millis();
  if (lastEGTMillis == 0) {
    lastEGTMillis = now;
    return;
  }

  float dt = (now - lastEGTMillis) / 1000.0f;
  lastEGTMillis = now;

  float dtClamped = clampf(dt, 0.0f, 0.2f);
  
  float rpmNorm = clampf(RPMValue / 2700.0f, 0.0f, 1.0f);
  float mixture = clampf(chtMixture, 0.0f, 1.0f);

  float distToPeak = fabsf(mixture - EGT_PEAK_MIXTURE);
  float peakShape = 1.0f - clampf(distToPeak / 0.45f, 0.0f, 1.0f);

  float egtTarget = 850.0f + (350.0f * rpmNorm) + (380.0f * peakShape);

  if (RPMValue <= 100.0f) {
    egtTarget = EGT_MIN_F;
  }

  egtValue += (egtTarget - egtValue) * EGT_ALPHA * dtClamped;
  egtValue = clampf(egtValue, EGT_MIN_F, EGT_MAX_F);
}