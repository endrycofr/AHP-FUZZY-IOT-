#include <WiFi.h>
#include "Fuzzy.h"

String apiKey = "1G6ESH0J41RP60LC";     // Enter your Write API key from ThingSpeak
const char *ssid = "akubisa";         // ganti dengan SSID WiFi dan kunci WPA2 Anda
const char *pass = "pastibisa";
const char *server = "api.thingspeak.com";
const int sensor_pin = A0;  /* Soil moisture sensor O/P pin */
const int ldr_pin = 32;

WiFiClient client;

Fuzzy *fuzzy = new Fuzzy();
FuzzySet *lowMoisture = new FuzzySet(0, 0, 0, 30);
FuzzySet *normalMoisture = new FuzzySet(20, 30, 30, 70);
FuzzySet *highMoisture = new FuzzySet(30, 70, 100, 100);


FuzzySet *lowLight = new FuzzySet(0, 0, 0, 250);
FuzzySet *normalLight = new FuzzySet(250, 300, 300, 450);
FuzzySet *highLight = new FuzzySet(450, 500, 500, 600);

// Matriks perbandingan berpasangan
float pairwiseMatrix[2][2] = {
  {1, 3},
  {0.33, 1}
};

// Matriks bobot akhir AHP
float ahpWeights[2] = {0};

void calculateAHPWeights() {
  int size = sizeof(pairwiseMatrix) / sizeof(pairwiseMatrix[0]);

  // Normalisasi matriks perbandingan berpasangan
  float normalizedMatrix[size][size];
  float sumCols[size] = {0};

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      sumCols[j] += pairwiseMatrix[i][j];
    }
  }

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      normalizedMatrix[i][j] = pairwiseMatrix[i][j] / sumCols[j];
    }
  }

  // Menghitung bobot akhir
  float sumRows[size] = {0};

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      sumRows[i] += normalizedMatrix[i][j];
    }
  }

  for (int i = 0; i < size; i++) {
    ahpWeights[i] = sumRows[i] / size;
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Connecting to " + String(ssid));

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // FuzzyInput
  FuzzyInput *moisture = new FuzzyInput(1);
  moisture->addFuzzySet(lowMoisture);
  moisture->addFuzzySet(normalMoisture);
  moisture->addFuzzySet(highMoisture);
  fuzzy->addFuzzyInput(moisture);

  FuzzyInput *ldr = new FuzzyInput(2);
  ldr->addFuzzySet(lowLight);
  ldr->addFuzzySet(normalLight);
  ldr->addFuzzySet(highLight);
  fuzzy->addFuzzyInput(ldr);

  calculateAHPWeights();
}

void loop() {
  int moistureValue = analogRead(sensor_pin);
  int ldrValue = analogRead(ldr_pin);
  
  float moisturePercentage = 100 - ((moistureValue / 4095.00) * 100);


  // Fuzzy Logic Calculation
  fuzzy->setInput(1, moisturePercentage); // Set input value for soil moisture
  fuzzy->setInput(2, ldrValue);           // Set input value for light intensity
  fuzzy->fuzzify();                       // Perform fuzzification

  // Fuzzy Output - Soil Moisture
  float lowMoistureOutput = lowMoisture->getPertinence();
  float normalMoistureOutput = normalMoisture->getPertinence();
  float highMoistureOutput = highMoisture->getPertinence();

  // Fuzzy Output - Light Intensity
  float lowLightOutput = lowLight->getPertinence();
  float normalLightOutput = normalLight->getPertinence();
  float highLightOutput = highLight->getPertinence();

  Serial.print("Moisture: ");
  Serial.print(moisturePercentage);
  Serial.print("%, Light Intensity: ");
  Serial.print(ldrValue);
  Serial.println("");
  Serial.print("Plant Condition: ");

  // Fuzzy Rule Evaluation
  float dryScore = (lowMoistureOutput * ahpWeights[0]) + (lowLightOutput * ahpWeights[1]);
  float moistScore = (normalMoistureOutput * ahpWeights[0]) + (normalLightOutput * ahpWeights[1]);
  float wetScore = (highMoistureOutput * ahpWeights[0]) + (highLightOutput * ahpWeights[1]);

  if (dryScore > moistScore && dryScore > wetScore) {
    Serial.println("Tanah kering, maka harus disiram ");
  }
  else if (moistScore > dryScore && moistScore > wetScore) {
    Serial.println("Tanah cukup basah, maka siram sedikit");
  }
  else if (wetScore > dryScore && wetScore > moistScore) {
    Serial.println("Tanah basah, maka tidak usah disiram");
  }

  
  // Send data to ThingSpeak
  if (client.connect(server, 80)) {
    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(moisturePercentage);
    postStr += "&field2=";
    postStr += String(ldrValue);
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
  }

  client.stop();
  delay(1000); // Wait 5 seconds before repeating the loop
}
