#define MELODY_SIZE 28    //Размер проигрываемой мелодии.
#define PAUSE_NOTES 1.30  //Соотношение между мелодией и паузой.
#define LED_IN_LAYERS 16  //Cветодиодов в слое.
#define LAYERS_COUNT 4    //Количество слоев в кубе.

#include "pitch.h"

//Код проигрываемой мелодии - «В лесу родилась елочка».
int melody[] = {
  NOTE_C4, NOTE_A4, NOTE_A4, NOTE_G4,
  NOTE_A4, NOTE_F4, NOTE_C4, NOTE_C4,
  NOTE_C4, NOTE_A4, NOTE_A4, NOTE_AS4,
  NOTE_G4, NOTE_C5, NOTE_C5, NOTE_D4,
  NOTE_D4, NOTE_AS4, NOTE_AS4, NOTE_A4,
  NOTE_G4, NOTE_F4, NOTE_C4, NOTE_A4,
  NOTE_A4, NOTE_G4, NOTE_A4, NOTE_F4
};

//Продолжительность звучания каждой ноты.
int Durations[] = { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2 };

//Номера выводов платы(не микроконтроллера!) к которым подключаются регистры сдвига для управления светодиодами в слоях.
int latchPin = 10;  //Пин подключен к ST_CP входу 74HC595
int clockPin = 13;  //Пин подключен к SH_CP входу 74HC595
int dataPin = 11;   //Пин подключен к DS входу 74HC595

//Переменная с номером пина к которому подключен повторитель на транзисторе для динамика.
const int soundPin = 3;  //D3 вывод на плате arduino. PD3 вывод микроконтроллера.

int layerFlag;  //Номер текущего работающего слоя.

int curentMode;   //Номер эффекта который сейчас работает.
int effectTimer;  //Флаг счета времени смены эффектов. Програмный таймер.

uint16_t curiterat;  //Текущая итерация обработки эффекта.
uint16_t ticvalue;   //Временная переменная для программного таймера tickValue.
uint16_t curnote;    //Текущая нота.
int tmpVal;          //Временное значение для эффекта сдвига.

void setup() {

  effectTimer = 0;
  curentMode = 0;
  curiterat = 0;
  ticvalue = 0;
  curnote = 0;

  pinMode(A0, OUTPUT);  //Устанавливаю порт как выход, высокий уровень на выводе включает слой 0.
  pinMode(A1, OUTPUT);  //Устанавливаю порт как выход, высокий уровень на выводе включает слой 1.
  pinMode(A2, OUTPUT);  //Устанавливаю порт как выход, высокий уровень на выводе включает слой 2.
  pinMode(A3, OUTPUT);  //Устанавливаю порт как выход, высокий уровень на выводе включает слой 3.

  pinMode(latchPin, OUTPUT);  //Настраивает выводы платы подключенные к регистрам как выходы.
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  digitalWrite(A0, HIGH);  //После запуска включаем слой 0. Остальные отключены.
  digitalWrite(A1, LOW);
  digitalWrite(A2, LOW);
  digitalWrite(A3, LOW);

  pinMode(soundPin, OUTPUT);  //Объявляем 3 вывод на плате как выход, для работы динамика.

  //Настройка прерываний таймера.Для смены режимов. Описание работы с таймерами в arduino https://habr.com/ru/post/453276/

  //Инициализация Timer1 16 битный.
  cli();       //Отключить глобальные прерывания
  TCCR1A = 0;  //Установить TCCR1A регистр в 0
  TCCR1B = 0;

  //Делитель на 1024.
  //На вход таймера поступит сигнал ~16 000 000 гц(встроенные кварцевый генератор)/1024 = 15 625
  TCCR1B |= (1 << CS12);
  // TCCR1B |= (1 << CS11);
  TCCR1B |= (1 << CS10);

  TIMSK1 = (1 << TOIE1);  //Включить прерывание Timer1 overflow:
  sei();                  //Включить глобальные прерывания
}

//Прерывание по 16 битному таймеру. Изменяет эффекты отображаемые кубом.
ISR(TIMER1_OVF_vect) {
  //Прерывание приходит раз в 6 секунд.
  //Эффекты будут меняться 6*2=12. Каждые 12 секунд.
  if (effectTimer < 2) {
    effectTimer++;
    return;
  }
  effectTimer = 0;

  //Меняем текущий эффект куба.
  curentMode++;
}

//Бесконечный цикл(стандартная функция arduino).
void loop() {
  //Мелодия еще играет. Программный счетчик мелодий.
  if (curnote < MELODY_SIZE) {
    int Duration = 1000 / Durations[curnote];

    tone(soundPin, melody[curnote], Duration);
    int pauseNotes = Duration * PAUSE_NOTES;

    ledBlink();  //Выполняет математический операции и загрузку в регистры новых данных светового эффекта. Фактически переключение на другой поток (в терминах операционной системы)

    delay(pauseNotes);
    noTone(soundPin);

    curnote++;
  } else {
    curnote = 0;
  }
}

//Выполняет логику эффектов и переключает их(если прерывания по таймеру сменили их).
void ledBlink() {
  switch (curentMode) {
    case 0: verticalChangeEffect(0); break;
    case 1: snakeEffect(1); break;
    case 2: rainEffect(2); break;
    case 3: fastLayerEffect(3); break;
    default: curentMode = 0; break;
  }
}

//Очистка куба. Гасим все светодиоды.
void clearCube() {
  digitalWrite(latchPin, LOW);  //Устанавливаем синхронизацию "защелки" на LOW

  shiftOut(dataPin, clockPin, MSBFIRST, 0);  //Передаем последовательно на dataPin
  shiftOut(dataPin, clockPin, MSBFIRST, 0);

  digitalWrite(latchPin, HIGH);  //"Защелкиваем" регистр, тем самым устанавливая значения на выходах
}

//Зажигает светодиоды в слое согласно значению val. Каждый бит val это светодиод в слое. 16 бит-16 светодиодов.
void setLedInLayer(uint16_t data) {
  digitalWrite(latchPin, LOW);  // устанавливаем синхронизацию "защелки" на LOW
  //Передаем последовательно на dataPin
  shiftOut(dataPin, clockPin, MSBFIRST, (data >> 8));  //Выводим старший байт
  shiftOut(dataPin, clockPin, MSBFIRST, data);         //Выводим младший бит

  digitalWrite(latchPin, HIGH);  //"Защелкиваем" регистр, тем самым устанавливая значения на выходах
}

//Сменяет слой.
void changeLayer() {
  switch (layerFlag) {
    case 0:
      layerFlag = 1;
      digitalWrite(A0, LOW);
      digitalWrite(A1, HIGH);
      digitalWrite(A2, LOW);
      digitalWrite(A3, LOW);
      break;

    case 1:
      layerFlag = 2;
      digitalWrite(A0, LOW);
      digitalWrite(A1, LOW);
      digitalWrite(A2, HIGH);
      digitalWrite(A3, LOW);
      break;

    case 2:
      layerFlag = 3;
      digitalWrite(A0, LOW);
      digitalWrite(A1, LOW);
      digitalWrite(A2, LOW);
      digitalWrite(A3, HIGH);
      break;

    case 3:
      layerFlag = 0;
      digitalWrite(A0, HIGH);
      digitalWrite(A1, LOW);
      digitalWrite(A2, LOW);
      digitalWrite(A3, LOW);
      break;

    default:; break;
  }
}

//Эффект дождя.
void rainEffect(uint8_t effectOrder) {

  if (curiterat < LED_IN_LAYERS) {
    uint16_t leds = random(0, 65535);  //2 в степени количество светодиодов.
    setLedInLayer(leds);
    changeLayer();
    // delay(130);

    if (effectChange(effectOrder)) return;  //Изменен режим выходим
  } else {
    curiterat = 0;
  }
}

//Растущая змейка.
void snakeEffect(uint8_t effectOrder) {
  if (curiterat < LED_IN_LAYERS * LED_IN_LAYERS) {
    if (tmpVal > LED_IN_LAYERS) {
      tmpVal = 0;
      changeLayer();
    }

    int x = 1;
    for (int i = 0; i < tmpVal; i++) {
      x |= 1 << i;
    }

    tmpVal++;

    setLedInLayer(x);

    if (effectChange(effectOrder)) return;  //Изменен режим выходим
    curiterat++;

  } else {
    curiterat = 0;
  }
}

//Быстрое мигание слоями.
void fastLayerEffect(uint8_t effectOrder) {
  if (curiterat < LAYERS_COUNT * 2) {

    setLedInLayer(0xffff);      //Cветим всеми.
    if (!tickValue(5)) return;  //Программный таймер еще не досчитал.

    if (curiterat % 2 == 0) {
      setLedInLayer(0x0000);
      changeLayer();
    }

    curiterat++;

    if (effectChange(effectOrder)) return;  //Изменен режим выходим
  } else
    curiterat = 0;
}

//Эффект вертикальное переключение слоев , все светодиоды в слое светиться.
void verticalChangeEffect(uint8_t effectOrder) {
  setLedInLayer(65535);  //Cветим всеми.
  changeLayer();
  if (effectChange(effectOrder)) return;  //Изменен режим выходим.
}

//Программный таймер на val единиц.
boolean tickValue(uint16_t val) {
  if (val >= ticvalue) {
    ticvalue = 0;
    return true;
  } else {
    ticvalue++;
  }
  return false;
}

//Был ли изменен режим(номер эффекта)?
boolean effectChange(uint8_t effectOrder) {
  return curentMode != effectOrder;
}