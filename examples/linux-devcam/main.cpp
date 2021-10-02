#include "platglue.h"

#include "SimStreamer.h"

#include "camerastreamer.h"
#include "CRtspSession.h"
#include <assert.h>
#include <sys/time.h>

#include "threads/iothread.h"
#include "net/tcp_server.h"

TCP_Server gServer;
LF::threads::IOThread gThread;

CStreamer* gStreamer;

/********** DEBUG SETUP **********/
#define ENABLE_SDEBUG
//#define DEBUG_PREFIX "rtspserver: "
#include "utils/screenlogger.h"
/*********************************/

void OnClientConnected()
{
    TCP_Socket* client = reinterpret_cast<TCP_Socket*>(gServer.NextPendingConnection());
    if (client)
    {
        SDEB("Client connected!");
        WiFiClient* wifiClient = new WiFiClient(client);
        gStreamer->addSession(*wifiClient);
    }
}

void Loop()
{
    //SDEB(">> %d", millis());
    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();

    // If we have an active client connection, just service that until gone
    gStreamer->handleRequests(0); // we don't use a timeout here,
    // instead we send only if we have new enough frames
    uint32_t now = millis();
    if(gStreamer->anySessionsStreaming())
    {
        if(now > lastimage + msecPerFrame || now < lastimage)
        {
            // handle clock rollover
            gStreamer->streamImage(now);
            lastimage = now;

            // check if we are overrunning our max frame rate
            now = millis();
            if(now > lastimage + msecPerFrame)
            {
                printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
            }
        }
    }
}

int main()
{
//    gStreamer = new SimStreamer(false);
    gStreamer = new CameraStreamer();

    gThread.SetLoopTimeout(100);
    CONNECT(gThread.TICK, Loop);

    gThread.Start();
    gServer.Open(8554);
    gThread.RegisterWaitable(&gServer);
    CONNECT(gServer.CLIENT_CONNECTED, OnClientConnected);

    gThread.Join();
}
