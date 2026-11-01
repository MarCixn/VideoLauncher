=====================================================
  STM32 ARGB Controller for Video Launcher
=====================================================

HARDWARE:
---------
STM32F103C8T6 (Blue Pill)

PINOUT:
-------
PA7  -> DATA pin tasmy WS2812B / ARGB
5V   -> +5V tasmy LED (lub zewnetrzne zasilanie)
GND  -> GND tasmy LED
PA0  -> (opcjonalnie) wejscie audio dla beat detection

USB  -> do radia Android (kabel USB-A do Micro-USB)


SCHEMAT POLACZENIA:
-------------------

    STM32 Blue Pill              Tasma WS2812B / ARGB
    +-------------+              +------------------+
    |             |              |                  |
    |         PA7 |------------->| DIN (Data In)    |
    |             |              |                  |
    |          5V |--+---------->| +5V              |
    |             |  |           |                  |
    |         GND |--+---------->| GND              |
    +-------------+  |           +------------------+
                     |
                     +-- Zewnetrzne zasilanie 5V
                         (dla wiecej niz 30 LED)


WGRYWANIE FIRMWARE:
-------------------
1. Zainstaluj PlatformIO (VSCode extension)
2. Otworz folder stm32_argb w VSCode
3. Podlacz ST-Link lub uzyj UART bootloader
4. Kliknij "Upload" w PlatformIO

Alternatywnie przez UART:
- Ustaw BOOT0 na HIGH (jumper)
- Podlacz USB-UART (TX->PA10, RX->PA9)
- Wgraj przez STM32CubeProgrammer


PROTOKOL KOMUNIKACJI:
--------------------
Komendy przez USB Serial (115200 baud):

COLOR:R:G:B       - ustaw kolor (0-255)
EFFECT:id:speed   - ustaw efekt (0-7) i predkosc (0-255)
BRIGHT:level      - jasnosc (0-255)
NUMLEDS:count     - ilosc LED (1-144)
BEAT:intensity    - beat detection (0-255)
PING              - test polaczenia

Efekty:
  0 = Wylaczony
  1 = Staly kolor
  2 = Tecza
  3 = Pulsowanie
  4 = Fala
  5 = Ogien
  6 = Beat (reaguje na BEAT:)
  7 = Animowany gradient


WIATRAK ARGB (Krux Corvus):
---------------------------
Wiatraki PC ARGB maja 3-pinowe zlacze:
- +5V (czerwony/bialy)
- DATA (zielony/zolty)  -> PA7
- GND (czarny)

Odetnij oryginalne zlacze i podlacz do STM32.


DODATKOWE UWAGI:
----------------
- Dla >30 LED uzyj zewnetrznego zasilania 5V
- Dodaj kondensator 100-1000uF przy zasilaniu LED
- Rezystor 330-470 ohm na linii DATA (opcjonalnie)
- STM32 moze wymagac 3.3V->5V level shifter dla dlugich kabli
