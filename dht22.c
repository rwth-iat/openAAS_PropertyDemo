#include <stdio.h>
#include "pi_dht_read.h"
 
int main()
{
  int pin;
  pin = 4;
  float humidity, temperature = 0.0f;
  int result = pi_dht_read(DHT22, pin, &humidity, &temperature);
  printf("%d %f %f\n", result, humidity, temperature);
  return 0;
}
