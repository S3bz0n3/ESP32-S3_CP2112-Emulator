# ESP32-S3 CP2112 Emulator

Dieses Projekt emuliert einen **Silicon Labs CP2112 HID-to-SMBus-Adapter** mit
einem ESP32-S3. Dadurch kann die **DJI Battery Killer GUI von mixeysan** mit
einem ESP32-S3 kommunizieren, als wäre ein echter CP2112 angeschlossen.

Der Sketch ist für die Kommunikation mit dem DJI-BMS vorgesehen, zum Beispiel
zum Zurücksetzen eines DJI-Spark-Akkus. Der ESP32-S3 übernimmt dabei die
USB-HID-Schnittstelle des CP2112 und reicht die SMBus-/I²C-Transfers an das
angeschlossene BMS weiter.

Das Projekt wurde erfolgreich mit Akkus der **DJI Spark** getestet. Dabei kam
die DJI Battery Killer GUI mit dem Versionshinweis **„compiled 13.06.2021“**
zum Einsatz.

## Funktionen

- USB-HID-Emulation des CP2112
- CP2112-kompatible USB-Kennung: `VID 10C4`, `PID EA90`
- SMBus-/I²C-Transfers für Lesen, Schreiben und Schreiben/Lesen
- Konfigurierbare Busgeschwindigkeit über die HID-Kommandos der GUI
- Serielle Debug-Ausgaben für USB-, I²C- und Transferstatus

## Hardware und Verdrahtung

| ESP32-S3 | Funktion |
| --- | --- |
| GPIO8 | I²C/SMBus SDA |
| GPIO9 | I²C/SMBus SCL |
| GND | Gemeinsame Masse mit dem BMS |

Die Pins sind im Sketch festgelegt:

```cpp
#define I2C_SDA 8
#define I2C_SCL 9
```

Der Standardtakt beträgt `100 kHz`. SDA und SCL benötigen geeignete Pull-up-
Widerstände auf die zulässige Signalspannung des angeschlossenen BMS. Der
ESP32-S3 darf nicht direkt mit einer höheren Spannung als seiner zulässigen
GPIO-Spannung verbunden werden. Bei abweichenden Pegeln ist ein passender
Pegelwandler zu verwenden.

## Voraussetzungen

- ESP32-S3-Board mit nutzbarem nativen USB-Anschluss
- Arduino IDE oder Arduino CLI
- ESP32 Arduino Core **2.0.17**
- **TinyUSB** für die native USB-HID-Kommunikation
- DJI Battery Killer GUI von **mixeysan**
- USB-Datenkabel

`USB.h`, `USBHID.h`, `Wire.h` und `Arduino.h` werden vom ESP32-Arduino-Core
bereitgestellt. Die USB-HID-Funktion nutzt TinyUSB; bei einer Installation des
ESP32-Arduino-Cores 2.0.17 sind dafür normalerweise keine weiteren Bibliotheken
erforderlich.

## Installation und Flashen

1. Dieses Repository herunterladen oder klonen.
2. Die Datei `DJI_Battery_Killer_ESP32-S3-CP2112_Emulator.ino` in der Arduino
	 IDE öffnen.
3. Als Board das verwendete ESP32-S3-Board auswählen.
4. Den richtigen USB-Port auswählen.
5. Den Sketch kompilieren und auf den ESP32-S3 flashen.
6. Den ESP32-S3 nach dem Flashen per USB mit dem Rechner verbinden.

Nach dem Start sollte das Gerät als CP2112-kompatibles HID-Gerät erscheinen.
Die DJI Battery Killer GUI kann anschließend dieses USB-Gerät für die
Kommunikation mit dem BMS verwenden.

## Serielle Diagnose

Der Sketch schreibt Diagnosemeldungen mit `115200 Baud` über die serielle
Schnittstelle. Beim Start werden unter anderem die I²C-Pins und die USB-
Kennung ausgegeben. Zusätzlich werden HID-Reports sowie Lese-, Schreib- und
Statusoperationen protokolliert.

Wenn die Diagnose nicht benötigt wird, kann sie im Sketch über folgende Zeile
deaktiviert werden:

```cpp
#define DEBUG_SERIAL 0
```

## Hinweise zur Verwendung

- Die I²C-/SMBus-Adresse und weitere SMBus-Parameter werden normalerweise von
	der GUI über die CP2112-HID-Kommandos gesetzt.
- Der ESP32-S3 muss während der Kommunikation stabil über USB versorgt sein.
- USB-HID-Verarbeitung und Transferstatus werden vom Sketch kontinuierlich
	bearbeitet; der Haupt-Loop sollte deshalb nicht mit langen Verzögerungen
	erweitert werden.

## Sicherheit

Arbeiten an DJI-Akkus und deren BMS können Kurzschluss-, Brand- und
Beschädigungsgefahr verursachen. Nur mit geeigneter Strombegrenzung, isolierter
Verdrahtung und ausreichendem Fachwissen arbeiten. Akku und BMS niemals
unbeaufsichtigt betreiben und die Pinbelegung des konkreten Akkus vor dem
Anschluss prüfen.

## Lizenz

In diesem Repository ist derzeit keine separate Lizenzdatei enthalten. Bitte
vor einer Weitergabe die Lizenzbedingungen des ursprünglichen Projekts und der
DJI Battery Killer GUI von mixeysan prüfen.
