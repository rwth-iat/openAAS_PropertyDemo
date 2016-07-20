PLATFORM_PREFIX = ./external/Adafruit_Python_DHT/source/Raspberry_Pi/

all:
	gcc -std=gnu99 -Wall -I $(PLATFORM_PREFIX) dht22.c $(PLATFORM_PREFIX)pi_dht_read.c $(PLATFORM_PREFIX)pi_mmio.c $(PLATFORM_PREFIX)../common_dht_read.c -o dht22
        
clean:
	rm dht22
