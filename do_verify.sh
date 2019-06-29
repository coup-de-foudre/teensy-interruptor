#!/bin/bash

~/arduino-1.8.9/arduino --verbose --board teensy:avr:teensy31:speed=72,usb=midi,keys=en-us,opt=osstd --verify interocitor/interocitor.ino
