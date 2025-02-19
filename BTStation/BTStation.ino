//#define DEBUG
//#define AUTOREPORT

#include <Wire.h>
#include "ds3231.h"
#include <MFRC522.h>
#include <EEPROM.h>
#include <SPIFlash.h>

#define UART_SPEED 38400

// версия прошивки, номер пишется в чипы
#define FW_VERSION        108

#define BT_COMMAND_ENABLE   2 // светодиод ошибки (красный)
#define BUZZER_PIN        3 // пищалка
#define INFO_LED_PIN         4 // светодиод синий
#define RTC_ENABLE_PIN      5 // питание часов кроме батарейного
#define FLASH_ENABLE_PIN    7 // SPI enable pin
#define FLASH_SS_PIN      8 // SPI SELECT pin
#define RFID_RST_PIN      9 // рфид модуль reset
#define RFID_SS_PIN       10 // рфид модуль chip_select
//#define RFID_MOSI_PIN     11 // рфид модуль
//#define RFID_MISO_PIN     12 // рфид модуль
//#define RFID_SCK_PIN      13 // рфид модуль
#define BATTERY_PIN       A0 // замер напряжения батареи
#define ERROR_LED_PIN     A1 // светодиод ошибки (красный)

// номер станции в eeprom памяти
#define EEPROM_STATION_NUMBER 00 // 1 byte
// номер режима в eeprom памяти
#define EEPROM_STATION_MODE   10 // 1 byte
// коэфф. пересчета значения ADC в вольты = 0,00587
#define EEPROM_VOLTAGE_KOEFF  20 // 4 byte
// усиление сигнала RFID
#define EEPROM_GAIN       40 // 1 byte
// тип чипа, с которым должна работать станция
#define EEPROM_CHIP_TYPE    50 // 1 byte
// размер блока на флэше под данные команды
#define EEPROM_TEAM_BLOCK_SIZE  60 // 2 byte
// размер стираемого блока на флэше
#define EEPROM_FLASH_BLOCK_SIZE 70 // 2 byte
// минимальное напряжение батареи
#define EEPROM_BATTERY_LIMIT  80 // 4 byte

// команды
#define COMMAND_SET_MODE        0x80
#define COMMAND_SET_TIME        0x81
#define COMMAND_RESET_STATION     0x82
#define COMMAND_GET_STATUS        0x83
#define COMMAND_INIT_CHIP       0x84
#define COMMAND_GET_LAST_TEAMS      0x85
#define COMMAND_GET_TEAM_RECORD     0x86
#define COMMAND_READ_CARD_PAGE      0x87
#define COMMAND_UPDATE_TEAM_MASK    0x88
#define COMMAND_WRITE_CARD_PAGE     0x89
#define COMMAND_READ_FLASH        0x8a
#define COMMAND_WRITE_FLASH       0x8b
#define COMMAND_ERASE_FLASH_SECTOR    0x8c
#define COMMAND_GET_CONFIG        0x8d
#define COMMAND_SET_V_KOEFF       0x8e
#define COMMAND_SET_GAIN        0x8f
#define COMMAND_SET_CHIP_TYPE     0x90
#define COMMAND_SET_TEAM_FLASH_SIZE   0x91
#define COMMAND_SET_FLASH_BLOCK_SIZE  0x92
#define COMMAND_SET_BT_NAME       0x93
#define COMMAND_SET_BT_PIN        0x94
#define COMMAND_SET_BATTERY_LIMIT       0x95
#define COMMAND_SCAN_TEAMS       0x96
#define COMMAND_SEND_BT_COMMAND       0x97
#define COMMAND_GET_LAST_ERRORS       0x98

// размеры данных для команд
#define DATA_LENGTH_SET_MODE        1
#define DATA_LENGTH_SET_TIME        6
#define DATA_LENGTH_RESET_STATION     7
#define DATA_LENGTH_GET_STATUS        0
#define DATA_LENGTH_INIT_CHIP       4
#define DATA_LENGTH_GET_LAST_TEAMS      0
#define DATA_LENGTH_GET_TEAM_RECORD     2
#define DATA_LENGTH_READ_CARD_PAGE      2
#define DATA_LENGTH_UPDATE_TEAM_MASK    8
#define DATA_LENGTH_WRITE_CARD_PAGE     13
#define DATA_LENGTH_READ_FLASH        5
#define DATA_LENGTH_WRITE_FLASH       4  // and more according to data length
#define DATA_LENGTH_ERASE_FLASH_SECTOR    2
#define DATA_LENGTH_GET_CONFIG        0
#define DATA_LENGTH_SET_V_KOEFF       4
#define DATA_LENGTH_SET_GAIN        1
#define DATA_LENGTH_SET_CHIP_TYPE     1
#define DATA_LENGTH_SET_TEAM_FLASH_SIZE   2
#define DATA_LENGTH_SET_FLASH_BLOCK_SIZE  2
#define DATA_LENGTH_SET_BT_NAME       1
#define DATA_LENGTH_SET_BT_PIN        1
#define DATA_LENGTH_SET_BATTERY_LIMIT       4
#define DATA_LENGTH_SCAN_TEAMS 2
#define DATA_LENGTH_SEND_BT_COMMAND 1
#define DATA_LENGTH_GET_LAST_ERRORS 0

// ответы станции
#define REPLY_SET_MODE        0x90
#define REPLY_SET_TIME        0x91
#define REPLY_RESET_STATION     0x92
#define REPLY_GET_STATUS      0x93
#define REPLY_INIT_CHIP       0x94
#define REPLY_GET_LAST_TEAMS    0x95
#define REPLY_GET_TEAM_RECORD   0x96
#define REPLY_READ_CARD_PAGE    0x97
#define REPLY_UPDATE_TEAM_MASK    0x98
#define REPLY_WRITE_CARD_PAGE   0x99
#define REPLY_READ_FLASH      0x9a
#define REPLY_WRITE_FLASH     0x9b
#define REPLY_ERASE_FLASH_SECTOR  0x9c
#define REPLY_GET_CONFIG      0x9d
#define REPLY_SET_V_KOEFF       0x9e
#define REPLY_SET_GAIN        0x9f
#define REPLY_SET_CHIP_TYPE     0xa0
#define REPLY_SET_TEAM_FLASH_SIZE 0xa1
#define REPLY_SET_FLASH_BLOCK_SIZE  0xa2
#define REPLY_SET_BT_NAME     0xa3
#define REPLY_SET_BT_PIN      0xa4
#define REPLY_SET_BATTERY_LIMIT     0xa5
#define REPLY_SCAN_TEAMS     0xa6
#define REPLY_SEND_BT_COMMAND 0xa7
#define REPLY_GET_LAST_ERRORS 0xa8

// режимы станции
#define MODE_INIT   0
#define MODE_START_KP 1
#define MODE_FINISH_KP  2

// коды ошибок станции
#define OK        0
#define WRONG_STATION 1
#define RFID_READ_ERROR   2
#define RFID_WRITE_ERROR    3
#define LOW_INIT_TIME 4
#define WRONG_CHIP    5
#define NO_CHIP     6
#define BUFFER_OVERFLOW 7
#define WRONG_DATA    8
#define WRONG_UID   9
#define WRONG_TEAM    10
#define NO_DATA     11
#define WRONG_COMMAND 12
#define ERASE_ERROR   13
#define WRONG_CHIP_TYPE 14
#define WRONG_MODE    15
#define WRONG_SIZE    16
#define WRONG_FW_VERSION  17
#define WRONG_PACKET_LENGTH 18
#define FLASH_READ_ERROR  19
#define FLASH_WRITE_ERROR 20
#define EEPROM_READ_ERROR 21
#define EEPROM_WRITE_ERROR  22
#define BT_ERROR  23

//коды ошибок STARTUP
#define STARTUP_NUMBER  50 //STARTUP: incorrect station number in EEPROM
#define STARTUP_MODE  51 //STARTUP: incorrect station mode in EEPROM
#define STARTUP_VKOEFF  52 //STARTUP: incorrect Vkoeff in EEPROM
#define STARTUP_GAIN  53 //STARTUP: incorrect gain in EEPROM
#define STARTUP_CHIP_TYPE  54 //STARTUP: incorrect chip type in EEPROM
#define STARTUP_TEAM_SIZE  55 //STARTUP: incorrect team block size in EEPROM
#define STARTUP_ERASE_SIZE  56 //STARTUP: incorrect erase block size in EEPROM
#define STARTUP_BATTERY_LIMIT  57 //STARTUP: incorrect battery limit in EEPROM


//коды ошибок UART
#define UART_TIMEOUT  60 //UART: receive timeout
#define UART_PACKET_LENGTH  61 //UART: incorrect packet length
#define UART_CRC  62 //UART: CRC incorrect
#define UART_UNEXPECTED_BYTE  63 //UART: unexpected byte

//коды ошибок обработки чипа
#define PROCESS_READ_CHIP  70 //CARD PROCESSING: error reading chip
#define PROCESS_HW_CHIP_TYPE  71 //CARD PROCESSING: wrong hw chip type
#define PROCESS_SW_CHIP_TYPE  72 //CARD PROCESSING: wrong sw chip type
#define PROCESS_FW_VERSION  73 //CARD PROCESSING: wrong fw. version
#define PROCESS_INIT_TIME  74 //CARD PROCESSING: chip init time is due
#define PROCESS_CHIP_NUMBER  75 //CARD PROCESSING: wrong chip number
#define PROCESS_WRITE_CHIP  76 //CARD PROCESSING: error writing to chip
#define PROCESS_ALREADY_CHECKED  77 //CARD PROCESSING: chip already checked
#define PROCESS_FIND_FREE_PAGE  78 //CARD PROCESSING: error finding free page
#define PROCESS_SAVE_DUMP  79 //CARD PROCESSING: error saving dump
#define PROCESS_SEND_AUTOREPORT  80 //CARD PROCESSING: error sending autoreport

// страницы в чипе. 0-7 служебные, 8-... для отметок
#define PAGE_UID    0
#define PAGE_CHIP_SYS 3 // system[2] + тип_чипа[1] + system[1]
#define PAGE_CHIP_NUM 4 // номер_чипа[2] + тип_чипа[1] + версия_прошивки[1]
#define PAGE_INIT_TIME  5 // время инициализации[4]
#define PAGE_TEAM_MASK  6 // маска команды[2] + resserved[2]
//#define PAGE_RESERVED2  7 // reserved for future use[4]
#define PAGE_DATA_START 8 // 1st data page: номер КП[1] + время посещения КП[3]

#define NTAG213_ID      0x12
#define NTAG213_MARK    213
#define NTAG213_MAX_PAGE  40

#define NTAG215_ID      0x3e
#define NTAG215_MARK    215
#define NTAG215_MAX_PAGE  130

#define NTAG216_ID      0x6d
#define NTAG216_MARK    216
#define NTAG216_MAX_PAGE  226

// тип чипа
uint8_t chipType = NTAG215_ID;
// отметка для чипа
uint8_t NTAG_MARK = NTAG215_MARK;
// размер чипа в страницах
uint8_t TAG_MAX_PAGE = NTAG215_MAX_PAGE;

// размер записи лога (на 1 чип)
uint16_t TEAM_FLASH_SIZE = 1024;
uint16_t FLASH_BLOCK_SIZE = 4096;

// максимальное кол-во записей в логе
uint16_t maxTeamNumber = 1; // = (flashSize - FLASH_BLOCK_SIZE) / TEAM_FLASH_SIZE - 1;

// описание протокола
#define PACKET_ID     2
#define STATION_NUMBER_BYTE 3
#define LENGTH_BYTE     4
#define COMMAND_BYTE    5
#define DATA_START_BYTE   6

// тайм-аут приема команды с момента начала
#define receiveTimeOut 1000

// размер буфера последних команд
const uint8_t lastTeamsLength = 10;

// станция запоминает последние команды сюда
uint8_t lastTeams[lastTeamsLength * 2];
uint32_t lastTimeChecked = 0;
// количество отмеченных чипов в памяти
uint16_t totalChipsChecked = 0;

// станция запоминает последние коды ошибок сюда
uint8_t lastErrors[lastTeamsLength];

// по умолчанию номер станции и режим.
uint8_t stationNumber = 0;
uint8_t stationMode = MODE_INIT;
const uint32_t maxTimeInit = 600000UL; // одна неделя

// коэфф. перевода значения АЦП в напряжение для делителя 10кОм/2.2кОм
float voltageCoeff = 0.00578;

// минимальное напряжение батареи
float batteryLimit = 3;

uint8_t ntag_page[16]; // буфер для чтения из чипа через ntagRead4pages()

SPIFlash SPIflash(FLASH_SS_PIN); // флэш-память

// рфид-модуль
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
// коэфф. усиления антенны - работают только биты 4,5,6
uint8_t gainCoeff = 0x40; // [0, 16, 32, 48, 64, 80, 96, 112]

// хранение времени
struct ts systemTime;

// UART command buffer
uint8_t uartBuffer[256];
uint8_t uartBufferPosition = 0;
bool uartReady = false;
uint32_t uartTimeout = 1000;
bool receivingData = false;
uint32_t receiveStartTime = 0;
uint16_t batteryLevel = 500;
uint8_t batteryAlarmCount = 0;

// новая маска для замены в чипе
uint8_t newTeamMask[8];

// флаг последней команды для отсечки двойной отметки
uint16_t lastTeamFlag = 0;


// сброс контроллера
void(*resetFunc) () = 0; // declare reset function @ address 0

void setup()
{
  Serial.begin(UART_SPEED);

  analogReference(INTERNAL);

  pinMode(INFO_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BT_COMMAND_ENABLE, OUTPUT);

  digitalWrite(INFO_LED_PIN, LOW);
  digitalWrite(ERROR_LED_PIN, LOW);
  digitalWrite(BT_COMMAND_ENABLE, LOW);

  //даем питание на флэш
  pinMode(FLASH_ENABLE_PIN, OUTPUT);
  digitalWrite(FLASH_ENABLE_PIN, HIGH);
  delay(1);
  SPIflash.begin();

  //даем питание на часы
  pinMode(RTC_ENABLE_PIN, OUTPUT);
  digitalWrite(RTC_ENABLE_PIN, HIGH);
  delay(1);
  DS3231_init(DS3231_INTCN);
  DS3231_get(&systemTime);

  //читаем номер станции из памяти
  int c = eepromread(EEPROM_STATION_NUMBER);
  if (c == 255 || c == -1)
  {
#ifdef DEBUG
    Serial.print(F("!!! StationNumber"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_NUMBER);
    stationNumber = 0;
  }
  else stationNumber = c;

  //читаем номер режима из памяти
  c = eepromread(EEPROM_STATION_MODE);
  if (c == MODE_INIT) stationMode = MODE_INIT;
  else if (c == MODE_START_KP) stationMode = MODE_START_KP;
  else if (c == MODE_FINISH_KP) stationMode = MODE_FINISH_KP;
  else
  {
#ifdef DEBUG
    Serial.print(F("!!! StationMode"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_MODE);
  }

  //читаем коэфф. пересчета напряжения
  union Convert
  {
    float number;
    uint8_t byte[4];
  } p;
  uint8_t flag = 0;
  for (uint8_t i = 0; i < 4; i++)
  {
    c = eepromread(EEPROM_VOLTAGE_KOEFF + i * 3);
    p.byte[i] = c;
    if (c == 0xff || c == -1) flag++;
  }
  if (flag < 4) voltageCoeff = p.number;
  else
  {
#ifdef DEBUG
    Serial.print(F("!!! VoltageKoeff"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_VKOEFF);
  }

  //читаем коэфф. усиления
  c = eepromread(EEPROM_GAIN);
  if (c != 255 && c != -1) gainCoeff = c;
  else
  {
#ifdef DEBUG
    Serial.print(F("!!! AntennaGain"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_GAIN); //STARTUP: incorrect gain in EEPROM
  }

  //читаем тип чипа
  c = eepromread(EEPROM_CHIP_TYPE);
  if (c != 255 && c != -1) selectChipType(chipType);
  else
  {
#ifdef DEBUG
    Serial.print(F("!!! ChipType"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_CHIP_TYPE);
  }

  const uint32_t flashSize = SPIflash.getCapacity();

  //читаем размер блока команды
  uint8_t n[2];
  flag = 0;
  for (uint8_t i = 0; i < 2; i++)
  {
    c = eepromread(EEPROM_TEAM_BLOCK_SIZE + i * 3);
    if (c == 0xff || c == -1) flag++;
    n[i] = c;
  }
  if (flag < 2) TEAM_FLASH_SIZE = n[0] * 256 + n[1];
  else
  {
#ifdef DEBUG
    Serial.print(F("!!! TeamSize"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_TEAM_SIZE);
  }

  //читаем размер стираемого блока
  flag = 0;
  for (uint8_t i = 0; i < 2; i++)
  {
    c = eepromread(EEPROM_FLASH_BLOCK_SIZE + i * 3);
    if (c == 0xff || c == -1) flag++;
    n[i] = c;
  }
  if (flag < 2) FLASH_BLOCK_SIZE = n[0] * 256 + n[1];
  else
  {
#ifdef DEBUG
    Serial.print(F("!!! EraseSize"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_ERASE_SIZE);
  }

  //читаем минимальное напряжение батареи
  flag = 0;
  for (uint8_t i = 0; i < 4; i++)
  {
    c = eepromread(EEPROM_BATTERY_LIMIT + i * 3);
    p.byte[i] = c;
    if (c == 0xff || c == -1) flag++;
  }
  if (flag < 4) batteryLimit = p.number;
  else
  {
#ifdef DEBUG
    Serial.print(F("!!! batteryLimit"));
#endif
    errorBeepMs(4, 200);
    addLastError(STARTUP_BATTERY_LIMIT);
  }

  maxTeamNumber = (flashSize - FLASH_BLOCK_SIZE) / TEAM_FLASH_SIZE - 1;

  totalChipsChecked = refreshChipCounter();

  batteryLevel = analogRead(BATTERY_PIN);

  beep(1, 800);
}

void loop()
{
  /*
   * В цикле сначала считывааем данные из порта, если получили полный пакет,
   * то ищем соответсвующую функцию и обабатываем её
   * Если режим станци не нулевой, то станция работает также как кп - автоматически делает отметки на чипы
   */

   // check receive timeout
  if (receivingData && millis() - receiveStartTime > receiveTimeOut)
  {
#ifdef DEBUG
    //Serial.println(F("!!!receive timeout"));
#endif
    uartBufferPosition = 0;
    uartReady = false;
    receivingData = false;
    //errorBeepMs(1, 50);
    addLastError(UART_TIMEOUT);
  }

  // check UART for data
  if (Serial.available())
  {
    uartReady = readUart();
  }

  //обработать пришедшую команду
  if (uartReady)
  {
    uartReady = false;
    executeCommand();
  }

  //если режим КП то отметить чип автоматом
  if (stationMode != MODE_INIT)
  {
    processRfidCard();
  }

  batteryLevel = (batteryLevel + getBatteryLevel()) / 2;
  if (float(float(batteryLevel) * voltageCoeff) <= batteryLimit)
  {
    if (batteryAlarmCount > 100)
    {
      digitalWrite(ERROR_LED_PIN, HIGH);
      tone(BUZZER_PIN, 50, 50);
      delay(50);
      digitalWrite(ERROR_LED_PIN, LOW);
    }
    else batteryAlarmCount++;
  }
  else batteryAlarmCount = 0;
}

// Обработка поднесенного чипа
void processRfidCard()
{
  if (stationNumber == 0 || stationNumber == 0xff) return;
  DS3231_get(&systemTime);
  uint32_t checkTime = systemTime.unixtime;

  // включаем SPI ищем чип вблизи. Если не находим выходим из функции чтения чипов
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  mfrc522.PCD_SetAntennaGain(gainCoeff);

  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    SPI.end();
    lastTeamFlag = 0;
    return;
  }
#ifdef DEBUG
  Serial.println(F("!!!chip found"));
#endif

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    SPI.end();
#ifdef DEBUG
    Serial.println(F("!!!fail to select chip"));
#endif
    return;
  }

  // читаем блок информации
  if (!ntagRead4pages(PAGE_CHIP_SYS))
  {
    SPI.end();
#ifdef DEBUG
    Serial.println(F("!!!fail to read chip"));
#endif
    //errorBeep(1);
    addLastError(PROCESS_READ_CHIP);
    return;
  }

  //неправильный тип чипа
  if (ntag_page[2] != chipType)
  {
    SPI.end();
#ifdef DEBUG
    Serial.println(F("!!!incorrect hw chip type"));
#endif
    errorBeep(1);
    addLastError(PROCESS_HW_CHIP_TYPE);
    return;
  }

  /*
  Фильтруем
  1 - неправильный тип чипа
  2 - чип от другой прошивки
  3 - чип более недельной давности инициализации
  4 - чипы с командой №0 или >maxTeamNumber
  5 - чип, который совпадает с уже отмеченным (в lastTeams[])
  */

  // неправильный тип чипа
  if (ntag_page[6] != NTAG_MARK)
  {
    SPI.end();
#ifdef DEBUG
    Serial.println(F("!!!incorrect sw chip type"));
#endif
    errorBeep(2);
    addLastError(PROCESS_SW_CHIP_TYPE);
    return;
  }

  // чип от другой прошивки
  if (ntag_page[7] != FW_VERSION)
  {
    SPI.end();
#ifdef DEBUG
    Serial.println(F("!!!incorrect fw ver."));
#endif
    errorBeep(4);
    addLastError(PROCESS_FW_VERSION);
    return;
  }

  // Не слишком ли старый чип? Недельной давности и более
  uint32_t initTime = ntag_page[8];
  initTime = initTime << 8;
  initTime += ntag_page[9];
  initTime = initTime << 8;
  initTime += ntag_page[10];
  initTime = initTime << 8;
  initTime += ntag_page[11];
  if ((systemTime.unixtime - initTime) > maxTimeInit)
  {
    SPI.end();
#ifdef DEBUG
    Serial.println(F("!!!outdated chip"));
#endif
    errorBeep(4);
    addLastError(PROCESS_INIT_TIME); //CARD PROCESSING: chip init time is due
    return;
  }

  // Если номер чипа =0 или >maxTeamNumber
  uint16_t teamNumber = (ntag_page[4] << 8) + ntag_page[5];
  if (teamNumber < 1 || teamNumber > maxTeamNumber)
  {
    SPI.end();
#ifdef DEBUG
    Serial.print(F("!!!incorrect chip #: "));
    Serial.println(String(teamNumber));
#endif
    errorBeep(4);
    addLastError(PROCESS_CHIP_NUMBER);
    return;
  }

  // не надо ли обновить у чипа маску?
  // 0-1: номер команды
  // 2-5: время выдачи чипа
  // 6-7: маска участников
  if (newTeamMask[0] + newTeamMask[1] != 0
    && ntag_page[4] == newTeamMask[0]
    && ntag_page[5] == newTeamMask[1]
    && ntag_page[8] == newTeamMask[2]
    && ntag_page[9] == newTeamMask[3]
    && ntag_page[10] == newTeamMask[4]
    && ntag_page[11] == newTeamMask[5])
  {
#ifdef DEBUG
    Serial.print(F("!!!updating mask"));
#endif
    if (ntag_page[12] != newTeamMask[6] || ntag_page[13] != newTeamMask[7])
    {
      digitalWrite(INFO_LED_PIN, HIGH);
      uint8_t dataBlock[4] = { newTeamMask[6], newTeamMask[7], ntag_page[14], ntag_page[15] };
      if (!ntagWritePage(dataBlock, PAGE_TEAM_MASK))
      {
        SPI.end();
#ifdef DEBUG
        Serial.print(F("!!!failed to write chip"));
#endif
        digitalWrite(INFO_LED_PIN, LOW);
        errorBeep(1);
        addLastError(PROCESS_WRITE_CHIP); //CARD PROCESSING: error writing to chip
        return;
      }
    }
    SPI.end();
    clearNewMask();
    lastTeamFlag = teamNumber;
    digitalWrite(INFO_LED_PIN, LOW);
#ifdef DEBUG
    Serial.print(F("!!!mask updated"));
#endif
    return;
  }

  uint16_t mask = (ntag_page[12] << 8) + ntag_page[13];

  // Если это повторная отметка
  if (teamNumber == lastTeamFlag)
  {
#ifdef DEBUG
    Serial.print(F("!!!same chip attached"));
#endif
    SPI.end();
    return;
  }

  bool already_checked = false;
  // сравнить с буфером последних команд
  if (stationMode == MODE_START_KP)
  {
    for (uint8_t i = 0; i < lastTeamsLength * 2; i = i + 2)
    {
      if (lastTeams[i] == ntag_page[0] && lastTeams[i + 1] == ntag_page[1])
      {
        already_checked = true;
#ifdef DEBUG
        Serial.println(F("!!!chip already checked"));
#endif
        break;
      }
    }
  }

  // Есть ли чип на флэше
  if (!already_checked && SPIflash.readByte(uint32_t(uint32_t(teamNumber) * uint32_t(TEAM_FLASH_SIZE))) != 255)
  {
    already_checked = true;
#ifdef DEBUG
    Serial.println(F("!!!chip already checked"));
#endif
  }

  // если известный чип и стартовый КП
  if (already_checked && stationMode == MODE_START_KP)
  {
#ifdef DEBUG
    Serial.println(F("!!!Can't read chip"));
#endif
    SPI.end();
    //digitalWrite(INFO_LED_PIN, LOW);
    errorBeep(1);
    addLastError(PROCESS_ALREADY_CHECKED);
    lastTeamFlag = teamNumber;
    return;
  }

#ifdef DEBUG
  Serial.println(F("!!!searching free page"));
#endif

  // ищем свободную страницу на чипе
  int newPage = findNewPage();

  // ошибка чтения чипа
  if (newPage == 0)
  {
    SPI.end();
    //digitalWrite(INFO_LED_PIN, LOW);
    errorBeep(1);
    addLastError(PROCESS_READ_CHIP);
#ifdef DEBUG
    Serial.println(F("!!!Can't read chip"));
#endif
    return;
  }

  // больше/меньше нормы... Наверное, переполнен???
  if (newPage != -1 && (newPage < PAGE_DATA_START || newPage >= TAG_MAX_PAGE))
  {
    SPI.end();
    //digitalWrite(INFO_LED_PIN, LOW);
    errorBeep(4);
    addLastError(PROCESS_FIND_FREE_PAGE);
#ifdef DEBUG
    Serial.print(F("!!!chip page# incorrect: "));
    Serial.println(String(newPage));
#endif
    return;
  }

  // chip was checked by another station with the same number
  if (newPage == -1)
  {
#ifdef DEBUG
    Serial.print(F("!!!chip marked by another station"));
#endif
    lastTeamFlag = teamNumber;
    //digitalWrite(INFO_LED_PIN, LOW);
    return;
  }

#ifdef DEBUG
  Serial.println(F("!!!writing to chip"));
#endif
  // Пишем на чип отметку
  digitalWrite(INFO_LED_PIN, HIGH);
  if (!writeCheckPointToCard(newPage, checkTime))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    errorBeep(1);
    addLastError(PROCESS_WRITE_CHIP); //CARD PROCESSING: error writing chip
#ifdef DEBUG
    Serial.print(F("!!!failed to write chip"));
#endif
    return;
  }
  // Пишем дамп чипа во флэш
  if (!writeDumpToFlash(teamNumber, checkTime, initTime, mask))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    errorBeep(2);
    addLastError(PROCESS_SAVE_DUMP); //CARD PROCESSING: error saving dump
#ifdef DEBUG
    Serial.print(F("!!!failed to write dump"));
#endif
    return;
  }
  SPI.end();
  digitalWrite(INFO_LED_PIN, LOW);
  beep(1, 200);

  // добавляем в буфер последних команд
  addLastTeam(teamNumber, already_checked);
  lastTimeChecked = checkTime;
  lastTeamFlag = teamNumber;

#ifdef AUTOREPORT
  //autoreport new command
  if (readTeamFromFlash(teamNumber))
  {
    init_package(REPLY_GET_TEAM_RECORD);
    // 0: код ошибки
    if (!addData(OK)) return;

    // 1-2: номер команды
    // 3-6 : время инициализации
    // 7-8: маска команды
    // 9-12 : время последней отметки на станции
    // 13: счетчик сохраненных страниц
    for (uint8_t i = 0; i < 13; i++)
    {
      if (!addData(ntag_page[i])) return;
    }
    sendData();
  }
  else addLastError(PROCESS_SEND_AUTOREPORT); //CARD PROCESSING: error sending autoreport
#endif

#ifdef DEBUG
  Serial.print(F("!!!New record #"));
  Serial.println(String(teamNumber));
#endif
}

// обработка входящих команд
bool readUart()
{
  while (Serial.available())
  {
    int c = Serial.read();

    if (c == -1) // can't read stream
    {
#ifdef DEBUG
      Serial.println(F("!!! UARTread error"));
#endif
      uartBufferPosition = 0;
      receivingData = false;
      return false;
    }

    // 0 byte = FE
    if (uartBufferPosition == 0 && c == 0xfe)
    {
#ifdef DEBUG
      Serial.print(F("!!!byte0="));
      if (c < 0x10) Serial.print(F("0"));
      Serial.println(String(uint8_t(c), HEX));
#endif
      receivingData = true;
      uartBuffer[uartBufferPosition] = uint8_t(c);
      uartBufferPosition++;
      // refresh timeout
      receiveStartTime = millis();
    }
    // 1st byte = FE
    else if (uartBufferPosition == 1 && c == 0xfe)
    {
#ifdef DEBUG
      Serial.print(F("!!!byte1"));
      if (c < 0x10) Serial.print(F("0"));
      Serial.println(String(uint8_t(c), HEX));
#endif
      uartBuffer[uartBufferPosition] = uint8_t(c);
      uartBufferPosition++;
    }
    // 2nd byte = command, length and data
    else if (uartBufferPosition >= PACKET_ID)
    {
      uartBuffer[uartBufferPosition] = uint8_t(c);
#ifdef DEBUG
      Serial.print(F("!!!byte"));
      Serial.print(String(uartBufferPosition));
      Serial.print(F("="));
      if (c < 0x10) Serial.print(F("0"));
      Serial.println(String(uint8_t(c), HEX));
#endif
      // incorrect length
      if (uartBufferPosition == LENGTH_BYTE && uartBuffer[LENGTH_BYTE] > (254 - DATA_START_BYTE))
      {
#ifdef DEBUG
        Serial.println(F("!!!incorrect length"));
#endif
        uartBufferPosition = 0;
        receivingData = false;
        errorBeepMs(3, 50);
        addLastError(UART_PACKET_LENGTH);
        return false;
      }

      // packet is received
      if (uartBufferPosition > LENGTH_BYTE&& uartBufferPosition >= DATA_START_BYTE + uartBuffer[LENGTH_BYTE])
      {
        // crc matching
#ifdef DEBUG
        Serial.print(F("!!!received packet expected CRC="));
        Serial.println(String(crcCalc(uartBuffer, STATION_NUMBER_BYTE, uartBufferPosition - 1), HEX));
#endif
        if (uartBuffer[uartBufferPosition] == crcCalc(uartBuffer, PACKET_ID, uartBufferPosition - 1))
        {
#ifdef DEBUG
          Serial.print(F("!!!Command received:"));
          for (uint8_t i = 0; i <= uartBufferPosition; i++)
          {
            Serial.print(F(" "));
            if (uartBuffer[i] < 0x10) Serial.print(F("0"));
            Serial.print(String(uartBuffer[i], HEX));
          }
          Serial.println();
#endif
          uartBufferPosition = 0;
          receivingData = false;
          return true;
        }
        else // CRC not correct
        {
#ifdef DEBUG
          Serial.println(F("!!!incorrect crc"));
#endif
          uartBufferPosition = 0;
          receivingData = false;
          errorBeepMs(3, 50);
          addLastError(UART_CRC);
          return false;
        }
      }
      uartBufferPosition++;
    }
    else
    {
#ifdef DEBUG
      Serial.println(F("!!!unexpected byte"));
#endif
      receivingData = false;
      uartBufferPosition = 0;
      addLastError(UART_UNEXPECTED_BYTE);
    }
  }
  return false;
}

// Commands processing

// поиск функции
void executeCommand()
{
#ifdef DEBUG
  Serial.print(F("!!!Command: "));
  Serial.println(String(uartBuffer[COMMAND_BYTE], HEX));
#endif
  bool errorLengthFlag = false;
  // Если номер станции не совпадает с присланным в пакете, то отказ
  if (uartBuffer[COMMAND_BYTE] != COMMAND_GET_STATUS && uartBuffer[COMMAND_BYTE] != COMMAND_GET_CONFIG && stationNumber != uartBuffer[STATION_NUMBER_BYTE])
  {
    uartBufferPosition = 0;
    sendError(WRONG_STATION, uartBuffer[COMMAND_BYTE]);
    return;
  }
  switch (uartBuffer[COMMAND_BYTE])
  {
  case COMMAND_SET_MODE:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_MODE) setMode();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_TIME:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_TIME) setTime();
    else errorLengthFlag = true;
    break;
  case COMMAND_RESET_STATION:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_RESET_STATION) resetStation();
    else errorLengthFlag = true;
    break;
  case COMMAND_GET_STATUS:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_STATUS) getStatus();
    else errorLengthFlag = true;
    break;
  case COMMAND_INIT_CHIP:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_INIT_CHIP) initChip();
    else errorLengthFlag = true;
    break;
  case COMMAND_GET_LAST_TEAMS:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_LAST_TEAMS) getLastTeams();
    else errorLengthFlag = true;
    break;
  case COMMAND_GET_TEAM_RECORD:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_TEAM_RECORD) getTeamRecord();
    else errorLengthFlag = true;
    break;
  case COMMAND_READ_CARD_PAGE:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_READ_CARD_PAGE) readCardPages();
    else errorLengthFlag = true;
    break;
  case COMMAND_UPDATE_TEAM_MASK:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_UPDATE_TEAM_MASK) updateTeamMask();
    else errorLengthFlag = true;
    break;
  case COMMAND_WRITE_CARD_PAGE:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_WRITE_CARD_PAGE) writeCardPage();
    else errorLengthFlag = true;
    break;
  case COMMAND_READ_FLASH:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_READ_FLASH) readFlash();
    else errorLengthFlag = true;
    break;
  case COMMAND_WRITE_FLASH:
    if (uartBuffer[LENGTH_BYTE] >= DATA_LENGTH_WRITE_FLASH) writeFlash();
    else errorLengthFlag = true;
    break;
  case COMMAND_ERASE_FLASH_SECTOR:
    if (uartBuffer[LENGTH_BYTE] >= DATA_LENGTH_ERASE_FLASH_SECTOR) eraseTeamFlash();
    else errorLengthFlag = true;
    break;
  case COMMAND_GET_CONFIG:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_CONFIG) getConfig();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_V_KOEFF:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_V_KOEFF) setVCoeff();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_GAIN:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_GAIN) setGain();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_CHIP_TYPE:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_CHIP_TYPE) setChipType();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_TEAM_FLASH_SIZE:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_TEAM_FLASH_SIZE) setTeamFlashSize();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_FLASH_BLOCK_SIZE:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_FLASH_BLOCK_SIZE) setFlashBlockSize();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_BT_NAME:
    if (uartBuffer[LENGTH_BYTE] >= DATA_LENGTH_SET_BT_NAME) setNewBtName();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_BT_PIN:
    if (uartBuffer[LENGTH_BYTE] >= DATA_LENGTH_SET_BT_PIN) setNewBtPinCode();
    else errorLengthFlag = true;
    break;
  case COMMAND_SET_BATTERY_LIMIT:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SET_BATTERY_LIMIT) setBatteryLimit();
    else errorLengthFlag = true;
    break;
  case COMMAND_SCAN_TEAMS:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_SCAN_TEAMS) scanTeams();
    else errorLengthFlag = true;
    break;
  case COMMAND_SEND_BT_COMMAND:
    if (uartBuffer[LENGTH_BYTE] >= DATA_LENGTH_SEND_BT_COMMAND) sendBtCommand();
    else errorLengthFlag = true;
    break;
  case COMMAND_GET_LAST_ERRORS:
    if (uartBuffer[LENGTH_BYTE] == DATA_LENGTH_GET_LAST_ERRORS) getLastErrors();
    else errorLengthFlag = true;
    break;

  default:
    sendError(WRONG_COMMAND, uartBuffer[COMMAND_BYTE]);
    break;
  }

  uartBufferPosition = 0;
  if (errorLengthFlag)
  {
#ifdef DEBUG
    Serial.println(F("!!!Incorrect data length"));
#endif
    sendError(WRONG_PACKET_LENGTH, uartBuffer[COMMAND_BYTE]);
  }
}

// установка режима
void setMode()
{
  if (stationNumber == 0 || stationNumber == 0xff)
  {
    sendError(WRONG_STATION, REPLY_SET_MODE);
    return;
  }

  // 0: новый номер режима
  stationMode = uartBuffer[DATA_START_BYTE];
  if (!eepromwrite(EEPROM_STATION_MODE, stationMode))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_SET_MODE);
    return;
  }

  // формирование пакета данных.
  init_package(REPLY_SET_MODE);

  // 0: код ошибки
  if (!addData(OK)) return;
  sendData();
}

// обновление времени на станции
void setTime()
{
  systemTime.year = uartBuffer[DATA_START_BYTE] + 2000;
  systemTime.mon = uartBuffer[DATA_START_BYTE + 1];
  systemTime.mday = uartBuffer[DATA_START_BYTE + 2];
  systemTime.hour = uartBuffer[DATA_START_BYTE + 3];
  systemTime.min = uartBuffer[DATA_START_BYTE + 4];
  systemTime.sec = uartBuffer[DATA_START_BYTE + 5];

#ifdef DEBUG
  // Serial.print(F("!!!Time: "));
  // for (uint8_t i = 0; i < 6; i++) Serial.println(String(uartBuffer[DATA_START_BYTE + i]) + " ");
#endif

  // 0-3: дата и время в unixtime

  delay(1);
  DS3231_set(systemTime); // correct time

  DS3231_get(&systemTime);
  uint32_t tmpTime = systemTime.unixtime;

  init_package(REPLY_SET_TIME);

  // 0: код ошибки
  // 1-4: текущее время
  bool flag = true;
  flag &= addData(OK);
  flag &= addData((tmpTime & 0xFF000000) >> 24);
  flag &= addData((tmpTime & 0x00FF0000) >> 16);
  flag &= addData((tmpTime & 0x0000FF00) >> 8);
  flag &= addData(tmpTime & 0x000000FF);
  if (!flag) return;
  sendData();
}

// сброс настроек станции
void resetStation()
{
  // 0-1: кол-во отмеченных чипов (для сверки)
  // 2-5: время последней отметки(для сверки)
  // 6 : новый номер станции
  // проверить количество отметок
  uint16_t checkCardNumber = uartBuffer[DATA_START_BYTE];
  checkCardNumber <<= 8;
  checkCardNumber += uartBuffer[DATA_START_BYTE + 1];

  // проверить кол-во отметок (для безопасности)
  if (checkCardNumber != totalChipsChecked)
  {
    sendError(WRONG_DATA, REPLY_RESET_STATION);
    return;
  }

  // проверить время последней отметки
  uint32_t checkLastTime = uartBuffer[DATA_START_BYTE + 2];
  checkLastTime <<= 8;
  checkLastTime += uartBuffer[DATA_START_BYTE + 3];
  checkLastTime <<= 8;
  checkLastTime += uartBuffer[DATA_START_BYTE + 4];
  checkLastTime <<= 8;
  checkLastTime += uartBuffer[DATA_START_BYTE + 5];
  if (checkLastTime != lastTimeChecked)
  {
    sendError(WRONG_DATA, REPLY_RESET_STATION);
    return;
  }

  if (uartBuffer[DATA_START_BYTE + 6] == 0xff)
  {
    sendError(WRONG_STATION, REPLY_RESET_STATION);
    return;
  }
  stationNumber = uartBuffer[DATA_START_BYTE + 6];
  if (!eepromwrite(EEPROM_STATION_NUMBER, stationNumber))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_RESET_STATION);
    return;
  }

  stationMode = 0;
  if (!eepromwrite(EEPROM_STATION_MODE, stationMode))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_RESET_STATION);
    return;
  }

  lastTimeChecked = 0;
  totalChipsChecked = 0;

  if (!SPIflash.eraseChip())
  {
    sendError(FLASH_WRITE_ERROR, REPLY_RESET_STATION);
    return;
  }

  init_package(REPLY_RESET_STATION);

  // 0: код ошибки
  if (!addData(OK)) return;
  sendData();
  delay(100);
  resetFunc();
}

// выдает статус: время на станции, номер станции, номер режима, число отметок, время последней страницы
void getStatus()
{
  DS3231_get(&systemTime);
  uint32_t tmpTime = systemTime.unixtime;

  // 0: код ошибки
  init_package(REPLY_GET_STATUS);

  bool flag = true;
  flag &= addData(OK);

  // 1 - 4: текущее время
  flag &= addData((tmpTime & 0xFF000000) >> 24);
  flag &= addData((tmpTime & 0x00FF0000) >> 16);
  flag &= addData((tmpTime & 0x0000FF00) >> 8);
  flag &= addData(tmpTime & 0x000000FF);

  // 5 - 6 : количество отметок на станции
  flag &= addData((totalChipsChecked & 0xFF00) >> 8);
  flag &= addData(totalChipsChecked & 0x00FF);

  // 7 - 10 : время последней отметки на станции
  flag &= addData((lastTimeChecked & 0xFF000000) >> 24);
  flag &= addData((lastTimeChecked & 0x00FF0000) >> 16);
  flag &= addData((lastTimeChecked & 0x0000FF00) >> 8);
  flag &= addData(lastTimeChecked & 0x000000FF);

  // 11 - 12 : напряжение батареи в условных единицах[0..1023] ~[0..1.1В]
  flag &= addData((batteryLevel & 0xFF00) >> 8);
  flag &= addData(batteryLevel & 0x00FF);

  // 13 - 14 : температура чипа DS3231(чуть выше окружающей среды)
  int temperature = int(DS3231_get_treg());
  flag &= addData((temperature & 0xFF00) >> 8);
  flag &= addData(temperature & 0x00FF);

  if (!flag) return;
  sendData();
}

// инициализация чипа
void initChip()
{
  digitalWrite(INFO_LED_PIN, HIGH);
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  mfrc522.PCD_SetAntennaGain(gainCoeff);

  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(NO_CHIP, REPLY_INIT_CHIP);
    return;
  }
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_READ_ERROR, REPLY_INIT_CHIP);
    return;
  }

  // читаем блок информации
  if (!ntagRead4pages(PAGE_CHIP_SYS))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_READ_ERROR, REPLY_INIT_CHIP);
    return;
  }

  // Фильтруем неправильный тип чипа
  if (ntag_page[2] != chipType)
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(WRONG_CHIP_TYPE, REPLY_INIT_CHIP);
    return;
  }

  // инициализация сработает только если время инициализации записанное уже на чипе превышает неделю до текущего времени
  uint32_t initTime = ntag_page[8];
  initTime <<= 8;
  initTime += ntag_page[9];
  initTime <<= 8;
  initTime += ntag_page[10];
  initTime <<= 8;
  initTime += ntag_page[11];
  DS3231_get(&systemTime);
  if ((systemTime.unixtime - initTime) < maxTimeInit)
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(LOW_INIT_TIME, REPLY_INIT_CHIP);
    return;
  }

  // заполняем чип 0x00
  uint8_t dataBlock[4] = { 0,0,0,0 };
  for (uint8_t page = PAGE_CHIP_NUM; page < TAG_MAX_PAGE; page++)
  {
    if (!ntagWritePage(dataBlock, page))
    {
      SPI.end();
      digitalWrite(INFO_LED_PIN, LOW);
      sendError(RFID_WRITE_ERROR, REPLY_INIT_CHIP);
      return;
    }
  }

  // 0-1: номер команды
  // 2-3 : маска участников

  // пишем данные на чип
  // номер команды, тип чипа, версия прошивки станции
  dataBlock[0] = uartBuffer[DATA_START_BYTE];
  dataBlock[1] = uartBuffer[DATA_START_BYTE + 1];
  dataBlock[2] = NTAG_MARK;
  dataBlock[3] = FW_VERSION;
  if (!ntagWritePage(dataBlock, PAGE_CHIP_NUM))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_WRITE_ERROR, REPLY_INIT_CHIP);
    return;
  }

  // пишем на чип текущее время
  uint32_t tmpTime = systemTime.unixtime;
  dataBlock[0] = (tmpTime & 0xFF000000) >> 24;
  dataBlock[1] = (tmpTime & 0x00FF0000) >> 16;
  dataBlock[2] = (tmpTime & 0x0000FF00) >> 8;
  dataBlock[3] = tmpTime & 0x000000FF;
  if (!ntagWritePage(dataBlock, PAGE_INIT_TIME))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_WRITE_ERROR, REPLY_INIT_CHIP);
    return;
  }

  // маска участников
  dataBlock[0] = uartBuffer[DATA_START_BYTE + 2];
  dataBlock[1] = uartBuffer[DATA_START_BYTE + 3];
  dataBlock[2] = 0;
  dataBlock[3] = 0;
  if (!ntagWritePage(dataBlock, PAGE_TEAM_MASK))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_WRITE_ERROR, REPLY_INIT_CHIP);
    return;
  }

  // получаем UID чипа
  if (!ntagRead4pages(PAGE_UID))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_READ_ERROR, REPLY_INIT_CHIP);
    return;
  }
  SPI.end();
  digitalWrite(INFO_LED_PIN, LOW);

  init_package(REPLY_INIT_CHIP);
  if (!addData(OK)) return;

  // добавляем в ответ время инициализации
  bool flag = true;
  flag &= addData((tmpTime & 0xFF000000) >> 24);
  flag &= addData((tmpTime & 0x00FF0000) >> 16);
  flag &= addData((tmpTime & 0x0000FF00) >> 8);
  flag &= addData(tmpTime & 0x000000FF);
  if (!flag) return;

  // добавляем в ответ UID
  for (uint8_t i = 0; i <= 7; i++)
  {
    if (!addData(ntag_page[i])) return;
  }
  sendData();
}

// получить последнюю отметившуюся команду
void getLastTeams()
{
  init_package(REPLY_GET_LAST_TEAMS);

  // 0: код ошибки
  if (!addData(OK)) return;

  // номера последних команд
  for (uint8_t i = 0; i < lastTeamsLength * 2; i++)
  {
    //stop if command is empty
    // if (lastTeams[i] + lastTeams[i + 1] == 0) break;
    if (!addData(lastTeams[i])) return;
    i++;
    if (!addData(lastTeams[i])) return;
  }
  sendData();
  for (uint8_t i = 0; i < lastTeamsLength * 2; i++) lastTeams[i] = 0;
}

// получаем запись о команде из флэша
void getTeamRecord()
{
  // 0-1: какую запись
  uint16_t recordNumber = uartBuffer[DATA_START_BYTE];
  recordNumber <<= 8;
  recordNumber += uartBuffer[DATA_START_BYTE + 1];

  if (recordNumber < 1 || recordNumber > maxTeamNumber)
  {
    sendError(WRONG_TEAM, REPLY_GET_TEAM_RECORD);
    return;
  }

  init_package(REPLY_GET_TEAM_RECORD);
  // 0: код ошибки
  if (!addData(OK)) return;

  // если ячейка лога пуста
  if (!readTeamFromFlash(recordNumber))
  {
    sendError(NO_DATA, REPLY_GET_TEAM_RECORD);
    return;
  }

  // 1-2: номер команды
  // 3-6 : время инициализации
  // 7-8: маска команды
  // 9-12 : время последней отметки на станции
  // 13: счетчик сохраненных страниц
  for (uint8_t i = 0; i < 13; i++)
  {
    if (!addData(ntag_page[i])) return;
  }
  sendData();
}

// читаем страницы с чипа
void readCardPages()
{
  digitalWrite(INFO_LED_PIN, HIGH);
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  mfrc522.PCD_SetAntennaGain(gainCoeff);

  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(NO_CHIP, REPLY_READ_CARD_PAGE);
    return;
  }
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_READ_ERROR, REPLY_READ_CARD_PAGE);
    return;
  }

  uint8_t pageFrom = uartBuffer[DATA_START_BYTE];
  uint8_t pageTo = uartBuffer[DATA_START_BYTE + 1];

  init_package(REPLY_READ_CARD_PAGE);

  // 0: код ошибки
  // 1-8: UID чипа
  // 9-12: данные из страницы чипа(4 байта)
  if (!addData(OK))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    return;
  }

  // читаем UID
  if (!ntagRead4pages(PAGE_UID))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_READ_ERROR, REPLY_READ_CARD_PAGE);
    return;
  }

  // пишем UID в буфер ответа
  for (uint8_t i = 0; i <= 7; i++)
  {
    if (!addData(ntag_page[i]))
    {
      SPI.end();
      digitalWrite(INFO_LED_PIN, LOW);
      return;
    }
  }

  // начальная страница
  if (!addData(pageFrom))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    return;
  }
  while (pageFrom <= pageTo)
  {
    if (!ntagRead4pages(pageFrom))
    {
      SPI.end();
      digitalWrite(INFO_LED_PIN, LOW);
      sendError(RFID_READ_ERROR, REPLY_READ_CARD_PAGE);
      return;
    }
    uint8_t n = (pageTo - pageFrom + 1);
    if (n > 4) n = 4;
    for (uint8_t i = 0; i < n; i++)
    {
      for (uint8_t j = 0; j < 4; j++)
      {
        if (!addData(ntag_page[i * 4 + j]))
        {
          SPI.end();
          digitalWrite(INFO_LED_PIN, LOW);
          return;
        }
      }
      pageFrom++;
    }
  }
  SPI.end();
  digitalWrite(INFO_LED_PIN, LOW);

  sendData();
}

// Обновить маску команды в буфере
void updateTeamMask()
{
  // 0-1: номер команды
  // 2-5: время выдачи чипа
  // 6-7: маска участников
  saveNewMask();

  init_package(REPLY_UPDATE_TEAM_MASK);
  // 0: код ошибки
  if (!addData(OK)) return;
  sendData();

  if (stationMode == MODE_INIT)
  {
    // включаем SPI ищем чип вблизи. Если не находим выходим из функции чтения чипов
    SPI.begin();      // Init SPI bus
    mfrc522.PCD_Init();    // Init MFRC522
    mfrc522.PCD_SetAntennaGain(gainCoeff);

    // Look for new cards
    if (!mfrc522.PICC_IsNewCardPresent())
    {
      SPI.end();
      lastTeamFlag = 0;
      sendError(NO_CHIP, REPLY_UPDATE_TEAM_MASK);
      return;
    }
#ifdef DEBUG
    Serial.println(F("!!!chip found"));
#endif

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial())
    {
      SPI.end();
      sendError(RFID_READ_ERROR, REPLY_UPDATE_TEAM_MASK);
#ifdef DEBUG
      Serial.println(F("!!!fail to select card"));
#endif
      return;
    }

    // читаем блок информации
    if (!ntagRead4pages(PAGE_UID))
    {
      SPI.end();
      sendError(RFID_READ_ERROR, REPLY_UPDATE_TEAM_MASK);
      return;
    }

    //неправильный тип чипа
    if (ntag_page[14] != chipType)
    {
      SPI.end();
#ifdef DEBUG
      Serial.println(F("!!!incorrect chip"));
#endif
      sendError(WRONG_CHIP_TYPE, REPLY_UPDATE_TEAM_MASK);
      return;
    }

    /*
    Фильтруем
    1 - неправильный тип чипа
    2 - чип от другой прошивки
    3 - чип более недельной давности инициализации
    4 - чипы с командой №0 или >maxTeamNumber
    5 - чип, который совпадает с уже отмеченным (в lastTeams[])
    */

    // читаем блок информации
    if (!ntagRead4pages(PAGE_CHIP_NUM))
    {
      SPI.end();
      sendError(RFID_READ_ERROR, REPLY_UPDATE_TEAM_MASK);
      return;
    }

    // неправильный тип чипа
    if (ntag_page[2] != NTAG_MARK)
    {
      SPI.end();
#ifdef DEBUG
      Serial.println(F("!!!incorrect chip"));
#endif
      sendError(WRONG_CHIP_TYPE, REPLY_UPDATE_TEAM_MASK);
      return;
    }

    // чип от другой прошивки
    if (ntag_page[3] != FW_VERSION)
    {
      SPI.end();
#ifdef DEBUG
      Serial.println(F("!!!incorrect fw"));
#endif
      sendError(WRONG_FW_VERSION, REPLY_UPDATE_TEAM_MASK);
      return;
    }

    // Не слишком ли старый чип? Недельной давности и более
    uint32_t timeInit = ntag_page[4];
    timeInit = timeInit << 8;
    timeInit += ntag_page[5];
    timeInit = timeInit << 8;
    timeInit += ntag_page[6];
    timeInit = timeInit << 8;
    timeInit += ntag_page[7];
    if ((systemTime.unixtime - timeInit) > maxTimeInit)
    {
      SPI.end();
#ifdef DEBUG
      Serial.println(F("!!!outdated chip"));
#endif
      sendError(LOW_INIT_TIME, REPLY_UPDATE_TEAM_MASK);
      return;
    }

    uint16_t chipNum = (ntag_page[0] << 8) + ntag_page[1];

    // Если номер чипа =0 или >maxTeamNumber

    if (chipNum < 1 || chipNum > maxTeamNumber)
    {
      SPI.end();
#ifdef DEBUG
      Serial.print(F("!!!incorrect chip #"));
      Serial.println(String(chipNum));
#endif
      sendError(WRONG_TEAM, REPLY_UPDATE_TEAM_MASK);
      return;
    }

    // не надо ли обновить у чипа маску?
    // 0-1: номер команды
    // 2-5: время выдачи чипа
    // 6-7: маска участников
    if (newTeamMask[0] + newTeamMask[1] != 0
      && ntag_page[0] == newTeamMask[0]
      && ntag_page[1] == newTeamMask[1]
      && ntag_page[4] == newTeamMask[2]
      && ntag_page[5] == newTeamMask[3]
      && ntag_page[6] == newTeamMask[4]
      && ntag_page[7] == newTeamMask[5])
    {
      if (ntag_page[8] != newTeamMask[6] || ntag_page[9] != newTeamMask[7])
      {
#ifdef DEBUG
        Serial.print(F("!!!updating mask"));
#endif
        digitalWrite(INFO_LED_PIN, HIGH);
        uint8_t dataBlock[4] = { newTeamMask[6], newTeamMask[7], ntag_page[10], ntag_page[11] };
        if (!ntagWritePage(dataBlock, PAGE_TEAM_MASK))
        {
          SPI.end();
          digitalWrite(INFO_LED_PIN, LOW);
          sendError(RFID_WRITE_ERROR, REPLY_UPDATE_TEAM_MASK);
          return;
        }
      }
      SPI.end();
      digitalWrite(INFO_LED_PIN, LOW);
      clearNewMask();
    }
  }
}

// пишем присланные с ББ 4 байта в указанную страницу
void writeCardPage()
{
  digitalWrite(INFO_LED_PIN, HIGH);
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  mfrc522.PCD_SetAntennaGain(gainCoeff);

  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(NO_CHIP, REPLY_WRITE_CARD_PAGE);
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial())
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_READ_ERROR, REPLY_WRITE_CARD_PAGE);
    return;
  }

  // 0-7: UID чипа
  // 8: номер страницы
  // 9-12: данные для записи (4 байта)

  // проверить UID
  if (!ntagRead4pages(PAGE_UID))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_READ_ERROR, REPLY_WRITE_CARD_PAGE);
    return;
  }
  bool flag = false;
  for (uint8_t i = 0; i <= 7; i++)
  {
    if (uartBuffer[DATA_START_BYTE + i] != 0xff && ntag_page[i] != uartBuffer[DATA_START_BYTE + i])
    {
      flag = true;
      break;
    }
  }
  if (flag)
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(WRONG_UID, REPLY_WRITE_CARD_PAGE);
    return;
  }

  // записать страницу
  uint8_t dataBlock[] =
  {
    uartBuffer[DATA_START_BYTE + 9],
    uartBuffer[DATA_START_BYTE + 10],
    uartBuffer[DATA_START_BYTE + 11],
    uartBuffer[DATA_START_BYTE + 12]
  };
  if (!ntagWritePage(dataBlock, uartBuffer[DATA_START_BYTE + 8]))
  {
    SPI.end();
    digitalWrite(INFO_LED_PIN, LOW);
    sendError(RFID_WRITE_ERROR, REPLY_WRITE_CARD_PAGE);
    return;
  }
  SPI.end();
  digitalWrite(INFO_LED_PIN, LOW);

  init_package(REPLY_WRITE_CARD_PAGE);

  // 0: код ошибки
  if (!addData(OK)) return;
  sendData();
}

// читаем флэш
void readFlash()
{
  // 0-3: адрес начала чтения
  // 4: размер блока
  uint32_t startAddress = uartBuffer[DATA_START_BYTE];
  startAddress <<= 8;
  startAddress += uartBuffer[DATA_START_BYTE + 1];
  startAddress <<= 8;
  startAddress += uartBuffer[DATA_START_BYTE + 2];
  startAddress <<= 8;
  startAddress += uartBuffer[DATA_START_BYTE + 3];

  uint16_t length = uartBuffer[DATA_START_BYTE + 4];
  if (length > (256 - 7 - DATA_LENGTH_READ_FLASH - 1)) length = 256 - 7 - DATA_LENGTH_READ_FLASH - 1;

#ifdef DEBUG
  Serial.print(F("!!!flash read="));
  Serial.print(String(startAddress));
  Serial.print(F("/"));
  Serial.println(String(length));
#endif

  init_package(REPLY_READ_FLASH);

  // 0: код ошибки
  // 0-3: адрес начала чтения
  // 4-n: данные из флэша
  if (!addData(OK)) return;
#ifdef DEBUG
  Serial.print(F("!!!OK "));
  Serial.println(String(uartBufferPosition));
#endif

  bool flag = true;
  flag &= addData((startAddress & 0xFF000000) >> 24);
  flag &= addData((startAddress & 0x00FF0000) >> 16);
  flag &= addData((startAddress & 0x0000FF00) >> 8);
  flag &= addData(startAddress & 0x000000FF);
  if (!flag) return;
#ifdef DEBUG
  //Serial.print(F("!!!address "));
  //Serial.println(String(uartBufferPosition));
#endif

  for (long i = startAddress; i < startAddress + uint32_t(length); i++)
  {
    uint8_t b = SPIflash.readByte(i);
    if (!addData(b)) return;

#ifdef DEBUG
    Serial.print(String(i));
    Serial.print(F("="));
    if (b < 0x10) Serial.print(F("0"));
    Serial.println(String(b, HEX));
#endif
  }

  sendData();
}

// пишем в флэш
void writeFlash()
{
  // 0-3: адрес начала записи
  // 1: кол-во записанных байт (для проверки)
  uint32_t startAddress = uartBuffer[DATA_START_BYTE];
  startAddress <<= 8;
  startAddress += uartBuffer[DATA_START_BYTE + 1];
  startAddress <<= 8;
  startAddress += uartBuffer[DATA_START_BYTE + 2];
  startAddress <<= 8;
  startAddress += uartBuffer[DATA_START_BYTE + 3];

  uint8_t length = uartBuffer[LENGTH_BYTE] - 4;

  init_package(REPLY_WRITE_FLASH);

  if (!SPIflash.writeByteArray(startAddress, &uartBuffer[DATA_START_BYTE + 4], length))
  {
    sendError(FLASH_WRITE_ERROR, REPLY_WRITE_FLASH);
    return;
  }

  // 0: код ошибки
  // 1: кол-во записанных байт (для проверки)
  if (!addData(OK)) return;
  if (!addData(length)) return;

  sendData();
}

// стираем команду с флэша (4096 байт)
void eraseTeamFlash()
{
  uint32_t teamNumber = uartBuffer[DATA_START_BYTE];
  teamNumber <<= 8;
  teamNumber += uartBuffer[DATA_START_BYTE + 1];
#ifdef DEBUG
  Serial.print(F("!!!erasing "));
  Serial.println(String(teamNumber));
#endif

  if (!eraseTeamFromFlash(teamNumber))
  {
    sendError(ERASE_ERROR, REPLY_ERASE_FLASH_SECTOR);
    return;
  }

  init_package(REPLY_ERASE_FLASH_SECTOR);
  // 0: код ошибки
  if (!addData(OK)) return;
  sendData();
}

// выдает конфигурацию станции
void getConfig()
{
  // 0: код ошибки
  // 1: версия прошивки
  // 2: номер режима
  // 3: тип чипов (емкость разная, а распознать их программно можно только по ошибкам чтения "дальних" страниц)
  // 4-7: емкость флэш - памяти
  // 8-11: размер сектора флэш - памяти
  // 12-15: коэффициент пересчета напряжения(float, 4 bytes) - просто умножаешь коэффициент на полученное в статусе число и будет температура
  // 16: коэффициент усиления антенны RFID
  // 17-18: размер блока хранения команды
  init_package(REPLY_GET_CONFIG);

  bool flag = true;
  flag &= addData(OK);
  flag &= addData(FW_VERSION);
  flag &= addData(stationMode);
  flag &= addData(NTAG_MARK);

  uint32_t n = SPIflash.getCapacity();
  flag &= addData((n & 0xFF000000) >> 24);
  flag &= addData((n & 0x00FF0000) >> 16);
  flag &= addData((n & 0x0000FF00) >> 8);
  flag &= addData(n & 0x000000FF);

  uint8_t v[4];
  floatToByte(v, voltageCoeff);
  flag &= addData(v[0]);
  flag &= addData(v[1]);
  flag &= addData(v[2]);
  flag &= addData(v[3]);

  flag &= addData(gainCoeff);

  flag &= addData(TEAM_FLASH_SIZE >> 8);
  flag &= addData(TEAM_FLASH_SIZE & 0x00FF);

  flag &= addData(FLASH_BLOCK_SIZE >> 8);
  flag &= addData(FLASH_BLOCK_SIZE & 0x00FF);

  floatToByte(v, batteryLimit);
  flag &= addData(v[0]);
  flag &= addData(v[1]);
  flag &= addData(v[2]);
  flag &= addData(v[3]);

  if (!flag) return;

  sendData();
}

// сохранить коэфф. пересчета ADC в напряжение для резисторного делителя 10кОм + 2.2кОм
void setVCoeff()
{
  // 0-3: коэфф.
  union Convert
  {
    float number;
    uint8_t byte[4];
  } p;

  for (uint8_t i = 0; i < 4; i++)
  {
    p.byte[i] = uartBuffer[DATA_START_BYTE + i];
    if (!eepromwrite(EEPROM_VOLTAGE_KOEFF + i * 3, uartBuffer[DATA_START_BYTE + i]))
    {
      sendError(EEPROM_WRITE_ERROR, REPLY_SET_V_KOEFF);
      return;
    }
  }
  voltageCoeff = p.number;

  init_package(REPLY_SET_V_KOEFF);
  // 0: код ошибки
  // 1...: данные из флэша
  if (!addData(OK)) return;

  sendData();
}

// сохранить коэфф. усиления для RFID
void setGain()
{
  // 0: коэфф.
  gainCoeff = uartBuffer[DATA_START_BYTE] & 0x70;
  if (!eepromwrite(EEPROM_GAIN, gainCoeff))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_SET_GAIN);
    return;
  }

  init_package(REPLY_SET_GAIN);
  // 0: код ошибки
  if (!addData(OK)) return;

  sendData();
}

// сохранить тип чипа
void setChipType()
{
  // 0: тип чипа
  chipType = uartBuffer[DATA_START_BYTE];

  if (!selectChipType(chipType))
  {
    sendError(WRONG_CHIP_TYPE, REPLY_SET_CHIP_TYPE);
    return;
  }
  if (!eepromwrite(EEPROM_CHIP_TYPE, chipType))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_SET_CHIP_TYPE);
    return;
  }

  init_package(REPLY_SET_CHIP_TYPE);
  // 0: код ошибки
  if (!addData(OK)) return;

  sendData();
}

// сохранить размер блока команды
void setTeamFlashSize()
{
  // 0-1: размер блока
  uint16_t n = uartBuffer[DATA_START_BYTE] * 256 + uartBuffer[DATA_START_BYTE + 1];

  if (n < 16)
  {
    sendError(WRONG_SIZE, REPLY_SET_TEAM_FLASH_SIZE);
    return;
  }
  TEAM_FLASH_SIZE = n;

  if (!eepromwrite(EEPROM_TEAM_BLOCK_SIZE, uartBuffer[DATA_START_BYTE]))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_SET_TEAM_FLASH_SIZE);
    return;
  }
  if (!eepromwrite(EEPROM_TEAM_BLOCK_SIZE + 3, uartBuffer[DATA_START_BYTE + 1]))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_SET_TEAM_FLASH_SIZE);
    return;
  }

  init_package(REPLY_SET_TEAM_FLASH_SIZE);
  // 0: код ошибки
  if (!addData(OK)) return;

  sendData();
}

// сохранить размер стираемого блока
void setFlashBlockSize()
{
  // 0-1: размер блока
  uint16_t n = uartBuffer[DATA_START_BYTE] * 256 + uartBuffer[DATA_START_BYTE + 1];

  if (n < 16)
  {
    sendError(WRONG_SIZE, REPLY_SET_FLASH_BLOCK_SIZE);
    return;
  }
  FLASH_BLOCK_SIZE = n;

  if (!eepromwrite(EEPROM_FLASH_BLOCK_SIZE, uartBuffer[DATA_START_BYTE]))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_SET_FLASH_BLOCK_SIZE);
    return;
  }
  if (!eepromwrite(EEPROM_FLASH_BLOCK_SIZE + 3, uartBuffer[DATA_START_BYTE + 1]))
  {
    sendError(EEPROM_WRITE_ERROR, REPLY_SET_FLASH_BLOCK_SIZE);
    return;
  }

  init_package(REPLY_SET_FLASH_BLOCK_SIZE);
  // 0: код ошибки
  if (!addData(OK)) return;

  sendData();
}

// поменять имя BT адаптера
void setNewBtName()
{
#ifdef DEBUG
  Serial.print(F("!!!Set new BT name"));
#endif

  if (uartBuffer[LENGTH_BYTE] < 1 || uartBuffer[LENGTH_BYTE]>32)
  {
    sendError(WRONG_DATA, REPLY_SET_BT_NAME);
    return;
  }
  String buf;
  buf.reserve(uartBuffer[LENGTH_BYTE]);
  for (uint16_t i = 0; i < uartBuffer[LENGTH_BYTE]; i++)
  {
    buf += String(char(uartBuffer[DATA_START_BYTE + i]));
  }

  if (!setBtName(buf))
  {
    sendError(BT_ERROR, REPLY_SET_BT_NAME);
    return;
  }
#ifdef DEBUG
  Serial.println();
#endif

  init_package(REPLY_SET_BT_NAME);
  // 0: код ошибки
  if (!addData(OK)) return;
  sendData();
}

// поменять пин-код BT адаптера
void setNewBtPinCode()
{
#ifdef DEBUG
  Serial.print(F("!!!Set new BT name"));
#endif
  if (uartBuffer[LENGTH_BYTE] < 1 || uartBuffer[LENGTH_BYTE]>16)
  {
    sendError(WRONG_DATA, REPLY_SET_BT_PIN);
    return;
  }
  String buf;
  buf.reserve(uartBuffer[LENGTH_BYTE]);
  for (uint16_t i = 0; i < uartBuffer[LENGTH_BYTE]; i++)
  {
    buf += String(char(uartBuffer[DATA_START_BYTE + i]));
  }

  if (!setBtPinCode(buf))
  {
    sendError(BT_ERROR, REPLY_SET_BT_PIN);
    return;
  }
#ifdef DEBUG
  Serial.println();
#endif

  init_package(REPLY_SET_BT_PIN);
  // 0: код ошибки
  if (!addData(OK)) return;

  sendData();
}

// установить лимит срабатывания сигнала о разряде батареи
void setBatteryLimit()
{
  // 0-3: коэфф.
  union Convert
  {
    float number;
    uint8_t byte[4];
  } p;

  for (uint8_t i = 0; i < 4; i++)
  {
    p.byte[i] = uartBuffer[DATA_START_BYTE + i];
    if (!eepromwrite(EEPROM_BATTERY_LIMIT + i * 3, uartBuffer[DATA_START_BYTE + i]))
    {
      sendError(EEPROM_WRITE_ERROR, REPLY_SET_BATTERY_LIMIT);
      return;
    }
  }
  batteryLimit = p.number;

  init_package(REPLY_SET_BATTERY_LIMIT);
  // 0: код ошибки
  // 1...: данные из флэша
  if (!addData(OK)) return;

  sendData();
}

// получить список команд на флэше
void scanTeams()
{
#ifdef DEBUG
  Serial.print(F("!!!Scan commands in flash"));
#endif

  // 0-1: номер команды  
  uint16_t startNumber = uartBuffer[DATA_START_BYTE] * 256 + uartBuffer[DATA_START_BYTE + 1];
#ifdef DEBUG
  Serial.print(F("!!!Start from command "));
  Serial.println(String(startNumber));
#endif

  if (startNumber < 1 || startNumber > maxTeamNumber)
  {
    sendError(WRONG_DATA, REPLY_SCAN_TEAMS);
    return;
  }

  init_package(REPLY_SCAN_TEAMS);
  // 0: код ошибки
  if (!addData(OK)) return;

  // 1...: список команд
  uint8_t data[2];
  for (; startNumber <= maxTeamNumber; startNumber++)
  {
#ifdef DEBUG
    Serial.print(F("!!!Trying "));
    Serial.println(String(startNumber));
#endif

    uint32_t addr = uint32_t(uint32_t(startNumber) * uint32_t(TEAM_FLASH_SIZE));

    if (!SPIflash.readByteArray(addr, data, 2))
    {
      sendError(FLASH_READ_ERROR, REPLY_SCAN_TEAMS);
      return;

    }
    if (data[0] != 0xff)
    {
#ifdef DEBUG
      Serial.print(F("!!!Found "));
      Serial.println(String(startNumber));
#endif
      if (!addData(data[0])) return;
      if (!addData(data[1])) return;
    }
    if (uartBufferPosition > 252) break;
  }
  sendData();
}

// отправить команду в BT-модуль
void sendBtCommand()
{
  String btCommand;
  btCommand.reserve(uartBuffer[LENGTH_BYTE]);
  for (uint16_t i = 0; i < uartBuffer[LENGTH_BYTE]; i++)
  {
    btCommand += String(char(uartBuffer[DATA_START_BYTE + i]));
  }

  String reply = sendCommandToBt(btCommand, (uint8_t)btCommand.length());
  if (reply.length() > 256 - 9)
  {
    sendError(BT_ERROR, REPLY_SEND_BT_COMMAND);
    return;
  }

  init_package(REPLY_SEND_BT_COMMAND);

  if (!addData(OK)) return;

  for (uint8_t i = 0; i < reply.length(); i++)
  {
    if (!addData(reply[i])) return;
  }

  sendData();
}

// получить последние ошибки
void getLastErrors()
{
  init_package(REPLY_GET_LAST_ERRORS);

  // 0: код ошибки
  if (!addData(OK)) return;

  // номера последних ошибок
  uint8_t i = 0;
  while (lastErrors[i] != 0 && i < lastTeamsLength)
  {
    if (!addData(lastErrors[i])) return;
    i++;
  }
  sendData();
  for (i = 0; i < lastTeamsLength; i++) lastErrors[i] = 0;
}

// Internal functions

// поменять имя BT адаптера
// Переделать на работу с указателем
bool setBtName(String name)
{
  bool result = false;
  digitalWrite(BT_COMMAND_ENABLE, HIGH);
  delay(200);
  // AT+NAME=<nameArray> [1-32]
  Serial.println("AT+NAME=" + name);
  char reply[2] = { 0,0 };
  delay(200);
  Serial.readBytes(reply, 2);
  while (Serial.available()) Serial.read();
  digitalWrite(BT_COMMAND_ENABLE, LOW);
  delay(200);
  while (Serial.available()) Serial.read();
  if (reply[0] == 'O' && reply[1] == 'K')
  {
    result = true;
  }
  return result;
}

// поменять пин-код BT адаптера
// Переделать на работу с указателем
bool setBtPinCode(String code)
{
  bool result = false;
  digitalWrite(BT_COMMAND_ENABLE, HIGH);
  delay(200);
  // AT+PSWD:"<nameArray>" [1-16] for HC-06
  // AT+PSWD=<nameArray> [1-16] for HC-05
  Serial.println("AT+PSWD:\"" + code + "\"");
  char reply[2] = { 0,0 };
  delay(200);
  Serial.readBytes(reply, 2);
  while (Serial.available()) Serial.read();
  if (reply[0] == 'O' && reply[1] == 'K')
  {
    result = true;
  }
  else
  {
    Serial.println("AT+PSWD=" + code);
    reply[0] = 0;
    reply[1] = 0;
    delay(200);
    Serial.readBytes(reply, 2);
    while (Serial.available()) Serial.read();
    if (reply[0] == 'O' && reply[1] == 'K')
    {
      result = true;
    }
  }
  digitalWrite(BT_COMMAND_ENABLE, LOW);
  delay(200);

  while (Serial.available()) Serial.read();
  return result;
}

String sendCommandToBt(String btCommand, uint8_t length)
{
  String result = "";
  result.reserve(250);
  digitalWrite(BT_COMMAND_ENABLE, HIGH);
  delay(200);
  Serial.print(btCommand);
  delay(200);
  while (Serial.available())
  {
    result += (char)Serial.read();
  }
  digitalWrite(BT_COMMAND_ENABLE, LOW);
  delay(200);
  return result;
}

// заполнить буфер смены маски
void saveNewMask()
{
#ifdef DEBUG
  Serial.print(F("!!!New mask set to: "));
#endif

  for (uint8_t i = 0; i < 8; i++)
  {
    newTeamMask[i] = uartBuffer[DATA_START_BYTE + i];
#ifdef DEBUG
    Serial.print(String(newTeamMask[i], HEX));
#endif
  }
#ifdef DEBUG
  Serial.println();
#endif
}

// очистить буфер смены маски
void clearNewMask()
{
  for (uint8_t i = 0; i < 8; i++)
  {
    newTeamMask[i] = 0;
  }
#ifdef DEBUG
  Serial.print(F("!!!Mask cleared: "));
#endif
}

// чтение заряда батареи
uint16_t getBatteryLevel()
{
  const uint8_t MeasurementsToAverage = 16;
  uint32_t AverageValue = analogRead(BATTERY_PIN);
  for (uint8_t i = 1; i < MeasurementsToAverage; ++i)
  {
    uint16_t val = analogRead(BATTERY_PIN);
    AverageValue = (AverageValue + val) / 2;
    delay(5);
  }
  return AverageValue;
}

// запись в память с мажоритарным резервированием
bool eepromwrite(uint16_t adr, uint8_t val)
{
  for (uint8_t i = 0; i < 3; i++)
  {
    EEPROM.write(adr + i, val);
    if (EEPROM.read(adr + i) != val) return false;
  }
  return true;
}

// считывание из памяти с учетом мажоритального резервирования
int eepromread(uint16_t adr)
{
  uint8_t byte1 = EEPROM.read(adr);
  uint8_t byte2 = EEPROM.read(adr + 1);
  uint8_t byte3 = EEPROM.read(adr + 2);

  // возвращаем при совпадении два из трех
  if (byte1 == byte2 && byte1 == byte3)
  {
    return byte1;
  }
  if (byte1 == byte2)
  {
    return byte1;
  }
  if (byte1 == byte3)
  {
    return byte1;
  }
  if (byte2 == byte3)
  {
    return byte2;
  }
  return -1;
}

// сигнал станции, длительность сигнала и задержки в мс и число повторений
void beep(uint8_t n, uint16_t ms)
{
  for (uint8_t i = 0; i < n; i++)
  {
    digitalWrite(INFO_LED_PIN, HIGH);
    tone(BUZZER_PIN, 4000, ms);
    delay(ms);
    digitalWrite(INFO_LED_PIN, LOW);
    if (n - i != 0)
    {
      delay(ms);
    }
  }
}

// сигнал ошибки станции
void errorBeepMs(uint8_t n, uint16_t ms)
{
  for (uint8_t i = 0; i < n; i++)
  {
    digitalWrite(ERROR_LED_PIN, HIGH);
    tone(BUZZER_PIN, 500, ms);
    delay(ms);
    digitalWrite(ERROR_LED_PIN, LOW);
    if (n - i > 0)
    {
      delay(ms);
    }
  }
}

// сигнал ошибки станции
void errorBeep(uint8_t n)
{
  uint16_t ms = 200;
  for (uint8_t i = 0; i < n; i++)
  {
    digitalWrite(ERROR_LED_PIN, HIGH);
    tone(BUZZER_PIN, 500, ms);
    delay(ms);
    digitalWrite(ERROR_LED_PIN, LOW);
    if (n - i > 0)
    {
      delay(ms);
    }
  }
}

// инициализация пакета данных
void init_package(uint8_t command)
{
  uartBuffer[0] = uartBuffer[1] = 0xFE;
  uartBuffer[STATION_NUMBER_BYTE] = stationNumber;
  uartBuffer[COMMAND_BYTE] = command;
  uartBufferPosition = DATA_START_BYTE;
}

// добавление данных в буфер
bool addData(uint8_t data)
{
  if (uartBufferPosition > 254)
  {
    sendError(BUFFER_OVERFLOW);
    return false;
  }
  uartBuffer[uartBufferPosition] = data;
  uartBufferPosition++;
  return true;
}

// передача пакета данных
void sendData()
{
  uartBuffer[LENGTH_BYTE] = uartBufferPosition - COMMAND_BYTE - 1;
  uartBuffer[uartBufferPosition] = crcCalc(uartBuffer, PACKET_ID, uartBufferPosition - 1);
  uartBufferPosition++;
#ifdef DEBUG
  Serial.print(F("!!!Sending:"));
  for (uint8_t i = 0; i < uartBufferPosition; i++)
  {
    Serial.print(F(" "));
    if (uartBuffer[i] < 0x10) Serial.print(F("0"));
    Serial.print(String(uartBuffer[i], HEX));
  }
  Serial.println();
#endif
  Serial.write(uartBuffer, uartBufferPosition);
  uartBufferPosition = 0;
}

// запись страницы (4 байта) в чип
bool ntagWritePage(uint8_t* dataBlock, uint8_t pageAdr)
{
  const uint8_t sizePageNtag = 4;

  uint8_t n = 0;

  MFRC522::StatusCode status = MFRC522::STATUS_ERROR;
  while (status != MFRC522::STATUS_OK && n < 3)
  {
    status = MFRC522::StatusCode(mfrc522.MIFARE_Ultralight_Write(pageAdr, dataBlock, sizePageNtag));
    n++;
    if (status != MFRC522::STATUS_OK)
    {
      mfrc522.PCD_Init();
      mfrc522.PICC_IsNewCardPresent();
      mfrc522.PICC_ReadCardSerial();
    }
  }

  if (status != MFRC522::STATUS_OK)
  {
#ifdef DEBUG
    Serial.println(F("!!!chip write failed"));
#endif

    return false;
  }

  uint8_t buffer[18];
  uint8_t size = sizeof(buffer);

  n = 0;
  status = MFRC522::STATUS_ERROR;
  while (status != MFRC522::STATUS_OK && n < 3)
  {
    status = MFRC522::StatusCode(mfrc522.MIFARE_Read(pageAdr, buffer, &size));
    n++;
  }
  if (status != MFRC522::STATUS_OK)
  {
#ifdef DEBUG
    Serial.println(F("!!!chip read failed"));
#endif

    return false;
  }

  for (uint8_t i = 0; i < 4; i++)
  {
    if (buffer[i] != dataBlock[i])
    {
#ifdef DEBUG
      Serial.println(F("!!!chip verify failed"));
#endif
      return false;
    }
  }

  return true;
}

// чтение 4-х страниц (16 байт) из чипа
bool ntagRead4pages(uint8_t pageAdr)
{
  uint8_t size = 18;
  uint8_t buffer[18];

  uint8_t n = 0;

  MFRC522::StatusCode status = MFRC522::STATUS_ERROR;
  while (status != MFRC522::STATUS_OK && n < 3)
  {
    status = MFRC522::StatusCode(mfrc522.MIFARE_Read(pageAdr, buffer, &size));
    if (status != MFRC522::STATUS_OK)
    {
      mfrc522.PCD_Init();
      mfrc522.PICC_IsNewCardPresent();
      mfrc522.PICC_ReadCardSerial();
    }
    n++;
  }

  if (status != MFRC522::STATUS_OK)
  {
#ifdef DEBUG
    Serial.println(F("!!!card read failed"));
#endif
    return false;
  }

  for (uint8_t i = 0; i < 16; i++)
  {
    ntag_page[i] = buffer[i];
  }
  return true;
}

// пишет на чип время и станцию отметки
bool writeCheckPointToCard(uint8_t newPage, uint32_t checkTime)
{
  uint8_t dataBlock[4];
  dataBlock[0] = stationNumber;
  dataBlock[1] = (checkTime & 0x00FF0000) >> 16;
  dataBlock[2] = (checkTime & 0x0000FF00) >> 8;
  dataBlock[3] = (checkTime & 0x000000FF);

  if (!ntagWritePage(dataBlock, newPage))
  {
    return false;
  }
  return true;
}

// Поиск пустой на чипе.
// !!! разобраться в алгоритме Саши или сделать свой бинарный поиск
int findNewPage()
{
  int page = PAGE_DATA_START;
  while (page < TAG_MAX_PAGE)
  {
    if (!ntagRead4pages(page))
    {
#ifdef DEBUG
      Serial.println(F("!!!Can't read chip"));
#endif
      // chip read error
      return 0;
    }
    for (uint8_t n = 0; n < 4; n++)
    {
      if (stationMode == MODE_START_KP && ntag_page[n * 4] == stationNumber)
      {
#ifdef DEBUG
        Serial.println(F("!!!Chip checked already"));
#endif
        // chip was checked by another station with the same number
        return -1;
      }
      if (ntag_page[n * 4] == 0 ||
        (stationMode == MODE_FINISH_KP && ntag_page[n * 4] == stationNumber))
      {
        // free page found
        return page;
      }
      page++;
    }
  }
  // чип заполнен
  return TAG_MAX_PAGE;
}

// пишем дамп чипа в лог
bool writeDumpToFlash(uint16_t teamNumber, uint32_t checkTime, uint32_t initTime, uint16_t mask)
{
  // адрес хранения в каталоге
  const uint32_t teamFlashAddress = uint32_t(uint32_t(teamNumber) * uint32_t(TEAM_FLASH_SIZE));
#ifdef DEBUG
  Serial.print(F("!!!Write to flash address: "));
  Serial.println(String(teamFlashAddress));
#endif
  if (SPIflash.readByte(teamFlashAddress) != 0xff)
  {
    // если режим финишной станции, то надо переписать содержимое
    if (stationMode == MODE_FINISH_KP)
    {
#ifdef DEBUG
      Serial.print(F("!!!erasing team #"));
      Serial.println(String(teamNumber));
#endif
      if (!eraseTeamFromFlash(teamNumber))
      {
#ifdef DEBUG
        Serial.println(F("!!!fail to erase"));
#endif
        return false;
      }
    }
    else
    {
#ifdef DEBUG
      Serial.print(F("!!!team already saved"));
#endif
      return false;
    }
  }

  // save basic parameters
  uint8_t basic_record[12];
  // 1-2: номер команды
  basic_record[0] = (teamNumber & 0xFF00) >> 8;
  basic_record[1] = teamNumber & 0x00FF;
  //3-6: время инициализации
  basic_record[2] = (initTime & 0xFF000000) >> 24;
  basic_record[3] = (initTime & 0x00FF0000) >> 16;
  basic_record[4] = (initTime & 0x0000FF00) >> 8;
  basic_record[5] = initTime & 0x000000FF;
  //7-8: маска команды
  basic_record[6] = (mask & 0xFF00) >> 8;
  basic_record[7] = mask & 0x00FF;
  //9-12: время последней отметки на станции
  basic_record[8] = (checkTime & 0xFF000000) >> 24;
  basic_record[9] = (checkTime & 0x00FF0000) >> 16;
  basic_record[10] = (checkTime & 0x0000FF00) >> 8;
  basic_record[11] = checkTime & 0x000000FF;
  bool flag = SPIflash.writeByteArray(teamFlashAddress, basic_record, 12);
  if (!flag)
  {
#ifdef DEBUG
    Serial.println(F("!!!fail write flash1"));
#endif
    return false;
  }

#ifdef DEBUG
  Serial.println(F("!!!basics wrote"));
#endif

  // copy card content to flash. все страницы не начинающиеся с 0
  uint8_t checkCount = 0;
  uint8_t block = 0;
  while (block < TAG_MAX_PAGE)
  {
#ifdef DEBUG
    Serial.print(F("!!!reading 4 page from #"));
    Serial.println(String(block));
#endif
    if (!ntagRead4pages(block))
    {
#ifdef DEBUG
      Serial.println(F("!!!fail read chip"));
#endif
      flag = false;
      break;
    }

    //4 pages in one read block
    for (uint8_t i = 0; i < 4; i++)
    {
      if (block < 8 || ntag_page[i * 4]>0)
      {
#ifdef DEBUG
        Serial.print(F("!!!writing to flash: "));
        for (uint8_t k = 0; k < 16; k++)Serial.print(String(ntag_page[k], HEX) + " ");
        Serial.println();
#endif
        flag &= SPIflash.writeByteArray(uint32_t(teamFlashAddress + uint32_t(16) + uint32_t(block) * uint32_t(4) + uint32_t(i) * uint32_t(4) + uint32_t(0)), &ntag_page[0 + i * 4], 4);
        checkCount++;
      }
      else
      {
#ifdef DEBUG
        Serial.print(F("!!!chip last block: "));
        Serial.println(String(block + i));
#endif
        block = TAG_MAX_PAGE;
        break;
      }
    }
    block += 4;
  }
  // add dump pages number
  flag &= SPIflash.writeByte(uint32_t(teamFlashAddress + uint32_t(12)), checkCount);
#ifdef DEBUG
  if (!flag)
  {
    Serial.println(F("!!!fail write flash2"));
  }
#endif
  //if (!flag) eraseTeamFromFlash(teamNumber);
  return flag;
}

// сохраняем весь блок, стираем весь блок и возвращаем назад все, кроме переписываемой команды
// оптимизировать чтение и запись флэш (блоками)
bool eraseTeamFromFlash(uint16_t teamNumber)
{
  bool flag = true;
  uint32_t const tmpBufferStart = uint32_t(uint32_t(uint32_t(maxTeamNumber + 1) + uint32_t(1)) * uint32_t(TEAM_FLASH_SIZE));
  const uint32_t eraseBlockFlashAddress = uint32_t(teamNumber) * uint32_t(TEAM_FLASH_SIZE) / uint32_t(FLASH_BLOCK_SIZE) * uint32_t(FLASH_BLOCK_SIZE);
  const uint8_t teamInBlock = uint32_t(uint32_t(uint32_t(teamNumber) * uint32_t(TEAM_FLASH_SIZE) - uint32_t(eraseBlockFlashAddress)) / uint32_t(TEAM_FLASH_SIZE));
#ifdef DEBUG
  Serial.print(F("!!!teamNumber="));
  Serial.println(String(teamNumber));
  Serial.print(F("!!!tmpBufferStart="));
  Serial.println(String(tmpBufferStart));
  Serial.print(F("!!!teamInBlock="));
  Serial.println(String(teamInBlock));
#endif

  // erase sector
  flag &= SPIflash.eraseSector(tmpBufferStart);

  // backup Flash Block
  for (byte i = 0; i < int32_t(FLASH_BLOCK_SIZE) / int32_t(TEAM_FLASH_SIZE); i++)
  {
    // не копировать перезаписываемую запись
    if (i != teamInBlock)
    {
#ifdef DEBUG
      Serial.print(F("!!!Copy team "));
      Serial.println(String(teamNumber - teamInBlock + i));
      Serial.print(F("!!!to "));
      Serial.println(String(maxTeamNumber + 1 + i));
#endif

      copyTeam(teamNumber - teamInBlock + i, maxTeamNumber + 1 + i);
    }
  }

  // erase sector
  flag &= SPIflash.eraseSector(eraseBlockFlashAddress);

  // restore Flash Block
  for (byte i = 0; i < int32_t(FLASH_BLOCK_SIZE) / int32_t(TEAM_FLASH_SIZE); i++)
  {
    // не копировать перезаписываемую запись
    if (i != teamInBlock)
    {
      copyTeam(uint16_t(maxTeamNumber + uint16_t(1) + uint16_t(i)), uint16_t(teamNumber - uint16_t(teamInBlock) + uint16_t(i)));
    }
  }

  return flag;
}

//copies team record from one place to another. Destination block must be cleaned already.
bool copyTeam(uint16_t teamNumberFrom, uint16_t teamNumberTo)
{
  const uint32_t teamFlashAddressFrom = uint32_t(uint32_t(teamNumberFrom) * uint32_t(TEAM_FLASH_SIZE));
  const uint32_t teamFlashAddressTo = uint32_t(uint32_t(teamNumberTo) * uint32_t(TEAM_FLASH_SIZE));
#ifdef DEBUG
  Serial.print(F("!!!teamNumberFrom="));
  Serial.println(String(teamNumberFrom));
  Serial.print(F("!!!FlashAddressFrom="));
  Serial.println(String(teamFlashAddressFrom));

  Serial.print(F("!!!teamNumberTo="));
  Serial.println(String(teamNumberTo));
  Serial.print(F("!!!FlashAddressTo="));
  Serial.println(String(teamFlashAddressTo));
#endif

  bool flag = true;
  // backup Flash Block
  const int bufSize = 256;
  uint8_t b[bufSize];
  uint32_t i = 0;
  while (i < uint32_t(TEAM_FLASH_SIZE))
  {
    if (SPIflash.readByte(uint32_t(teamFlashAddressFrom + uint32_t(i))) != 0xff)
    {
      flag &= SPIflash.readByteArray(uint32_t(teamFlashAddressFrom + uint32_t(i)), b, bufSize);
      flag &= SPIflash.writeByteArray(uint32_t(teamFlashAddressTo + uint32_t(i)), b, bufSize);
    }
    i += bufSize;
  }
  return flag;
}

// получаем сведения о команде из лога
bool readTeamFromFlash(uint16_t recordNum)
{
  const uint32_t addr = uint32_t(uint32_t(recordNum) * uint32_t(TEAM_FLASH_SIZE));
  if (SPIflash.readByte(addr) == 0xff) return false;
  // #команды
  // время инициализации
  // маска
  // время отметки
  // счетчик страниц на чипе
  SPIflash.readByteArray(addr, ntag_page, 13);
  return true;
}

// подсчет записанных в флэш отметок
uint16_t refreshChipCounter()
{
#ifdef DEBUG
  Serial.print(F("!!!team records found="));
#endif

  uint16_t chips = 0;

  for (uint16_t i = 1; i <= maxTeamNumber; i++)
  {
    uint32_t addr = uint32_t(uint32_t(i) * uint32_t(TEAM_FLASH_SIZE));
    uint16_t c = SPIflash.readByte(addr);
    if (SPIflash.readByte(addr) != 255)
    {
      chips++;
      uint32_t time = SPIflash.readByte(addr + 8);
      time <<= 8;
      time += SPIflash.readByte(addr + 9);
      time <<= 8;
      time += SPIflash.readByte(addr + 10);
      time <<= 8;
      time += SPIflash.readByte(addr + 11);
      if (time > lastTimeChecked)
      {
        lastTimeChecked = time;
        lastTeams[0] = c;
        lastTeams[1] = SPIflash.readByte(addr + 1);
      }
#ifdef DEBUG
      Serial.print(String(i));
      Serial.print(F(", "));
#endif
    }
  }
#ifdef DEBUG
  Serial.println();
  Serial.print(F("!!!checked chip counter="));
  Serial.println(String(chips));
#endif
  return chips;
}

// обработка ошибок. формирование пакета с сообщением о ошибке
void sendError(uint8_t errorCode, uint8_t commandCode)
{
  init_package(commandCode);
  uartBuffer[DATA_START_BYTE] = errorCode;
  uartBufferPosition = DATA_START_BYTE + 1;
  sendData();
}

void sendError(uint8_t errorCode)
{
  uartBuffer[DATA_START_BYTE] = errorCode;
  uartBufferPosition = DATA_START_BYTE + 1;
  sendData();
}

// сделать проверку дублей по всему массиву
// добавляем номер в буфер последних команд
void addLastTeam(uint16_t teamNumber, bool already_checked)
{
  // фильтровать дубли
  if (lastTeams[0] == uint8_t(teamNumber >> 8) && lastTeams[1] == uint8_t(teamNumber)) return;

  for (uint8_t i = lastTeamsLength * 2 - 1; i > 1; i = i - 2)
  {
    lastTeams[i] = lastTeams[i - 2];
    lastTeams[i - 1] = lastTeams[i - 3];
  }
  lastTeams[0] = uint8_t(teamNumber >> 8);
  lastTeams[1] = uint8_t(teamNumber);
  if (!already_checked) totalChipsChecked++;
}

// добавляем код ошибки в буфер последних ошибок
void addLastError(uint8_t errorCode)
{
  for (uint8_t i = lastTeamsLength - 1; i > 0; i--)
  {
    lastErrors[i] = lastErrors[i - 1];
  }
  lastErrors[0] = errorCode;
}

uint8_t crcCalc(uint8_t* dataArray, uint16_t startPosition, uint16_t dataEnd)
{
  uint8_t crc = 0x00;
  uint16_t i = startPosition;
  while (i <= dataEnd)
  {
    uint8_t tmpByte = dataArray[i];
    for (uint8_t tempI = 8; tempI; tempI--)
    {
      uint8_t sum = (crc ^ tmpByte) & 0x01;
      crc >>= 1;
      if (sum)
      {
        crc ^= 0x8C;
      }
      tmpByte >>= 1;
    }
    i++;
  }
  return (crc);
}

void floatToByte(uint8_t* bytes, float f)
{
  uint16_t length = sizeof(float);

  for (uint16_t i = 0; i < length; i++)
  {
    bytes[i] = ((uint8_t*)&f)[i];
  }
}

// check chip type consistence
bool selectChipType(uint8_t type)
{
  if (type == NTAG213_ID) //NTAG213
  {
    chipType = type;
    NTAG_MARK = NTAG213_MARK;
    TAG_MAX_PAGE = NTAG213_MAX_PAGE;
  }
  else if (type == NTAG216_ID) //NTAG216
  {
    chipType = type;
    NTAG_MARK = NTAG216_MARK;
    TAG_MAX_PAGE = NTAG216_MAX_PAGE;
  }
  else //NTAG215
  {
    chipType = NTAG215_ID;
    NTAG_MARK = NTAG215_MARK;
    TAG_MAX_PAGE = NTAG215_MAX_PAGE;
    if (chipType != NTAG215_ID) return false;
  }
  return true;
}
