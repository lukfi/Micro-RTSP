#pragma once

#include "platglue.h"
#include "LinkedListElement.h"

typedef unsigned const char *BufPtr;

struct DecodedImageInfo
{
    uint32_t width;
    uint32_t height;
    BufPtr qTable0;
    BufPtr qTable1;
    BufPtr jpegPayload;
    uint32_t payloadLen;
    uint32_t type; // as described in rfc2435 [4.1. The Type Field]
};

class CRtspSession;

class CStreamer
{
public:
    CStreamer();
    virtual ~CStreamer();
#ifdef USE_LFF
    void addSession()
    {

    }
#endif
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

protected:
    void streamFrame(unsigned const char *data, uint32_t dataLen, uint32_t curMsec);

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
};

// When JPEG is stored as a file it is wrapped in a container
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped
// returns true if the file seems to be valid jpeg
// If quant tables can be found they will be stored in qtable0/1
bool decodeJPEGfile(BufPtr start, uint32_t len, DecodedImageInfo& info);
bool findJPEGheader(BufPtr *start, uint32_t *len, uint8_t marker);

///
/// \brief findJPEGHeader
/// \param startData - pointer to data to search the marker, if marker found - points to the marker start byte
/// \param dataLen - size of the data startData points at (after the header)
/// \param marker - marker to search
/// \param length - marker length
/// \return - pointer to the marker data
///
BufPtr findJPEGheader2(const BufPtr startData, uint32_t dataLen, uint8_t marker, uint32_t& length);

// Given a jpeg ptr pointing to a pair of length bytes, advance the pointer to
// the next 0xff marker byte
void nextJpegBlock(BufPtr *start);
