#pragma once

#include "platglue.h"
#include "LinkedListElement.h"
#include "JPEGAnalyse.h"

// forward declaration
class CRtspSession;

class StreamSink
{
public:
    virtual void StreamFrame(unsigned const char *data, uint32_t dataLen, uint32_t curMsec) = 0;
};

class StreamerMetrics
{
public:
    void DecodeStart();
    void DecodeStop();

    void Report(uint32_t dimeDiff);
private:
    void Reset();

    uint32_t mDecodeStartTime { 0 };
    uint32_t jpegDecodeCount { 0 };
    uint32_t jpegDecodeTimeAcc { 0 };
    uint32_t jpegDecodeTimeMin { 0xffffffff };
    uint32_t jpegDecodeTimeMax { 0 };

    uint32_t mReportTime;
};

class CStreamer
{
public:
    CStreamer();
    virtual ~CStreamer();

    void SetSink(StreamSink* sink)
    {
        mSink = sink;
    }

    CRtspSession* addSession(WiFiClient& aClient);
    LinkedListElement* getClientsListHead() { return &m_Clients; }

    int anySessions() { return m_Clients.NotEmpty(); }
    bool anySessionsStreaming();

    virtual bool handleRequests(uint32_t readTimeoutMs);

    u_short GetRtpServerPort();
    u_short GetRtcpServerPort();

    virtual void streamImage(uint32_t curMsec) = 0; // send a new image to the client
    bool InitUdpTransport(void);
    void ReleaseUdpTransport(void);

    void streamFrame(unsigned const char *data, uint32_t dataLen, uint32_t curMsec);

protected:
    StreamSink* mSink { nullptr };

private:
    int SendRtpPacket(const DecodedImageInfo &info, uint32_t fragmentOffset);// returns new fragmentOffset or 0 if finished with frame

    UDPSOCKET m_RtpSocket;       // RTP socket for streaming RTP packets to client
    UDPSOCKET m_RtcpSocket;      // RTCP socket for sending/receiving RTCP packages

    IPPORT m_RtpServerPort;      // RTP sender port on server
    IPPORT m_RtcpServerPort;     // RTCP sender port on server

    u_short m_SequenceNumber;
    uint32_t m_Timestamp;
    int m_SendIdx;

    LinkedListElement m_Clients;
    uint32_t m_prevMsec;

    int m_udpRefCount;

    StreamerMetrics mMetrics;
};

