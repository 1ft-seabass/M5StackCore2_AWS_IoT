#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiMulti.h>
#include <M5Core2.h>

// Wi-Fi の設定
char *ssid = "Wi-Fi SSID";
char *password = "Wi-Fi PASSWORD";
 
// AWS IoT の設定
const char *endpoint = "AWS IoT Endpoint";
const int port = 8883;
char *pubTopic = "/pub/sample123";
char *subTopic = "/sub/sample123";
 
// AWS IoT Amazon Root CA 1
static const char AWS_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)EOF";
 
// AWS IoT Device Certificate
static const char AWS_CERT_CRT[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)KEY";
 
// AWS IoT Device Private Key
static const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----
-----END RSA PRIVATE KEY-----
)KEY";

WiFiClientSecure httpsClient;
PubSubClient mqttClient(httpsClient);
WiFiMulti WiFiMulti;

// ArduinoJson v6 の記述で JSON を格納する StaticJsonDocument を準備
StaticJsonDocument<2048> root;
// JSON 送信時に使う buffer
char pubJson[255];
// 一定時間カウント用の送られた時間を管理する変数
long messageSentAt = 0;
// 一定時間カウント用の実際に待つ時間
long waitTime = 5000;

void setup()
{
    M5.begin(true, true, true, true);
    
    // Serial.begin(115200);
    delay(10);

    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.clear(BLACK);
    M5.Lcd.println("AWS IoT Connect begin....");
    M5.Lcd.drawFastHLine(0, 20, 320, WHITE);
    
    Serial.println();
    Serial.println("begin....");

    // Wi-Fi の接続
    WiFiMulti.addAP(ssid, password);

    M5.Lcd.setCursor(0, 30);
    M5.Lcd.print("[Wi-Fi] ");
    Serial.println();
    Serial.println("Waiting for WiFi... ");

    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    M5.Lcd.println(" Connected.");

    Serial.println("Connected.");
    Serial.println(pubTopic);
    Serial.println(subTopic);
   
    // AWS IoT につなぐ MQTT の設定
    httpsClient.setCACert(AWS_CERT_CA);
    httpsClient.setCertificate(AWS_CERT_CRT);
    httpsClient.setPrivateKey(AWS_CERT_PRIVATE);
    mqttClient.setServer(endpoint, port);
    mqttClient.setCallback(mqttCallback);

    M5.Lcd.setCursor(0, 55);
    
    connectAWSIoT();

    // INFO
    M5.Lcd.printf("sub: ");
    M5.Lcd.println(subTopic);
    M5.Lcd.printf("pub: ");
    M5.Lcd.println(pubTopic);
    M5.Lcd.drawFastHLine(0, 110, 320, WHITE);
    M5.Lcd.setCursor(0, 112);
    M5.Lcd.print("message:");
    M5.Lcd.drawFastHLine(0, 156, 320, WHITE);
    M5.Lcd.setCursor(0, 158);
    M5.Lcd.print("button:");
  
}
  
void connectAWSIoT() {
  Serial.println("connectAWSIoT");
  M5.Lcd.print("[AWS IoT] ");
  while (!mqttClient.connected()) {
    Serial.print(".");
    if (mqttClient.connect("ESP32_device")) {
        Serial.println("Connected.");
        M5.Lcd.println("Connected.");
        int qos = 0;
        mqttClient.subscribe(subTopic, qos);
        Serial.println("Subscribed.");
        // 接続時にデータの送信

        // AWS IoT に送るときの JSON データ作成時に使う DynamicJsonDocument を準備
        DynamicJsonDocument doc(1024);
        doc["value"] = "Connected";
        serializeJson(doc, pubJson);
        mqttClient.publish(pubTopic, pubJson);
    } else {
        Serial.print("Failed. Error state=");
        Serial.print(mqttClient.state());
        // 接続できないときはリトライ
        delay(5000);
    }
  }
}
  
void mqttCallback (char* topic, byte* payload, unsigned int length) {
  
  String str = "";
  Serial.print("Received. topic=");
  Serial.println(topic);
  for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
      str += (char)payload[i];
  }
  Serial.print("\n");

  // 受け取った文字列を JSON データ化(Deserialize)
  DeserializationError error = deserializeJson(root, str);
  
  // JSON データ化 失敗時
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  // Serial.println(root["messeage"]);
  // AWS IoT から受信した JSON データの中の messeage 値を表示
  const char* message = root["message"];
  M5.Lcd.setCursor(0, 112+18);
  M5.Lcd.fillRect(0, 112+18, 320, 14, BLACK);
  M5.Lcd.println(message);
}

void mqttLoop() {
  if (!mqttClient.connected()) {
      connectAWSIoT();
  }
  mqttClient.loop();
}

void pressMessage(String _msg){
  Serial.println(_msg);
  M5.Lcd.setCursor(0, 158+18);
  M5.Lcd.fillRect(0, 158+18, 320, 14, BLACK);
  M5.Lcd.print(_msg);
  // AWS IoT にメッセージ送信
  DynamicJsonDocument doc(1024);
  doc["value"] = _msg;
  serializeJson(doc, pubJson);
  mqttClient.publish(pubTopic, pubJson);
}

void loop() {
  mqttLoop();

  M5.update();

  if(M5.BtnA.wasPressed()) {
    pressMessage("Press A");
  }
  
  if(M5.BtnB.wasPressed()) {
    pressMessage("Press B");
  }
  
  if(M5.BtnC.wasPressed()) {
    pressMessage("Press C");
  }

  // waitTime のミリ秒ごとにチェックする（初期値 5000 ミリ秒 = 5 秒）
  long now = millis();
  long spanTime = now - messageSentAt;
  if (spanTime > waitTime) {
    messageSentAt = now;  // 送られた時間を現在 now にする
    Serial.println("データ送信:");
    Serial.println(messageSentAt);
    Serial.print(" ミリ秒");
    // AWS IoT にメッセージ送信
    DynamicJsonDocument doc(1024);
    doc["value"] = "Press C";
    serializeJson(doc, pubJson);
    mqttClient.publish(pubTopic, pubJson);
  }
}
