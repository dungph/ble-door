#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>


int pin_rfid_rst = 10;
int pin_rfid_ss = 2;
int pin_lock = 3;
int pin_door = 9;
int pin_lcd_clk = 8;
int pin_lcd_sda = 7;
LiquidCrystal_I2C lcd(0x27, 16, 2);

TaskHandle_t door_task_handle;
QueueHandle_t display_message_queue;



char *open_door_message_success = "WELCOME";
char *open_door_message_failed = "ACCESS DENIED";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);


  display_message_queue = xQueueCreate(10, sizeof(char *));

  xTaskCreate(taskDoorHandle, "Button open door", 1024, NULL, 1, NULL);
  xTaskCreate(taskOpenDoor, "Door open", 1024, NULL, 1, &door_task_handle);
  xTaskCreate(taskDisplay, "Display", 1024, NULL, 1, NULL);
  xTaskCreate(taskRead, "Read RFID", 4096, NULL, 1, NULL);
}
void loop() {}


void taskRead(void *p) {
  SPI.begin();                                 // Init SPI bus
  MFRC522 mfrc522(pin_rfid_ss, pin_rfid_rst);  // Create MFRC522 instance

  while (1) {
    vTaskDelay(500 / portTICK_PERIOD_MS);

    mfrc522.PCD_Init();  // Init MFRC522 card

    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      for (int i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i]);
        Serial.print(", ");
      }
      Serial.println();
      xTaskResumeFromISR(door_task_handle);
      xQueueSend(display_message_queue, &open_door_message_success, 1);
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    } else {
      Serial.println("failed to read");
    }
  }
}


void taskDisplay(void *p) {
  Wire.begin(pin_lcd_sda, pin_lcd_clk);
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
  int pin = pin_lock;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, 0);
  for (;;) {
    vTaskSuspend(NULL);
    digitalWrite(pin, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    digitalWrite(pin, 0);
  }
}

void taskDoorHandle(void *p) {
  int pin = pin_door;

  pinMode(pin, INPUT_PULLUP);
  for (;;) {
    if (digitalRead(pin) == 0) {
      vTaskResume(door_task_handle);
      xQueueSend(display_message_queue, &open_door_message_success, 1);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}