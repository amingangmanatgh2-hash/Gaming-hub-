// Board catalog. Compile target is the ATmega328P core bundled in the
// offline toolchain (Uno / Nano / Mini share the same MCU).
export interface Board {
  id: string;
  name: string;     // fa
  nameEn: string;
  chip: string;
  mhz: number;
  flash: number;
  sram: number;
  note?: string;    // fa tooltip
}

export const COMPILE_BOARDS: Board[] = [
  { id: 'uno', name: 'Arduino Uno R3', nameEn: 'Arduino Uno R3', chip: 'ATmega328P', mhz: 16, flash: 32768, sram: 2048 },
  { id: 'nano', name: 'Arduino Nano', nameEn: 'Arduino Nano', chip: 'ATmega328P', mhz: 16, flash: 32768, sram: 2048, note: 'همان خانواده Uno (ATmega328P)' },
  { id: 'mini', name: 'Arduino Mini', nameEn: 'Arduino Mini', chip: 'ATmega328P', mhz: 16, flash: 32768, sram: 2048 },
  { id: 'fio', name: 'Arduino Fio', nameEn: 'Arduino Fio', chip: 'ATmega328P', mhz: 8, flash: 32768, sram: 2048 },
  { id: 'promini3', name: 'Pro Mini 3.3V', nameEn: 'Pro Mini 3.3V', chip: 'ATmega328P', mhz: 8, flash: 32768, sram: 2048 },
];

// boards that need other cores — shown in the manager as «آینده» (soon)
export interface BoardSoon { name: string; nameEn: string; chip: string; }
export const BOARDS_SOON: BoardSoon[] = [
  { name: 'Arduino Mega 2560', nameEn: 'Arduino Mega 2560', chip: 'ATmega2560' },
  { name: 'Arduino Leonardo', nameEn: 'Arduino Leonardo', chip: 'ATmega32U4' },
  { name: 'Arduino Micro', nameEn: 'Arduino Micro', chip: 'ATmega32U4' },
  { name: 'ESP32', nameEn: 'ESP32', chip: 'ESP32' },
  { name: 'ESP8266 NodeMCU', nameEn: 'ESP8266 NodeMCU', chip: 'ESP8266' },
  { name: 'Arduino Due', nameEn: 'Arduino Due', chip: 'ATSAM3X8E' },
];

export function boardById(id: string): Board | undefined {
  return COMPILE_BOARDS.find((b) => b.id === id);
}
