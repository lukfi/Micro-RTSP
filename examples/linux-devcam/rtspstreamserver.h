#ifndef RTSPSTREAMSERVER_H
#define RTSPSTREAMSERVER_H

#include "net/tcp_server.h"
#include "threads/iothread.h"

#include "CStreamer.h"

class RTSPStreamServer : public StreamSink
{
public:
    RTSPStreamServer(CStreamer* streamer, uint16_t serverPort = 8554);

private:
    virtual void StreamFrame(unsigned const char *data, uint32_t dataLen, uint32_t curMsec) override;
    virtual void StreamFrameThread(unsigned const char *data, uint32_t dataLen, uint32_t curMsec);

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
