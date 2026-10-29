// Libraries bundled offline in the toolchain (from its manifest.json).
export interface LibInfo {
  name: string;
  nameEn: string;
  desc: string;     // fa
  descEn: string;
  installed: boolean;
  kind: 'core' | 'lib' | 'sensor';
}

export const LIBS: LibInfo[] = [
  { name: 'کورِ AVR', nameEn: 'AVR Core', desc: 'وابسته‌های اصلی Arduino (digital/analog/Serial/…)', descEn: 'Core APIs', installed: true, kind: 'core' },
  { name: 'Wire (I2C)', nameEn: 'Wire', desc: 'ارتباط I2C / TWI', descEn: 'I2C / TWI bus', installed: true, kind: 'lib' },
  { name: 'SPI', nameEn: 'SPI', desc: 'ارتباط SPI', descEn: 'SPI bus', installed: true, kind: 'lib' },
  { name: 'Servo', nameEn: 'Servo', desc: 'کنترل موتور servo', descEn: 'Servo motor control', installed: true, kind: 'lib' },
  { name: 'DHT', nameEn: 'DHT', desc: 'سنسور دما و رطوبت DHT11 / DHT22', descEn: 'DHT11/DHT22 sensor', installed: true, kind: 'lib' },
  { name: 'Adafruit BMP085', nameEn: 'BMP085', desc: 'سنسور فشار جو BMP085', descEn: 'BMP085 pressure', installed: true, kind: 'lib' },
  { name: 'Adafruit BusIO', nameEn: 'BusIO', desc: 'ماژول I2C/SPI عمومی BusIO', descEn: 'Generic I2C/SPI', installed: true, kind: 'lib' },
  { name: 'Firmata', nameEn: 'Firmata', desc: 'رابط فرمان Firmata', descEn: 'Firmata protocol', installed: true, kind: 'lib' },
  { name: 'پکیج OLED (SSD1306)', nameEn: 'SSD1306 (OLED)', desc: 'پکیج سنسور: نمایشگر OLED — روی این برای فعال‌سازی', descEn: 'OLED display pack', installed: true, kind: 'sensor' },
  { name: 'پکیج TOF (VL53L0X)', nameEn: 'VL53L0X (TOF)', desc: 'پکیج سنسور: فاصله‌سنج TOF — روی این برای فعال‌سازی', descEn: 'Time-of-flight pack', installed: true, kind: 'sensor' },
];

export const SENSOR_OLED = 'OLED';
export const SENSOR_TOF = 'TOF';
