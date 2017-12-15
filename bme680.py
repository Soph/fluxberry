import time
import os
import bme680
from influxdb import InfluxDBClient

influx_host = os.getenv("INFLUX_HOST")
port = 8086
dbname = "environment"
user = os.getenv("INFLUX_USER")
password = os.getenv("INFLUX_PASS")
host = os.getenv("PI_HOST")
sleep_time = os.getenv("SAMPLE_PAUSE")
i2c_address = int(os.getenv("BME680_I2C"),16) # 0x77

client = InfluxDBClient(influx_host, port, user, password, dbname, True, True)
client.create_database(dbname)

sensor = bme680.BME680(i2c_address)
sensor.set_humidity_oversample(bme680.OS_2X)
sensor.set_pressure_oversample(bme680.OS_4X)
sensor.set_temperature_oversample(bme680.OS_8X)
sensor.set_filter(bme680.FILTER_SIZE_3)
sensor.set_gas_status(bme680.ENABLE_GAS_MEAS)

def get_cpu_temp():
    path="/sys/class/thermal/thermal_zone0/temp"
    f = open(path, "r")
    temp_raw = int(f.read().strip())
    temp_cpu = float(temp_raw / 1000.0)
    return temp_cpu

def get_data_points():
    if sensor.get_sensor_data():
        #if not sensor.data.heat_stable:
        #    return []

        temp_cpu = get_cpu_temp()
        iso = time.ctime()

        json_body = [
            {
                "measurement": "ambient_celcius",
                "tags": {"host": host},
                "time": iso,
                "fields": {
                    "value": sensor.data.temperature,
                    "val": float(sensor.data.temperature)
                }
            },
            {
                "measurement": "cpu_celcius",
                "tags": {"host": host},
                "time": iso,
                "fields": {
                    "value": temp_cpu,
                }
            },
            {
                "measurement": "ambient_humidity",
                "tags": {"host": host},
                "time": iso,
                "fields": {
                    "value": sensor.data.humidity,
                }
            },
            {
                "measurement": "ambient_pressure",
                "tags": {"host": host},
                "time": iso,
                "fields": {
                    "value": sensor.data.pressure,
                }
            },
            {
                "measurement": "ambient_gas_resistance",
                "tags": {"host": host},
                "time": iso,
                "fields": {
                    "value": sensor.data.gas_resistance,
                }
            }

        ]

        return json_body

    else:
        return []

try:
    sleep_duration = float(sleep_time)
    while True:
        json_body = get_data_points()
        if len(json_body) > 0:
            client.write_points(json_body)
            print (".")
            time.sleep(sleep_duration)
        else:
            print ("waiting for heat stable")
            time.sleep(1)

except KeyboardInterrupt:
    pass