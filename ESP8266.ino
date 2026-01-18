#define BLYNK_TEMPLATE_ID "TMPL6fMmwcnWB"
#define BLYNK_TEMPLATE_NAME "Distance"
#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char auth[] = "88USLf7Em1NbxMG8CtxFiPXAHibATzrH";
char ssid[] = "BAO NGOC";
char pass[] = "bangbang2001";

// Moving Average buffer
#define NUM_SAMPLES 5
long samples[NUM_SAMPLES];
int indexSample = 0;

// Exponential smoothing
float smoothPosition = 0;
float alpha = 0.2; // hệ số làm mượt (0.1–0.3)

// Buffer nhận dữ liệu từ STM32
String inputString = "";
int set_point = 170;   // ✅ giá trị đặt sẵn trên ESP
int duty = 0;
int ledStatus = 0;
long position = 0;

void setup() {
  Serial.begin(9600);    // UART nhận dữ liệu từ STM32
  Blynk.begin(auth, ssid, pass);
  pinMode(BUILTIN_LED, OUTPUT);

  for (int i = 0; i < NUM_SAMPLES; i++) {
    samples[i] = 0;
  }
}

// Moving Average + Exponential Smoothing cho position
long filterPosition(long newValue) {
  if (newValue == 0) return (long)smoothPosition;

  samples[indexSample] = newValue;
  indexSample = (indexSample + 1) % NUM_SAMPLES;

  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) sum += samples[i];
  long avg = sum / NUM_SAMPLES;

  smoothPosition = alpha * avg + (1 - alpha) * smoothPosition;
  return (long)smoothPosition;
}

// Relay control từ Blynk (Virtual Pin V0)
BLYNK_WRITE(V0) {
  int relayState = param.asInt(); // 1 = ON, 0 = OFF
  if (relayState == 1) {
    Serial.println("RELAY:ON");   // gửi lệnh qua UART cho STM32
  } else {
    Serial.println("RELAY:OFF");
  }
}

// Parse dữ liệu từ STM32: "POSITION:123,DUTY:75,LED:2"
void parseSTM32Data(String data) {
  int posIndex  = data.indexOf("POSITION:");
  int dutyIndex = data.indexOf("DUTY:");
  int ledIndex  = data.indexOf("LED:");

  if (posIndex != -1 && dutyIndex != -1 && ledIndex != -1) {
    long rawPos = data.substring(posIndex + 9, dutyIndex - 1).toInt();
    duty        = data.substring(dutyIndex + 5, ledIndex - 1).toInt();
    ledStatus   = data.substring(ledIndex + 4).toInt();

    // Lọc position
    position = filterPosition(rawPos);

    // ✅ In ra màn hình giám sát
    Serial.print("SetPoint: ");
    Serial.print(set_point);
    Serial.print(" | Position: ");
    Serial.print(position);
    Serial.print(" | Duty: ");
    Serial.print(duty);
    Serial.print(" | LED: ");
    Serial.println(ledStatus);

    // Gửi dữ liệu lên Blynk
    Blynk.virtualWrite(V6, set_point);
    Blynk.virtualWrite(V5, position);
    Blynk.virtualWrite(V4, duty);

    // Hiển thị trạng thái LED trên Blynk + Alarm & Sound
    if (ledStatus == 1) {
      // LED đỏ
      Blynk.virtualWrite(V1, 255);
      Blynk.virtualWrite(V2, 0);
      Blynk.virtualWrite(V3, 0);

      // 🔔 Kích hoạt Alarm & Sound
      Blynk.virtualWrite(V7, 1);

    } else if (ledStatus == 2) {
      // LED xanh lá
      Blynk.virtualWrite(V1, 0);
      Blynk.virtualWrite(V2, 255);
      Blynk.virtualWrite(V3, 0);

      // Tắt Alarm
      Blynk.virtualWrite(V7, 0);

    } else if (ledStatus == 3) {
      // LED xanh dương
      Blynk.virtualWrite(V1, 0);
      Blynk.virtualWrite(V2, 0);
      Blynk.virtualWrite(V3, 255);

      // Tắt Alarm
      Blynk.virtualWrite(V7, 0);
    } else {
      // Không có LED nào → tắt Alarm
      Blynk.virtualWrite(V7, 0);
    }
  }
}

void loop() {
  Blynk.run();

  // Nhận dữ liệu từ STM32
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      parseSTM32Data(inputString);
      inputString = "";
    } else {
      inputString += c;
    }
  }

  delay(20);
}
