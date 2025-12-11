#define BLYNK_TEMPLATE_ID "TMPL6sDmNmcyE"
#define BLYNK_TEMPLATE_NAME "demo"
#define BLYNK_AUTH_TOKEN "lB50_2_yFQjQNO4vfOOJPoWirF7TiOrp"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

ThreeWire myWire(26, 25, 27);// data clk rst
RtcDS1302<ThreeWire> Rtc(myWire);

#define SS_PIN 21
#define RST_PIN 22
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ135 34
#define BUZZER 15



MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo doorServo;
DHT dht(DHTPIN, DHTTYPE);
byte masterCard[4] = {227, 202, 48, 3};

int failCount = 0;
int GAS_THRESHOLD = 7000;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
BlynkTimer timer;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int D = 2;
int D1 = 32;
int D2 = 5;


void sendTempHumi(){
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) return;

  Firebase.RTDB.setFloat(&fbdo, "dht/nhietdo", t);
  Firebase.RTDB.setFloat(&fbdo, "dht/doam", h);

  Blynk.virtualWrite(V4, t);
  Blynk.virtualWrite(V5, h);

  Serial.print("Temp: "); Serial.print(t);
  Serial.print(" | Humi: "); Serial.println(h);
}

BLYNK_WRITE(V0){
  int value = param.asInt();
  digitalWrite(D, value);
  Firebase.RTDB.setInt(&fbdo, "bongden/trangthai", value);
}

BLYNK_WRITE(V1){
  int value = param.asInt();
  digitalWrite(D1, value);
  Firebase.RTDB.setInt(&fbdo, "bongden1/trangthai", value);
}

BLYNK_WRITE(V2){
  int value = param.asInt();
  digitalWrite(D2, value);
  Firebase.RTDB.setInt(&fbdo, "bongden2/trangthai", value);
}

BLYNK_WRITE(V3){
  int value = param.asInt();
  if(value == 1){
    openDoor();
    Firebase.RTDB.setInt(&fbdo, "cua_chinh/trangthai", 1);
    delay(3000);
    Firebase.RTDB.setInt(&fbdo, "cua_chinh/trangthai", 0);
    Blynk.virtualWrite(V3, 0);
  }
}

void readFirebase(){
  if(Firebase.RTDB.getInt(&fbdo, "bongden/trangthai")){
    int st = fbdo.intData();
    digitalWrite(D, st);
    Blynk.virtualWrite(V0, st);
  }

  if(Firebase.RTDB.getInt(&fbdo, "bongden1/trangthai")){
    int st = fbdo.intData();
    digitalWrite(D1, st);
    Blynk.virtualWrite(V1, st);
  }

  if(Firebase.RTDB.getInt(&fbdo, "bongden2/trangthai")){
    int st = fbdo.intData();
    digitalWrite(D2, st);
    Blynk.virtualWrite(V2, st);
  }

  if(Firebase.RTDB.getInt(&fbdo, "cua_chinh/trangthai")){
    int st = fbdo.intData();
    Blynk.virtualWrite(V3, st);
    if(st == 1){
      openDoor();
      Firebase.RTDB.setInt(&fbdo, "cua_chinh/trangthai", 0);
      Blynk.virtualWrite(V3, 0);
    }
  }
}

void failProtection() {
  failCount++;
  if (failCount == 2) {
    mfrc522.PCD_Reset();
    mfrc522.PCD_Init();
    failCount = 0;
  }
}

void setup(){
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  doorServo.attach(13);
  doorServo.write(0);
  dht.begin();

  pinMode(D, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(MQ135, INPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);


  WiFi.begin("ThanhTinh", "12345678");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Blynk.begin(BLYNK_AUTH_TOKEN, "ThanhTinh", "12345678");

  config.database_url = "https://fir-f139e-default-rtdb.asia-southeast1.firebasedatabase.app/";
  config.signer.tokens.legacy_token = "k8YUiilWYjWDe61bHZcsRe4d3IJaVMEaINBK95U5";

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Wire.begin(16, 17);
  display.begin(0x3C, true);
  display.clearDisplay();
  display.display();

  Rtc.Begin();
  if (!Rtc.GetIsRunning()) {
    Rtc.SetIsRunning(true);
  }

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);

  timer.setInterval(5000, readFirebase);
  timer.setInterval(4000, displayTempHumi);
  timer.setInterval(3000, sendTempHumi);
  timer.setInterval(2000, readMQ135);

}

void loop(){
  Blynk.run();
  timer.run();

  if (!mfrc522.PICC_IsNewCardPresent()) {
    failProtection();
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    failProtection();
    return;
  }

  failCount = 0;

  if (checkCard()) {
    openDoor();
    Firebase.RTDB.setInt(&fbdo, "cua_chinh/trangthai", 1);
    Blynk.virtualWrite(V3, 1);
    delay(3000);
    Firebase.RTDB.setInt(&fbdo, "cua_chinh/trangthai", 0);
    Blynk.virtualWrite(V3, 0);
  }

  mfrc522.PICC_HaltA();
}

bool checkCard() {
  for (byte i = 0; i < 4; i++) {
    if (mfrc522.uid.uidByte[i] != masterCard[i])
      return false;
  }
  return true;
}

void openDoor() {
  doorServo.write(180);
  delay(3000);
  doorServo.write(0);
}

void displayTempHumi(){
  RtcDateTime now = Rtc.GetDateTime();

  float h = dht.readHumidity();
  float temp = dht.readTemperature();
  if (isnan(h) || isnan(temp)) {
    h = 0;
    temp = 0;
}

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 0);
  display.printf("%02u/%02u/%04u  %02u:%02u:%02u", now.Day(), now.Month(), now.Year(), now.Hour(), now.Minute(), now.Second());

  display.setCursor(0, 20);
  display.printf("Nhiet do: %.1f C", temp);
  display.setCursor(0, 35);
  display.printf("Do am   : %.1f %%", h);
  display.display();
}

void readMQ135(){
  int gasValue = analogRead(MQ135);

  Firebase.RTDB.setInt(&fbdo, "mq135/gas", gasValue);
  Blynk.virtualWrite(V6, gasValue);

  if(gasValue > GAS_THRESHOLD){
    digitalWrite(BUZZER, HIGH);

    Firebase.RTDB.setInt(&fbdo, "mq135/canhbao", 1);
    Blynk.virtualWrite(V7, 1);
  }
  else{
    digitalWrite(BUZZER, LOW);
    Firebase.RTDB.setInt(&fbdo, "mq135/canhbao", 0);
    Blynk.virtualWrite(V7, 0);
  }
}

