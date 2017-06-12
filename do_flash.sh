#!/bin/bash

# http://catherineh.github.io/programming/2016/04/15/arduino-builder-errors

~/arduino-1.8.2/arduino --verbose --board teensy:avr:teensy31:speed=72,usb=midi,keys=en-us,opt=osstd --upload turboencabulator/turboencabulator.ino
