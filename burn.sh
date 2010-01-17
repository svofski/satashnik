#!/bin/sh
kill -HUP `ps ax | grep -e "[0-9] screen.*usbserial" | sed 's/^ //' |  cut -f 1 -d ' '`

./mega8_bootload satashnik.bin /dev/tty.usbserial-A6007DJZ 19200
