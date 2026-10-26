/*
 * RFID vending machine (ESP32-S3)
 *
 * Students pay with a prepaid RFID card. An admin card opens a menu to add
 * students, add credit and see the transactions.
 *
 * Hardware: ESP32-S3, MFRC522 RFID reader, SPI TFT screen (TFT_eSPI), 4x4 keypad.
 * Data is stored in JSON files in the ESP32 flash (LittleFS):
 *   /Admin.json  /Students.json  /Products.json  /Transactions.json
 * Run the "create_database" sketch once to create them.
 *
 * Keys
 *   0..9  numbers / menu choice
 *   A     validate
 *   C     clear / return
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>   // screen pins are set in TFT_eSPI's User_Setup.h (see README)
#include <MFRC522.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // version 7
#include <Keypad.h>

// ---------------------------------------------------------------- pins

// RFID reader (default SPI bus)
const int RFID_SCK = 18;
const int RFID_MISO = 16;
const int RFID_MOSI = 17;
const int RFID_SS = 15;
const int RFID_RST = 7;

// Keypad 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {1, 2, 42, 41};
byte colPins[COLS] = {40, 39, 38, 36};

// ---------------------------------------------------------------- files

const char *ADMIN_FILE = "/Admin.json";
const char *STUDENTS_FILE = "/Students.json";
const char *PRODUCTS_FILE = "/Products.json";
const char *TRANSACTIONS_FILE = "/Transactions.json";

// ---------------------------------------------------------------- screen layout

const int LINE = 16;  // height of one text line with text size 2

TFT_eSPI tft = TFT_eSPI();
MFRC522 rfid(RFID_SS, RFID_RST);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------------------------------------------------------------- function list

void showWelcome();
bool readCardUid(String &uid);
char getUserType(const String &uid);

void adminMenu();
void addStudent();
void addCredit();
void showAllStudents();
void showTransactions();

void studentMenu(const String &uid);
bool showProduct(int productId, int &price);
bool buyProduct(const String &uid, int productId, int price);
void releaseProduct(int productId);

char waitKey();
int readMenuDigit();
int readProductNumber(int y);
int readAmount(int y);
bool confirm();
String readSerialLine(const char *prompt);

void clearScreen();
void clearLine(int y);
void showMessage(const String &text, uint16_t color);

bool loadJson(const char *path, JsonDocument &doc);
bool saveJson(const char *path, JsonDocument &doc);
JsonObject findStudent(JsonDocument &doc, const String &uid);

// ================================================================ setup / loop

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  clearScreen();

  if (!LittleFS.begin()) {
    tft.println("File system error");
    tft.println("Run create_database");
    Serial.println("Failed to mount LittleFS. Upload the create_database sketch first.");
    while (true) {
      delay(1000);
    }
  }

  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();

  tft.println("System ready");
  delay(1000);
  showWelcome();
}

void loop() {
  String uid;
  if (!readCardUid(uid)) {
    delay(50);
    return;
  }

  Serial.println("Card: " + uid);
  char type = getUserType(uid);

  if (type == 'A') {
    adminMenu();
  } else if (type == 'S') {
    studentMenu(uid);
  } else {
    showMessage("User not found", TFT_RED);
  }

  showWelcome();
}

void showWelcome() {
  clearScreen();
  tft.setCursor(10, 0);
  tft.println("Welcome!");
  tft.setCursor(10, 25);
  tft.println("Please scan your");
  tft.setCursor(10, 50);
  tft.println("card.");
}

// Returns true and the UID as "12 34 56 78" when a new card is on the reader.
bool readCardUid(String &uid) {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return false;
  }

  uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (i > 0) {
      uid += " ";
    }
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}

// 'A' admin, 'S' student, '0' unknown
char getUserType(const String &uid) {
  JsonDocument admin;
  if (loadJson(ADMIN_FILE, admin) && admin["Admin"]["UID"].as<String>() == uid) {
    return 'A';
  }

  JsonDocument students;
  if (loadJson(STUDENTS_FILE, students) && !findStudent(students, uid).isNull()) {
    return 'S';
  }

  return '0';
}

// ================================================================ admin

void adminMenu() {
  while (true) {
    clearScreen();
    tft.println("Admin Menu:");
    tft.println("0. Exit");
    tft.println("1. Add Student");
    tft.println("2. Add Credit");
    tft.println("3. Show All Students");
    tft.println("4. Show Transactions");

    switch (readMenuDigit()) {
      case 0: return;
      case 1: addStudent(); break;
      case 2: addCredit(); break;
      case 3: showAllStudents(); break;
      case 4: showTransactions(); break;
      default: showMessage("Invalid Input", TFT_RED); break;
    }
  }
}

// The new student scans the card, then the admin types the details in the Serial Monitor.
void addStudent() {
  clearScreen();
  tft.println("Add Student:");
  tft.println("Scan the card");
  tft.println("(C to cancel)");

  String uid;
  while (!readCardUid(uid)) {
    if (keypad.getKey() == 'C') {
      return;
    }
    delay(50);
  }

  JsonDocument doc;
  if (!loadJson(STUDENTS_FILE, doc)) {
    showMessage("Database error", TFT_RED);
    return;
  }
  if (!findStudent(doc, uid).isNull()) {
    showMessage("UID already exists", TFT_RED);
    return;
  }

  tft.println("UID: " + uid);
  tft.println("Type the details in");
  tft.println("the Serial Monitor");

  Serial.println("New student card: " + uid);
  String name = readSerialLine("Enter name:");
  String matricule = readSerialLine("Enter matricule:");
  String email = readSerialLine("Enter email:");
  int credit = readSerialLine("Enter initial credit:").toInt();

  JsonArray students = doc["Students"].is<JsonArray>() ? doc["Students"].as<JsonArray>()
                                                       : doc["Students"].to<JsonArray>();
  JsonObject student = students.add<JsonObject>();
  student["UID"] = uid;
  student["name"] = name;
  student["matricule"] = matricule;
  student["email"] = email;
  student["credit"] = credit;

  if (saveJson(STUDENTS_FILE, doc)) {
    showMessage("Student added with " + String(credit) + " credit", TFT_GREEN);
  } else {
    showMessage("Save failed", TFT_RED);
  }
}

void addCredit() {
  clearScreen();
  tft.println("Add Credit:");
  tft.println("Scan the card");
  tft.println("(C to cancel)");

  String uid;
  while (!readCardUid(uid)) {
    if (keypad.getKey() == 'C') {
      return;
    }
    delay(50);
  }

  JsonDocument doc;
  if (!loadJson(STUDENTS_FILE, doc)) {
    showMessage("Database error", TFT_RED);
    return;
  }
  JsonObject student = findStudent(doc, uid);
  if (student.isNull()) {
    showMessage("UID Not Found", TFT_RED);
    return;
  }

  int credit = student["credit"].as<int>();
  clearScreen();
  tft.println("Student Found:");
  tft.println(student["name"].as<String>());
  tft.println("Current Credit: " + String(credit));
  tft.println("Enter Credit Amount:");
  tft.println("A: Validate C: Return");

  int amount = readAmount(5 * LINE);
  if (amount <= 0) {
    return;  // cancelled
  }

  student["credit"] = credit + amount;
  if (saveJson(STUDENTS_FILE, doc)) {
    clearLine(6 * LINE);
    tft.println("Credit added!");
    tft.println("New Credit: " + String(credit + amount));
    delay(2000);
  } else {
    showMessage("Save failed", TFT_RED);
  }
}

void showAllStudents() {
  JsonDocument doc;
  if (!loadJson(STUDENTS_FILE, doc)) {
    showMessage("Database error", TFT_RED);
    return;
  }

  JsonArray students = doc["Students"].as<JsonArray>();
  for (size_t i = 0; i < students.size(); i++) {
    JsonObject student = students[i];
    clearScreen();
    tft.println("Student " + String(i + 1) + "/" + String(students.size()));
    tft.println("UID: " + student["UID"].as<String>());
    tft.println("Name: " + student["name"].as<String>());
    tft.println("Matricule: " + student["matricule"].as<String>());
    tft.println("Email: " + student["email"].as<String>());
    tft.println("Credit: " + String(student["credit"].as<int>()));
    tft.println();
    tft.println("0: Return 1: Next");

    int choice = readMenuDigit();
    while (choice != 0 && choice != 1) {
      choice = readMenuDigit();
    }
    if (choice == 0) {
      return;
    }
  }

  showMessage("No more students", TFT_WHITE);
}

void showTransactions() {
  JsonDocument doc;
  if (!loadJson(TRANSACTIONS_FILE, doc)) {
    showMessage("Database error", TFT_RED);
    return;
  }

  JsonArray transactions = doc["Transactions"].as<JsonArray>();
  const int PER_PAGE = 2;
  int total = transactions.size();

  if (total == 0) {
    showMessage("No transactions", TFT_WHITE);
    return;
  }

  for (int start = 0; start < total; start += PER_PAGE) {
    clearScreen();
    tft.println("Transactions:");
    for (int i = start; i < min(start + PER_PAGE, total); i++) {
      JsonObject t = transactions[i];
      tft.println("UID: " + t["UID"].as<String>());
      tft.println("Product ID: " + String(t["product_id"].as<int>()));
      tft.println("Price: " + String(t["price"].as<int>()));
      tft.println("-------------------");
    }
    tft.println("0: Exit 1: Next Page");

    int choice = readMenuDigit();
    while (choice != 0 && choice != 1) {
      choice = readMenuDigit();
    }
    if (choice == 0) {
      return;
    }
  }

  showMessage("No more transactions", TFT_WHITE);
}

// ================================================================ student

void studentMenu(const String &uid) {
  while (true) {
    // Reload each time so the credit shown is always up to date
    JsonDocument doc;
    if (!loadJson(STUDENTS_FILE, doc)) {
      showMessage("Database error", TFT_RED);
      return;
    }
    JsonObject student = findStudent(doc, uid);
    if (student.isNull()) {
      showMessage("Student not found!", TFT_RED);
      return;
    }
    int credit = student["credit"].as<int>();

    clearScreen();
    tft.println("Welcome! " + student["name"].as<String>());
    tft.println("Credit: " + String(credit));
    tft.println("Press 0 to Exit");
    tft.println("Enter product number:");

    int productId = readProductNumber(4 * LINE);
    if (productId == 0) {
      return;
    }

    int price = 0;
    if (!showProduct(productId, price)) {
      continue;  // not found or out of stock
    }

    if (credit < price) {
      clearLine(7 * LINE);
      tft.print("Not enough credit");
      delay(2000);
      continue;
    }

    clearLine(7 * LINE);
    tft.print("Press A to Validate");
    clearLine(8 * LINE);
    tft.print("Press C to Return");

    if (!confirm()) {
      continue;
    }

    if (buyProduct(uid, productId, price)) {
      clearLine(8 * LINE);
      tft.print("Releasing product...");
      releaseProduct(productId);
      delay(2000);
    }
    return;
  }
}

// Shows the product name and price. Returns false if it does not exist or is out of stock.
bool showProduct(int productId, int &price) {
  JsonDocument doc;
  if (!loadJson(PRODUCTS_FILE, doc)) {
    showMessage("Database error", TFT_RED);
    return false;
  }

  for (JsonObject product : doc["Products"].as<JsonArray>()) {
    if (product["product_id"].as<int>() != productId) {
      continue;
    }

    clearLine(5 * LINE);
    if (product["quantity"].as<int>() <= 0) {
      tft.print("Not available");
      delay(2000);
      return false;
    }

    price = product["price"].as<int>();
    tft.print(product["name"].as<String>());
    clearLine(6 * LINE);
    tft.print("Price: " + String(price));
    return true;
  }

  clearLine(5 * LINE);
  tft.print("Product not found");
  delay(2000);
  return false;
}

// Takes the price from the student's credit, removes one item from the stock
// and saves the transaction.
bool buyProduct(const String &uid, int productId, int price) {
  JsonDocument students;
  JsonDocument products;
  JsonDocument transactions;

  if (!loadJson(STUDENTS_FILE, students) || !loadJson(PRODUCTS_FILE, products) ||
      !loadJson(TRANSACTIONS_FILE, transactions)) {
    showMessage("Database error", TFT_RED);
    return false;
  }

  JsonObject student = findStudent(students, uid);
  JsonObject product;
  for (JsonObject p : products["Products"].as<JsonArray>()) {
    if (p["product_id"].as<int>() == productId) {
      product = p;
      break;
    }
  }

  // Check again with fresh data
  if (student.isNull() || product.isNull() || product["quantity"].as<int>() <= 0 ||
      student["credit"].as<int>() < price) {
    showMessage("Purchase refused", TFT_RED);
    return false;
  }

  student["credit"] = student["credit"].as<int>() - price;
  product["quantity"] = product["quantity"].as<int>() - 1;

  JsonArray list = transactions["Transactions"].is<JsonArray>() ? transactions["Transactions"].as<JsonArray>()
                                                                : transactions["Transactions"].to<JsonArray>();
  JsonObject t = list.add<JsonObject>();
  t["UID"] = uid;
  t["product_id"] = productId;
  t["price"] = price;

  if (!saveJson(STUDENTS_FILE, students) || !saveJson(PRODUCTS_FILE, products) ||
      !saveJson(TRANSACTIONS_FILE, transactions)) {
    showMessage("Save failed", TFT_RED);
    return false;
  }
  return true;
}

// Connect the motor or servo that drops the product here.
void releaseProduct(int productId) {
  Serial.println("Release product " + String(productId));
}

// ================================================================ keypad / serial input

char waitKey() {
  char key = NO_KEY;
  while (key == NO_KEY) {
    key = keypad.getKey();
    delay(10);
  }
  return key;
}

// Waits for one digit key and returns it (0..9).
int readMenuDigit() {
  while (true) {
    char key = waitKey();
    if (isdigit(key)) {
      return key - '0';
    }
  }
}

// Product numbers have 2 digits. 0 as first key = exit. C clears.
int readProductNumber(int y) {
  String input = "";
  while (true) {
    char key = waitKey();

    if (key == 'C') {
      input = "";
    } else if (isdigit(key)) {
      if (input.length() == 0 && key == '0') {
        return 0;
      }
      input += key;
    } else {
      continue;
    }

    clearLine(y);
    tft.print(input);
    if (input.length() == 2) {
      return input.toInt();
    }
  }
}

// Up to 4 digits, A to validate. C clears, or returns -1 (cancel) if nothing is typed.
int readAmount(int y) {
  String input = "";
  while (true) {
    char key = waitKey();

    if (isdigit(key) && input.length() < 4) {
      input += key;
    } else if (key == 'C') {
      if (input.length() == 0) {
        return -1;
      }
      input = "";
    } else if (key == 'A' && input.length() > 0) {
      return input.toInt();
    }

    clearLine(y);
    tft.print(input);
  }
}

// A = yes, C = no
bool confirm() {
  while (true) {
    char key = waitKey();
    if (key == 'A') return true;
    if (key == 'C') return false;
  }
}

String readSerialLine(const char *prompt) {
  Serial.println(prompt);
  while (Serial.available()) {
    Serial.read();  // drop old input
  }
  while (!Serial.available()) {
    delay(10);
  }
  String line = Serial.readStringUntil('\n');
  line.trim();  // removes the \r sent by the Serial Monitor
  return line;
}

// ================================================================ screen helpers

void clearScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
}

void clearLine(int y) {
  tft.fillRect(0, y, tft.width(), LINE, TFT_BLACK);
  tft.setCursor(0, y);
}

void showMessage(const String &text, uint16_t color) {
  clearScreen();
  tft.setTextColor(color, TFT_BLACK);
  tft.println(text);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  delay(2000);
}

// ================================================================ JSON database

// Reads a JSON file. A missing file gives an empty document.
// Returns false if the file is damaged, so the caller does not overwrite it.
bool loadJson(const char *path, JsonDocument &doc) {
  doc.clear();
  if (!LittleFS.exists(path)) {
    return true;
  }

  fs::File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("Cannot open %s\n", path);
    return false;
  }
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("Cannot read %s: %s\n", path, error.c_str());
    return false;
  }
  return true;
}

// Writes to a temporary file first, so a power cut cannot leave a half-written file.
bool saveJson(const char *path, JsonDocument &doc) {
  String tmp = String(path) + ".tmp";

  fs::File file = LittleFS.open(tmp, "w");
  if (!file) {
    Serial.printf("Cannot write %s\n", tmp.c_str());
    return false;
  }
  size_t written = serializeJson(doc, file);
  file.close();

  if (written == 0) {
    return false;
  }
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

JsonObject findStudent(JsonDocument &doc, const String &uid) {
  for (JsonObject student : doc["Students"].as<JsonArray>()) {
    if (student["UID"].as<String>() == uid) {
      return student;
    }
  }
  return JsonObject();
}
