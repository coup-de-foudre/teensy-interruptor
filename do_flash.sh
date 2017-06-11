#!/bin/bash

~/arduino-1.8.2/arduino --verbose --board teensy:avr:teensy31:speed=72 --verify turboencabulator/turboencabulator.ino

read -p "Press enter to continue"

~/arduino-1.8.2/arduino --verbose --board teensy:avr:teensy31:speed=72 --flash turboencabulator/turboencabulator.ino
