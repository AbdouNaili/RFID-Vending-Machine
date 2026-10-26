/*
 * Creates the JSON database in the ESP32 flash (LittleFS).
 *
 * Upload this sketch ONCE before the vending_machine sketch.
 * WARNING: it formats the flash if needed and replaces all existing data.
 *
 * The UIDs below are fake. Replace them with the UIDs of your own cards
 * (scan a card with the vending machine and read the UID in the Serial Monitor).
 * UID format: hex bytes in capitals separated by spaces, for example "12 34 56 78".
 */

#include <LittleFS.h>
#include <ArduinoJson.h>  // version 7

bool writeFile(const char *path, const char *json);

bool writeFile(const char *path, const char *json) {
  fs::File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.printf("Cannot write %s\n", path);
    return false;
  }
  file.print(json);
  file.close();
  Serial.printf("%s created\n", path);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!LittleFS.begin(true)) {  // true = format if the file system is not valid
    Serial.println("Failed to mount LittleFS");
    return;
  }

  writeFile("/Admin.json", R"({
    "Admin": { "UID": "AA BB CC 01", "name": "Admin", "email": "admin@example.com" }
  })");

  writeFile("/Students.json", R"({
    "Students": [
      { "UID": "11 22 33 01", "name": "Student1", "matricule": "000000000001", "email": "student1@example.com", "credit": 400 },
      { "UID": "11 22 33 02", "name": "Student2", "matricule": "000000000002", "email": "student2@example.com", "credit": 200 },
      { "UID": "11 22 33 03", "name": "Student3", "matricule": "000000000003", "email": "student3@example.com", "credit": 10 }
    ]
  })");

  // product_id must have 2 digits (10 to 99)
  writeFile("/Products.json", R"({
    "Products": [
      { "product_id": 15, "name": "chips",       "price": 40,  "quantity": 10 },
      { "product_id": 17, "name": "soda",        "price": 50,  "quantity": 5 },
      { "product_id": 19, "name": "juice",       "price": 30,  "quantity": 0 },
      { "product_id": 20, "name": "bifa story",  "price": 120, "quantity": 10 },
      { "product_id": 25, "name": "candy choco", "price": 50,  "quantity": 2 }
    ]
  })");

  writeFile("/Transactions.json", R"({ "Transactions": [] })");

  // Check that every file can be read back
  const char *files[] = {"/Admin.json", "/Students.json", "/Products.json", "/Transactions.json"};
  for (const char *path : files) {
    fs::File file = LittleFS.open(path, "r");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    Serial.printf("%s: %s\n", path, error ? error.c_str() : "OK");
  }

  Serial.println("Done. Now upload the vending_machine sketch.");
}

void loop() {
}
