# Smart Helmet Accident Detection and Emergency Alert System

**STM32F103C8T6 (Blue Pill) | Embedded C | STM32 HAL | MPU6050 | GPS | GSM**

> **Project status:** Educational breadboard prototype. This repository contains the application-level firmware and a prototype photograph, **not** the original complete STM32CubeIDE/CubeMX project. The firmware has not been rebuilt or hardware-verified from this package. This is **not** a certified safety device.

## Overview

This project explores a smart helmet accident-alert workflow using an STM32F103C8T6 microcontroller. The application checks a helmet-wear IR input, monitors MPU6050 acceleration for possible impact, sounds a buzzer, and provides a 15-second button cancellation period. If the warning is not cancelled, the firmware attempts to obtain GPS coordinates and send an emergency SMS using a SIM800L GSM module.

## Prototype photograph
![Hardware Prototype](Hardware/project%20o1.jpeg)

*Photograph of the team's breadboard-based electronics prototype. It does not establish helmet integration, wiring correctness, or completed functional testing.*

## Intended features

- Helmet-wear input using an IR sensor (active-low in the supplied code; verify on the actual module).
- MPU6050 accelerometer monitoring over I2C1.
- Impact detection using a 3.2 g acceleration-magnitude threshold and three consecutive samples.
- Audible warning and 15-second false-alarm cancellation window.
- GPS coordinates read from NEO-6M `$GPRMC` sentences over USART1.
- Emergency SMS via SIM800L AT commands over USART2, including a Google Maps link when a GPS fix is available.
- PC13 LED status indications for MPU6050 initialization and SMS outcome.

## Intended operating sequence

```text
Helmet wear detected (IR)
           |
           v
  Monitor acceleration
           |
    Threshold exceeded
   in 3 consecutive reads
           |
           v
   Buzzer / 15 s timer
           |
     +-----+-----+
     |           |
 Cancel button   No cancellation
     |           |
 Return to       v
 monitoring   Wait for GPS fix
                 |
                 v
             Send SMS
                 |
                 v
          Return to monitor
```

## Hardware described by the firmware

| Item | Intended role | Firmware interface |
|---|---|---|
| STM32F103C8T6 Blue Pill | Controller | STM32 HAL |
| MPU6050 | Accelerometer | I2C1, address `0x68` |
| IR proximity module | Helmet-wear input | PA0 |
| Active-high buzzer driver | Warning | PB12 |
| Push button with pull-up | Cancel warning | PB13 |
| NEO-6M GPS | Position | USART1 |
| SIM800L GSM | SMS | USART2 |
| Onboard LED | Status | PC13, active-low |

**Note:** These are the connections expected by the source, not a verified wiring diagram extracted from the photo. The actual supply rails, UART pin assignments, module power requirements, and ground connections must be checked before use. SIM800L requires a suitable high-current supply; do not assume it can be powered from a Blue Pill 3.3 V pin.

## Source files

- [`firmware/smart_helmet.c`](firmware/smart_helmet.c) — reconstructed plain-text version of the supplied application code, with original behavior preserved except minor formatting/defensive default-switch handling.
- [`firmware/smart_helmet.h`](firmware/smart_helmet.h) — reconstructed interface declarations.
- [`Documentation/SETUP.md`](Documentation/SETUP.md) — missing CubeMX configuration and integration steps.
- [`Documentation/TECHNICAL_NOTES.md`](Documentation/TECHNICAL_NOTES.md) — known limitations and validation checklist.

## Build status and integration

**This repository is not directly importable/buildable as an STM32CubeIDE project.** The original `.ioc`, `main.c`, startup files, linker script, generated HAL setup, and project metadata were not available. See [setup instructions](Documentation/SETUP.md) to recreate a CubeIDE project and integrate the application.

## Team and credits

Replace the placeholders below with the **actual** names, GitHub usernames, and responsibilities. Do not claim roles or commits that did not occur.

| Team member | GitHub profile
|---|---|---|
| `[KRISHNENDHU P J]` | `[krishnendhupj70] https://github.com/krishnendhupj70` | 
| `[SHIVNAND P P]` | `[shivnandpp] https://github.com/shivnandpp` | 
| `[AJIN JAMES ]` | `[ajin075] https://github.com/ajin075` | 



## Repository collaboration

The owner should invite all three teammates under **Settings → Collaborators → Add people**. Each person should accept the invitation and use their own Git identity for future commits. A shared README credit is distinct from GitHub's commit-history contributor statistics. Do not manufacture commit history or attribute code commits to teammates who did not make them.

## Disclaimer

This is a student engineering prototype, not a production-grade emergency-response system. Accident recognition can yield false positives or missed events, and GPS/GSM communication may fail. Never rely on it as the sole means of emergency notification.
