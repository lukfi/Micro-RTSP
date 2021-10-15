#include "rtspstreamserver.h"
#include "CStreamer.h"
#include "CRtspSession.h"

#include "net/socketmaster.h"
#include "utils/stringutils.h"

/********** DEBUG SETUP **********/
#define ENABLE_SDEBUG
#define DEBUG_PREFIX "RTSPStreamServer: "
#include "utils/screenlogger.h"
/*********************************/

RTSPStreamServer::RTSPStreamServer(CStreamer *streamer, uint16_t serverPort) :
    mStreamer(streamer),
    mThread("RTSPStreamServer"),
    mCleanupThread("CleanupThread")
{
    mRTSPServer = new LF::net::SocketMaster(SocketServerType_t::TCPServer, &mThread, false);
    mRTSPServer->Listen(serverPort);

//    mThread.RegisterWaitable(mRTSPServer);
    CONNECT(mRTSPServer->SM_CLIENT_CONNECTED, RTSPStreamServer, OnNewClient);

    mThread.Start();
    mCleanupThread.Start();
}

void RTSPStreamServer::OnNewClient(LF::net::SocketMaster*, ConnectionSocket* client)
{
    if (client)
    {
        SINFO("Client connected!");

        client->SetWaitingFlags(true, true);
        std::string socketName;
        LF::utils::sformat(socketName, "Socket:%d", ++mClientCount);
        client->SetName(socketName);
        LF::net::SocketMaster* clientMaster = new LF::net::SocketMaster(reinterpret_cast<TCP_Socket*>(client),
                                                                        &mThread);
        WiFiClient* wifiClient = new WiFiClient(clientMaster);
        CRtspSession* newSession = mStreamer->addSession(*wifiClient);
        clientMaster->SetUserData(newSession);

        CONNECT(clientMaster->SM_READ, RTSPStreamServer, OnClientRead);
        CONNECT(clientMaster->SM_ERROR, RTSPStreamServer, OnClientDisconnected);

        SCHEDULE_TASK(&mThread, &RTSPStreamServer::OnClientRead, this, clientMaster);
    }
}

void RTSPStreamServer::OnClientRead(LF::net::SocketMaster *)
{
    SDEB("OnClientRead");
    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();

    // If we have an active client connection, just service that until gone
    mStreamer->handleRequests(0); // we don't use a timeout here,

    // instead we send only if we have new enough frames
//    uint32_t now = millis();
//    if (mStreamer->anySessionsStreaming())
//    {
//        if (now > lastimage + msecPerFrame || now < lastimage)
//        {
//            // handle clock rollover
//            mStreamer->streamImage(now);
//            lastimage = now;

//            // check if we are overrunning our max frame rate
//            now = millis();
//            if(now > lastimage + msecPerFrame)
//            {
//                printf("warning exceeding max frame rate of %d ms\n", now - lastimage);
//            }
//        }
    //    }
}

void RTSPStreamServer::OnClientDisconnected(LF::net::SocketMaster* sock)
{
    CRtspSession* session = reinterpret_cast<CRtspSession*>(sock->GetUserData());
    if (session)
    {
        session->m_stopped = true;
    }

    SDEB("Client disconnected, streaming: %d", mStreamer->anySessionsStreaming());
    mCleanupThread.DeleteLater(sock);

    mStreamer->handleRequests(0);
}
