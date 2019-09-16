#include "ArduinoHttpClient.h"
#include "EthernetClient.h"

#include "FlashFs.h"
#define CHUNK_SIZE 1024

int download(const char *host, uint16_t port, char *url, char *filename)
{
    EthernetClient client;
    HttpClient http(client, host, port);

    LWIP_DEBUGF(TCPIP_DEBUG, ("[HTTP] begin...\n"));
    // start connection and send HTTP header
    int httpCode = http.get(url);
    // HTTP header has been send and Server response header has been handled
    LWIP_DEBUGF(TCPIP_DEBUG, ("[HTTP] GET... code: %d\n", httpCode));

    // file found at server
    if (httpCode == HTTP_SUCCESS)
    {

        // get lenght of document (is -1 when Server sends no Content-Length header)
        int downloadRemaining = http.contentLength();
        core_debug("[HTTP] GET...size=%d\n", downloadRemaining);
        // create buffer for read
        uint8_t cur_buffer[CHUNK_SIZE];
        bool success = true;
        FlashFile fil(filename, SPIFFS_O_CREAT | SPIFFS_WRONLY);
        // read all data from server
        while (success && http.connected() && (downloadRemaining > 0))
        {
            // get available data size

            auto size = http.available();
            if (size > 0)
            {
                auto c = http.read(cur_buffer, ((size > CHUNK_SIZE) ? CHUNK_SIZE : size));
                fil.write((const char *)cur_buffer, c);

                success &= c > 0;
                if (downloadRemaining > 0)
                {
                    downloadRemaining -= c;
                }
                core_debug("recv=%d,remain=%d,success=%d\n", c, downloadRemaining,success);
            }
            yield();
        }
        fil.close();

        core_debug("[HTTP] connection closed or file end.\n");
        return 0;
    }
    else
    {
        core_debug("[HTTP] GET... failed, error: %d\n", httpCode);
    }
    http.stop();
    return 1;
}