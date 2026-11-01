# VideoLauncher

Aplikacja na radio samochodowe z Androidem do sterowania ARGB LED i wyswietlania statystyk PC.

## Komponenty

### 1. Aplikacja Android (`app/`)
- Launcher z widgetami na pulpicie
- **ARGB Control** - sterowanie paskami LED przez USB (STM32)
- **PC Stats** - wyswietlanie statystyk CPU/GPU/RAM z komputera

### 2. STM32 ARGB Controller (`stm32_argb/`)
Firmware na STM32F103C8T6 (Blue Pill) do sterowania paskami WS2812B/SK6812.

**Funkcje:**
- 3 niezalezne strefy LED (zewnetrzna, wewnetrzna, wentylator)
- 13 efektow swietlnych (solid, rainbow, breathing, fire, scanner, chase, fill, police, wave, colorwipe, gradient, aurora, plasma)
- 8 wariantow kolorow
- Regulacja jasnosci i predkosci
- Sterowanie PWM wentylatora
- Przelacznik audio A/B

**Podlaczenie:**
```
PA0  - LED zewn. (144 LED)
PA1  - LED wewn. (60 LED)
PA6  - LED wentylator (16 LED)
PA8  - PWM wentylator
PB0  - Przekaznik audio A/B
PB12 - Przycisk 1 (audio A/B)
PB13 - Przycisk 2 (sterowanie LED)
USB  - Komunikacja z radiem
```

**Fizyczne przyciski:**
- **Przycisk 1 (PB12)** - przelacza przekaznik audio A/B
- **Przycisk 2 (PB13)** - wielofunkcyjny:
  - 1 klik: zmienia tryb/efekt
  - 2 klik: zmienia predkosc
  - 3 klik: zmienia jasnosc
  - 4 klik: zmienia kolor efektu
  - 5 klik: selektor strefy (zewn/wewn/wentylator)
  - 6 klik: auto mode (zmienia efekt co 10s)

### 3. PC Stats Sender (`pc_stats_sender/`)
Skrypt Python wysylajacy statystyki PC przez UDP do aplikacji Android.

**Wysylane dane:**
- CPU usage %
- GPU usage % (NVIDIA)
- RAM usage %
- GPU temp
- Network speed

## Instalacja

### PC Stats Sender (na PC z Windows)

1. Zainstaluj Python 3.10+ z python.org
2. Skopiuj folder `pc_stats_sender` na docelowy komputer
3. Uruchom `install.bat`

Skrypt automatycznie:
- Zainstaluje zaleznosci (psutil, pynvml, wmi)
- Doda autostart przy uruchomieniu Windows
- Uruchomi wysylanie statystyk

**Deinstalacja:** uruchom `uninstall.bat`

### STM32 Firmware

**Wymagania:**
- Arduino CLI
- STM32duino core
- OpenOCD + ST-Link

**Wgrywanie:**
```
cd stm32_argb
flash_v2.bat
```

### Aplikacja Android

**Budowanie:**
```
gradlew assembleDebug
```

**Instalacja przez ADB:**
```
adb install app/build/outputs/apk/debug/app-debug.apk
```

## Uzycie

1. Podlacz STM32 przez USB do radia Android
2. Uruchom PC Stats Sender na komputerze (w tej samej sieci WiFi co radio)
3. Otworz aplikacje VideoLauncher na radiu
4. Uzyj widgetu ARGB Control do sterowania LED
5. Widget PC Stats wyswietla statystyki komputera

## Protokol komunikacji

### USB (ARGB)
Format: `ZONA:PARAMETR:WARTOSC`
```
E:FX:5      - efekt 5 na zewn.
I:BR:200    - jasnosc 200 na wewn.
F:SP:7      - predkosc 7 na wentylatorze
F:PWM:2     - tryb PWM wentylatora (0=cichy, 1=norm, 2=max, 3=reczny)
F:PWM:3:75  - reczny PWM 75%
S:AB:1      - przelacz audio na B
```

### UDP (PC Stats)
Port: 5556
Format JSON:
```json
{"cpu": 45.2, "gpu": 30.0, "ram": 65.1, "gpu_temp": 55, "net": 1.2}
```

## Licencja

MIT
