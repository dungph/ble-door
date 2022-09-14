#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

int rfid_rst_pin = 10;
int rfid_ss_pin = 2;

LiquidCrystal_I2C lcd(0x27, 16, 2);

TaskHandle_t door_task_handle;
QueueHandle_t display_message_queue;

int door_pin = 3;
int button_pin = 9;
int i2c_pins[] = { 8, 7 };

char *open_door_message_success = "WELCOME";
char *open_door_message_failed = "FAILED";
MFRC522 mfrc522(rfid_ss_pin, rfid_rst_pin);  // Create MFRC522 instance

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522 card

  display_message_queue = xQueueCreate(10, sizeof(char *));

  xTaskCreate(taskButtonHandle, "Button open door", 1024, &button_pin, 1, NULL);
  xTaskCreate(taskOpenDoor, "Door open", 1024, &door_pin, 1, &door_task_handle);
  xTaskCreate(taskDisplay, "Display", 1024, NULL, 1, NULL);
}


void loop() {
  // put your main code here, to run repeatedly:

  // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;


  //-------------------------------------------

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println(F("**Card Detected:**"));
  xTaskResumeFromISR(door_task_handle);
  //-------------------------------------------

  //mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid));  //dump some details about the card
  mfrc522.PICC_HaltA();

  delay(500);
  
}


void taskDisplay(void *p) {
  Wire.begin(7, 8);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(1, 0);
  char *str;
  for (;;) {
    if (xQueueReceive(display_message_queue, &str, 1) == pdPASS) {
      lcd.print(str);
      vTaskDelay(3000 / portTICK_PERIOD_MS);
      lcd.clear();
    }
  }
}
void taskOpenDoor(void *p) {
  int pin = *(int *)p;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, 0);
  for (;;) {
    vTaskSuspend(NULL);
    digitalWrite(pin, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    digitalWrite(pin, 0);
  }
}

void taskButtonHandle(void *p) {
  int pin = *(int *)p;

  pinMode(pin, INPUT_PULLUP);
  for (;;) {
    if (digitalRead(pin) == 0) {
      vTaskResume(door_task_handle);
      xQueueSend(display_message_queue, &open_door_message_success, 0);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}