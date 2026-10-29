// Bundled example sketches. All of them compile offline against the bundled
// ATmega328P core (Wire / SPI / Servo / DHT included).
export interface Example {
  id: string;
  title: string;    // fa
  titleEn: string;
  desc: string;     // fa
  code: string;
}

export const EXAMPLES: Example[] = [
  {
    id: 'blink',
    title: 'چراغ Blink',
    titleEn: 'Blink',
    desc: 'چراغ داخلی Uno را روشن و خاموش می‌کند',
    code: `// ✨ اولین اسکرت Arduino — چراغ داخلی را می‌سوزد
// دکمه بررسی و رفع خطا را بزنید تا یک HEX قابل بارگذاری تولید شود.

const int ledPin = LED_BUILTIN;   // پین چراغ روی برد Uno (عدد ۱۳)

void setup() {
  pinMode(ledPin, OUTPUT);        // پین را به صورت خروجی تنظیم می‌کنیم
}

void loop() {
  digitalWrite(ledPin, HIGH);     // روشن
  delay(500);                     // ۵۰۰ میلی‌ثانیه توقف
  digitalWrite(ledPin, LOW);      // خاموش
  delay(500);                     // ۵۰۰ میلی‌ثانیه توقف
}
`,
  },
  {
    id: 'serial-hello',
    title: 'سلام دنیا از سری',
    titleEn: 'Serial Hello',
    desc: 'خروجی سری را روی مانیتور نشان می‌دهد',
    code: `// سلام از مانیتور سری!
// دکمه مانیتور سری را بزنید و با بازدهی 9600 output را ببینید.

void setup() {
  Serial.begin(9600);             // با بازدهی 9600 به جعبه سری متصل می‌شویم
  Serial.println("AMIN IDE سلام!");
}

void loop() {
  static unsigned long t0 = 0;
  unsigned long now = millis();
  if (now - t0 >= 1000) {         // هر ۱ ثانیه یک بار
    t0 = now;
    Serial.print("زمان: ");
    Serial.print(now / 1000);
    Serial.println(" ثانیه");
  }
}
`,
  },
  {
    id: 'fade',
    title: 'تغییر تدریجی نور',
    titleEn: 'Fade',
    desc: 'می‌تواند نور PWM را کم و زیاد کند',
    code: `// نور PWM را افزایش و کاهش می‌دهد
const int ledPin = LED_BUILTIN;

void setup() {
  pinMode(ledPin, OUTPUT);
}

void loop() {
  for (int brightness = 0; brightness <= 255; brightness += 5) {
    analogWrite(ledPin, brightness);   // نور را کماً به تدریج افزایش می‌دهد
    delay(20);
  }
  for (int brightness = 255; brightness >= 0; brightness -= 5) {
    analogWrite(ledPin, brightness);   // نور را کم و کم می‌کند
    delay(20);
  }
}
`,
  },
  {
    id: 'button',
    title: 'دکمه با pull-up داخلی',
    titleEn: 'Button (internal pull-up)',
    desc: 'دکمه را فشار دهید و وضعیتش را از روی سری ببینید',
    code: `// دکمه میان پین و GND، با pull-up داخلی
const int buttonPin = 2;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);   // pull-up داخلی فعال می‌شود
  Serial.begin(9600);
}

void loop() {
  int state = digitalRead(buttonPin);
  if (state == LOW) {
    Serial.println("دکمه فشرده شد");
  } else {
    Serial.println("دکمه باز است");
  }
  delay(300);
}
`,
  },
  {
    id: 'potentiometer',
    title: 'ولتاژ گردان',
    titleEn: 'Potentiometer',
    desc: 'ولتاژ A0 را می‌خواند و روی سری نمایش می‌دهد',
    code: `// پرنال (گردان) را به A0 وصل کنید و ولتاژ را ببینید
const int potPin = A0;

void setup() {
  Serial.begin(9600);
}

void loop() {
  int value = analogRead(potPin);              // ۰..۱۰۲۳
  float voltage = value * (5.0 / 1023.0);     // ولتاژ
  Serial.print("ولتاژ: ");
  Serial.print(voltage);
  Serial.println(" V");
  delay(300);
}
`,
  },
  {
    id: 'wire-scan',
    title: 'اسکن I2C',
    titleEn: 'Wire Scanner',
    desc: 'آدرس‌های I2C دستگاه‌های متصل را پیدا می‌کند',
    code: `// دستگاه‌های I2C متصل را اسکن می‌کند
#include <Wire.h>

void setup() {
  Wire.begin();                   // I2C master
  Serial.begin(9600);
  Serial.println("اسکن I2C...");
  for (uint8_t addr = 1; addr < 128; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("دستگاه پیدا شد: 0x");
      Serial.println(addr, HEX);
    }
  }
  Serial.println("تموم شد");
}

void loop() { delay(2000); }
`,
  },
  {
    id: 'servo-sweep',
    title: 'چرخش servo',
    titleEn: 'Servo Sweep',
    desc: 'سرو موتور را از حداقل تا حداکثر می‌چرخاند',
    code: `// موتور servo را از ۰ تا ۱۸۰ درجه می‌چرخاند
#include <Servo.h>

Servo servo;
const int servoPin = 9;

void setup() {
  servo.attach(servoPin);
}

void loop() {
  for (int angle = 0; angle <= 180; angle += 5) {
    servo.write(angle);
    delay(30);
  }
  for (int angle = 180; angle >= 0; angle -= 5) {
    servo.write(angle);
    delay(30);
  }
}
`,
  },
  {
    id: 'dht11',
    title: 'سنسور DHT11',
    titleEn: 'DHT11',
    desc: 'دما و رطوبت از DHT11 را می‌خواند',
    code: `// سنسور DHT11 را به پین ۲ وصل کنید
#include <DHT.h>

const int dhtPin = 2;
DHT dht(dhtPin, DHT11);

void setup() {
  Serial.begin(9600);
  dht.begin();
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  Serial.print("رطوبت: ");  Serial.print(h);
  Serial.print("%  دما: "); Serial.print(t);
  Serial.println(" C");
  delay(2000);
}
`,
  },
  {
    id: 'error-demo',
    title: 'نمونه خطای کامپایل',
    titleEn: 'Compile Error Demo',
    desc: 'یک اشتباه کوچک دارد — دو خطای کامپایل نشان می‌دهد',
    code: `// این اسکرت عمداً اشتباه دارد — دکمه بررسی و رفع خطا را بزنید تا
// خطاهای کامپایل با خط قرمز و کلیک‌پذیر نشان داده شود.

void setup() {
  pinMode(LED_BUILTIN, OUTPUT)   // خطا: semicolon گم شده (;)
  Serial.begin(9600);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}
`,
  },
];

export function exampleById(id: string): Example | undefined {
  return EXAMPLES.find((e) => e.id === id);
}
