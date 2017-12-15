#!/bin/bash

export INFLUX_HOST=
export INFLUX_USER=
export INFLUX_PASS=
export PI_HOST=test
export SAMPLE_PAUSE=60
export BME680_I2C=0x77

python process_bme680.py
