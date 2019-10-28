/*
  Web Server

 A simple web server that shows the value of the analog input pins.

 Circuit:
 * STM32 board with Ethernet support
 * Analog inputs attached to pins A0 through A5 (optional)

 created 18 Dec 2009
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 modified 02 Sept 2015
 by Arturo Guadalupi
 modified 23 Jun 2017
 by Wi6Labs
 modified 1 Jun 2018
 by sstaub
 */
#include <Arduino.h>
#include <LwIP.h>
#include <STM32Ethernet.h>
#include <core_debug.h>
extern "C"
{
  void httpd_init(void);
}
// Enter an IP address for your controller below.
// The IP address will be dependent on your local network:

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
//EthernetServer server(80);
void web_init();
uint8_t ip[4];
#if LWIP_DHCP
uint8_t enDhcp = 0;
#endif
void httpd_setup()
{
  // start the Ethernet connection and the server:
#if LWIP_DHCP
  if (enDhcp)
    Ethernet.begin();
  else
#endif
  {
    IPAddress ipp(192, 168, 31, 156);
    Ethernet.begin(ipp);
  }
  httpd_init();
  web_init();
}
