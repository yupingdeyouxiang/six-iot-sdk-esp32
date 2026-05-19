# Creeate the nvs-creds.bin to hold the private key and device GUID which is specific to the device itself, the info should not be changed during the OTA process.

## For default MQTT broker
python D:/Espressif/frameworks/esp-idf-v5.5.1/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs-creds-default.csv nvs-creds.bin 0x1000

## For AWS MQTT broker
python D:/Espressif/frameworks/esp-idf-v5.5.1/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs-creds-aws.csv nvs-creds.bin 0x1000


# Erase the flash
esptool.py --port COM4 erase_flash

# Flash the creds binary to the flash, 0XA000 must match the start address of the NVS partition in the partition table
esptool.py --port COM4 write_flash 0xA000 nvs-creds.bin


# Select the MQTT broker 
wss://a2o5o645mb29bc-ats.iot.ap-southeast-1.amazonaws.com/mqtt
wss://shuhenglianchang.com:30084/mqtt


