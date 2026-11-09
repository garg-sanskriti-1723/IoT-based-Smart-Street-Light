# IoT-Based Smart Street Light System

An ESP32-based smart street lighting system designed to improve energy efficiency through automatic light control and vehicle detection.

## Project Overview

This project uses an LDR sensor to detect ambient light conditions and IR sensors to identify passing vehicles. Based on these inputs, the system adjusts street light brightness automatically, providing brighter illumination when vehicles are detected and reducing brightness when the road is inactive.

The project also includes a Wi-Fi-based monitoring dashboard to display sensor readings and system information.

## Key Features

- **Automatic Day/Night Detection:** Uses an LDR sensor to detect ambient light conditions.
- **Vehicle Detection:** Uses IR sensors to detect passing vehicles.
- **Automatic Brightness Control:** Adjusts LED brightness based on vehicle detection.
- **Vehicle Counting:** Tracks detected vehicles.
- **Speed Estimation:** Estimates vehicle speed using sensor detection timing.
- **Web-Based Monitoring:** Displays system status and sensor information through a web dashboard.
- **Energy Monitoring:** Provides estimated energy consumption and savings.

## Hardware Components

- ESP32 development board
- LDR sensor
- IR sensors
- LED lights
- Connecting wires
- Breadboard and supporting components

## Software and Technologies

- Arduino IDE
- Embedded C/C++
- ESP32
- Wi-Fi
- HTML (for the monitoring dashboard)

## How It Works

1. The LDR sensor detects whether the surroundings are bright or dark.
2. During nighttime, the street lights operate at reduced brightness.
3. IR sensors detect vehicles passing through the monitored area.
4. When a vehicle is detected, the corresponding street light increases its brightness.
5. Vehicle count and estimated speed are displayed on the monitoring dashboard.

## Project Status

Developed as an academic microcontroller project. The system demonstrates sensor-based automation, LED brightness control and Wi-Fi-based monitoring.

**Note:** Energy savings are estimated using programmed assumptions, and vehicle speed is an approximate calculation based on sensor timing and spacing.

## Repository Contents

- `IoTbasedStreetLight.ino` – Main Arduino program.

## Future Improvements

- Improve vehicle speed measurement accuracy.
- Add real-time data logging.
- Introduce remote monitoring and analytics.
- Improve fault detection and system reliability.
