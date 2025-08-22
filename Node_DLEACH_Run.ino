/*
NodeData nodes[] = {
  {1, 30.0, 25.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {2, 47.0, 10.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {3, 30.0, 32.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {4, 23.0, 10.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {5, 9.0, 26.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {6, 54.0, 34.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {7, 36.0, 5.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {8, 5.0, 2.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {9, 48.0, 25.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {10, 13.0, 19.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {11, 22.0, 34.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {12, 8.0, 16.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {13, 14.0, 4.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {14, 21.0, 21.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {15, 43.0, 13.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
  {16, 20.0, 32.0, 100.00, false, false, 0.0, 0.0, 0, 0.0, 0.0, 0, false, 0, 0.0, false},
};
*/
// --- PHASE 1 & 2 SENSOR NODE SIDE ---
// Panel: Node - Menangani Informasi Clustering dari Sink

#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>

// ==== PIN & DEFINISI SENSOR ====
#define SS_PIN 22
#define RST_PIN 15
#define DHT_PIN 14
#define MQ_PIN 27
#define VOLTAGE_PIN 13
#define ARUS_PIN 26
#define DIO0_PIN 21
#define DHTTYPE DHT11

#define SINK_ID 999

DHT dht(DHT_PIN, DHTTYPE);

// ==== KONFIGURASI NODE ====
const int nodeID = 7;  // Ganti sesuai dengan ID Node
const float posX = 36; // menyesuaikan
const float posY = 5;  // menyesuaiakn

// ==== STATUS NODE ====
bool isCH = false;
int myCH_ID = -1;
bool waitingAck1 = false;
bool readyToAggregate = false;
bool reqAggReceived = false;
bool dataSentToCH = false;
bool dataSentToSink = false;
bool ackReceivedFromCH = false;
bool waitingAckAgg = false;
bool hasSentAggData = false;
bool txPowerHighSet = false;
bool txPowerLowSet = false;

// ==== TIMESTAMP ====
unsigned long sendTime = 0;
unsigned long lastREQ1Time = 0;
unsigned long timestampINIT = 0;
unsigned long timestampCLINFO = 0;
unsigned long timestampREQDATA = 0;
unsigned long timestampDATAtoCH = 0;
unsigned long timestampACKfromCH = 0;
unsigned long timestampAGGtoSink = 0;

const unsigned long ackTimeout1 = 700;
const unsigned long ackTimeoutCM = 500;

// ==== DATA ====
String aggregatedData = "";
String sensorDataCM = "";
int myClusterNo = -1;

// ================ deklarasi Void===========
void listenToSink();
void sendREADY_AGG();
void requestDataFromCM();
void collectSensorDataFromCM();
void sendAggregatedDataToSink();
void listenREQCMfromCH();
void listenACKCMfromCH();
void resetNodeStateForNewRound();
void sendInit1(float voltage, float current);
void handleClusterInfo(String role, int chID);
void sendClusterACK();
void sendACKtoCM(int cmID);
void readSensorAndSendToCH();


// ==== SETUP ====
void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(1000);

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setTxPower(8);
  dht.begin();
  Serial.println("LoRa Node Initialized");
}

void loop() {
  listenToSink();

  if (waitingAck1 && millis() - lastREQ1Time > ackTimeout1) {
    Serial.println("ACK1 timeout. Return to listen mode.");
    waitingAck1 = false;
  }

  if (waitingAckAgg && millis() - sendTime > 3000) {
    Serial.println("⏳ Timeout ACK_AGG. Masuk mode listen.");
    waitingAckAgg = false;
  }
}

// ==== RESET UNTUK RONDE BARU ====
void resetNodeStateForNewRound() {
  isCH = false;
  myCH_ID = -1;
  waitingAck1 = false;
  readyToAggregate = false;
  dataSentToCH = false;
  dataSentToSink = false;
  ackReceivedFromCH = false;
  waitingAckAgg = false;
  aggregatedData = "";
  hasSentAggData = false;  // Reset untuk ronde baru
}


// ==== FASE INIT & CLUSTER INFO ====
/*
void listenToSink() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) msg += (char)LoRa.read();

    // Filter awal: hanya proses pesan dari SINK
    if (!msg.startsWith(String(SINK_ID) + ";")) {
      Serial.println("⚠️ Pesan bukan dari SINK. Abaikan: " + msg);
      return;
    }

    if (msg.indexOf(";AGGDATA;") > 0) {
      Serial.println("⚠️ AGGDATA diterima, tapi ini untuk SINK. Abaikan.");
      return;
    }

    Serial.println("Received from Sink: " + msg);

    // ============ INIT PHASE ==============
    if (msg.indexOf(";REQ1;") > 0) {
      int targetID = msg.substring(msg.indexOf(";REQ1;") + 6).toInt();
      if (targetID == nodeID) {
        float voltage = analogRead(VOLTAGE_PIN) * (4.2 / 2046.0);
        float current = analogRead(ARUS_PIN) * (492.0 / 2046.0);
        sendInit1(voltage, current);
        waitingAck1 = true;
        lastREQ1Time = millis();
        timestampINIT = millis();
        Serial.println("⏱️ [Node] Delay dari REQ1 ke INIT1: " + String(timestampINIT - lastREQ1Time) + " ms");
      }

    // ============ ACK1 ====================
    } else if (msg.indexOf(";ACK1;") > 0) {
      int ackID = msg.substring(msg.indexOf(";ACK1;") + 6).toInt();
      if (ackID == nodeID) {
        Serial.println("✅ ACK1 diterima dari Sink.");
        waitingAck1 = false;
      }

    // ============ CLUSTER INFO ============
    } else if (msg.indexOf(";CLINFO;") > 0) {
      // Format: SINK_ID;CLINFO;nodeID;ROLE;CH_ID
      String parts[6];
      int idx = 0;
      int lastIdx = 0;

      for (int i = 0; i < msg.length(); i++) {
        if (msg.charAt(i) == ';' || i == msg.length() - 1) {
          int endIdx = (msg.charAt(i) == ';') ? i : i + 1;
          parts[idx++] = msg.substring(lastIdx, endIdx);
          lastIdx = i + 1;
          if (idx >= 6) break;
        }
      }

      if (idx >= 5) {
        String role = parts[3];     // CH / CM / NONE / SELF
        int chID = parts[4].toInt();  // bisa 0, atau CH ID
        handleClusterInfo(role, chID);
        timestampCLINFO = millis();
        Serial.println("⏱️ [Node] Waktu terima CLINFO: " + String(timestampCLINFO) + " ms");
      } else {
        Serial.println("❌ Format CLINFO tidak valid: " + msg);
      }

    // ============ REQUEST AGGREGATION ============
    } else if (msg.indexOf(";REQ_AGG;") > 0) {
      int target = msg.substring(msg.indexOf(";REQ_AGG;") + 9).toInt();
      if (target == nodeID) {
        reqAggReceived = true;
        Serial.println("📥 REQ_AGG diterima dari Sink");
      }

    // ============ ACK AGGREGASI =============
    } else if (msg.indexOf(";ACK_AGG;") > 0) {
      int id = msg.substring(msg.indexOf(";ACK_AGG;") + 9).toInt();
      if (id == nodeID) {
        Serial.println("✅ ACK_AGG diterima. Node kembali ke listen mode.");
        waitingAckAgg = false;
      }

    // ============ NEW ROUND ===============
    } else if (msg.indexOf(";NEWROUND;") > 0) {
      if (isCH && !hasSentAggData) {
        Serial.println("⏳ Saya CH dan belum kirim AGGDATA. Abaikan NEWROUND.");
        return;  // Jangan reset dulu
      }
      resetNodeStateForNewRound();
      Serial.println("♻️ Node reset untuk ronde baru");

    // ============ UNKNOWN / UNHANDLED ============
    } else {
      Serial.println("⚠️ Pesan tidak dikenali atau belum ditangani: " + msg);
    }
  }
}
*/
void listenToSink() {
  int packetSize = LoRa.parsePacket();
  if (txPowerHighSet && !txPowerLowSet) {
    LoRa.setTxPower(2);  
    txPowerLowSet = true;
  }
  if (packetSize) {
    String msg = "";
    while (LoRa.available()) msg += (char)LoRa.read();

    if (!msg.startsWith(String(SINK_ID) + ";")) {
      Serial.println("⚠️ Pesan bukan dari SINK. Abaikan: " + msg);
      return;
    }

    if (msg.indexOf(";AGGDATA;") > 0) {
      Serial.println("⚠️ AGGDATA diterima, tapi ini untuk SINK. Abaikan.");
      return;
    }

    Serial.println("Received from Sink: " + msg);

    if (msg.indexOf(";REQ1;") > 0) {
      int targetID = msg.substring(msg.indexOf(";REQ1;") + 6).toInt();
      if (targetID == nodeID) {
        float voltage = analogRead(VOLTAGE_PIN) * (4.2 / 2046.0);
        float current = analogRead(ARUS_PIN) * (492.0 / 2046.0);
        sendInit1(voltage, current);
        waitingAck1 = true;
        lastREQ1Time = millis();
        timestampINIT = millis();
        Serial.println("⏱️ [Node] Delay dari REQ1 ke INIT1: " + String(timestampINIT - lastREQ1Time) + " ms");
      }

    } else if (msg.indexOf(";ACK1;") > 0) {
      int ackID = msg.substring(msg.indexOf(";ACK1;") + 6).toInt();
      if (ackID == nodeID) {
        Serial.println("✅ ACK1 diterima dari Sink.");
        waitingAck1 = false;
      }

    } else if (msg.indexOf(";CLINFO;") > 0) {
      String parts[6];
      int idx = 0;
      int lastIdx = 0;

      for (int i = 0; i < msg.length(); i++) {
        if (msg.charAt(i) == ';' || i == msg.length() - 1) {
          int endIdx = (msg.charAt(i) == ';') ? i : i + 1;
          parts[idx++] = msg.substring(lastIdx, endIdx);
          lastIdx = i + 1;
          if (idx >= 6) break;
        }
      }

      if (idx >= 5) {
        String role = parts[3];
        int chID = parts[4].toInt();
        handleClusterInfo(role, chID);
        timestampCLINFO = millis();
        Serial.println("⏱️ [Node] Waktu terima CLINFO: " + String(timestampCLINFO) + " ms");
      } else {
        Serial.println("❌ Format CLINFO tidak valid: " + msg);
      }

    } else if (msg.indexOf(";REQ_AGG;") > 0) {
      int target = msg.substring(msg.indexOf(";REQ_AGG;") + 9).toInt();
      if (target == nodeID) {
        Serial.println("📥 REQ_AGG diterima dari Sink");

        // Tambahkan delay berdasarkan ID node
        delay(nodeID * 400);

        // Kirim AGGDATA langsung
        sendSensorDataToSink();
        hasSentAggData = true;
        dataSentToSink = true;
        waitingAckAgg = true;
        sendTime = millis();
      }

    } else if (msg.indexOf(";ACK_AGG;") > 0) {
      int id = msg.substring(msg.indexOf(";ACK_AGG;") + 9).toInt();
      if (id == nodeID) {
        Serial.println("✅ ACK_AGG diterima. Node kembali ke listen mode.");
        waitingAckAgg = false;
      }

    } else if (msg.indexOf(";NEWROUND;") > 0) {
      if (isCH && !hasSentAggData) {
        Serial.println("⏳ Saya CH dan belum kirim AGGDATA. Abaikan NEWROUND.");
        return;
      }
      resetNodeStateForNewRound();
      Serial.println("♻️ Node reset untuk ronde baru");

    } else {
      Serial.println("⚠️ Pesan tidak dikenali atau belum ditangani: " + msg);
    }
  }
}

void sendInit1(float voltage, float current) {
  char buffer[80];
  snprintf(buffer, sizeof(buffer), "%d;INIT1;%d;%.2f,%.2f;%.2f,%.2f", SINK_ID, nodeID, posX, posY, voltage, current);
  LoRa.beginPacket();
  LoRa.print(buffer);
  LoRa.endPacket();
  Serial.println("Sent INIT1 to Sink: " + String(buffer));
}

void handleClusterInfo(String role, int chID) {
  if (role == "CH") {
    isCH = true;
    myCH_ID = nodeID;
    Serial.println("Saya adalah Cluster Head.");
  } else if (role == "CM") {
    isCH = false;
    myCH_ID = chID;
    Serial.println("Saya adalah anggota cluster milik CH ID: " + String(myCH_ID));
  } else {
    isCH = false;
    myCH_ID = -1;
    Serial.println("Saya tidak tergabung ke cluster manapun.");
  }

  sendClusterACK();
  // ✅ Kirim sinyal CH siap AGGDATA
 
}


void sendClusterACK() {
  delay(nodeID * 50 + random(10, 40));
  String ackMsg = String(SINK_ID) + ";ACK_cluster;" + String(nodeID);
  LoRa.beginPacket();
  LoRa.print(ackMsg);
  LoRa.endPacket();
  Serial.println("Sent ACK_cluster to Sink: " + ackMsg);
}


//====================== Fase Gregasi Data ===========================
//====================================================||
//=========== Peran Node sebagai CH dan CM============||
//====================================================||
void sendSensorDataToSink() {
  float voltage = analogRead(VOLTAGE_PIN) * (4.2 / 2046.0);
  float current = analogRead(ARUS_PIN) * (492.0 / 2046.0);
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float gas = analogRead(MQ_PIN) * (100.0 / 2046.0);

  String msg = String(SINK_ID) + ";AGGDATA;" + String(nodeID) + ";" +
               String(voltage, 2) + "," + String(current, 2) + ";" +
               String(temperature, 2) + "," + String(humidity, 2) + "," + String(gas, 2);

  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  delay(100);

  waitingAckAgg = true;
  sendTime = millis();
  hasSentAggData = true;  // Setelah pengiriman berhasil
  Serial.println("📤 AGGDATA terkirim ke Sink: " + msg);
}

