#ifndef RTSPSTREAMSERVER_H
#define RTSPSTREAMSERVER_H

#include "net/tcp_server.h"
#include "threads/iothread.h"

class CStreamer;

class RTSPStreamServer
{
public:
    RTSPStreamServer(CStreamer* streamer, uint16_t serverPort = 8554);

private:
    CStreamer* mStreamer;

    LF::net::SocketMaster* mRTSPServer;
    //TCP_Server* mRTSPServer;
    LF::threads::IOThread mThread;
    LF::threads::IOThread mCleanupThread;

    void OnNewClient(LF::net::SocketMaster *, ConnectionSocket *);
    void OnClientRead(LF::net::SocketMaster *);
    void OnClientDisconnected(LF::net::SocketMaster *sock);

    uint32_t mClientCount { 0 };
};

#endif // RTSPSTREAMSERVER_H
