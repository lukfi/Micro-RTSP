#include "CStreamer.h"
#include "CRtspSession.h"

#include <stdio.h>

CStreamer::CStreamer() :
    m_Clients()
{
    printf("Creating TSP streamer\n");
    m_RtpServerPort  = 0;
    m_RtcpServerPort = 0;

    m_SequenceNumber = 0;
    m_Timestamp      = 0;
    m_SendIdx        = 0;

    m_RtpSocket = NULLSOCKET;
    m_RtcpSocket = NULLSOCKET;

    m_prevMsec = 0;

    m_udpRefCount = 0;
};

CStreamer::~CStreamer()
{
    LinkedListElement* element = m_Clients.m_Next;
    CRtspSession* session = NULL;
    while (element != &m_Clients)
    {
        session = static_cast<CRtspSession*>(element);
        element = element->m_Next;
        delete session;
    }
};

CRtspSession* CStreamer::addSession(WiFiClient& aClient)
{
    // printf("CStreamer::addSession\n");
    CRtspSession* session = new CRtspSession(aClient, this); // our threads RTSP session and state
    return session;
    // we have it stored in m_Clients
}

bool CStreamer::anySessionsStreaming()
{
    LinkedListElement* element = m_Clients.m_Next;
    CRtspSession* session = NULL;
    while (element != &m_Clients)
    {
        session = static_cast<CRtspSession*>(element);
        if (session->m_streaming)
        {
            return true;
        }
        element = element->m_Next;
    }
    return false;
}

int CStreamer::SendRtpPacket(const DecodedImageInfo& info, uint32_t fragmentOffset)
{
    // printf("CStreamer::SendRtpPacket offset: %d - begin\n", fragmentOffset);
    const int KRtpHeaderSize = 12;           // size of the RTP header
    const int KJpegHeaderSize = 8;           // size of the special JPEG payload header

    const int MAX_FRAGMENT_SIZE = 508;     // FIXME, pick more carefully

    int fragmentLen = MAX_FRAGMENT_SIZE;
    if (fragmentLen + fragmentOffset > info.payloadLen) // Shrink last fragment if needed
    {
        fragmentLen = info.payloadLen - fragmentOffset;
    }

    bool isLastFragment = (fragmentOffset + fragmentLen) == info.payloadLen;

    if (!m_Clients.NotEmpty())
    {
        return isLastFragment ? 0 : fragmentOffset;
    }

    // Do we have custom quant tables? If so include them per RFC

    bool includeQuantTbl = info.qTable0 /*&& info.qTable1*/ && fragmentOffset == 0;
    int numOfQuantTbl = includeQuantTbl ? (info.qTable1 ? 2 : 1) : 0;

    uint8_t q = includeQuantTbl ? 128 : 0x5e;

    static char RtpBuf[2048]; // Note: we assume single threaded, this large buf we keep off of the tiny stack
    int RtpPacketSize = fragmentLen + KRtpHeaderSize + KJpegHeaderSize + (includeQuantTbl ? (4 + 64 * numOfQuantTbl) : 0);

    memset(RtpBuf,0x00,sizeof(RtpBuf));
    // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
    RtpBuf[0]  = '$';        // magic number
    RtpBuf[1]  = 0;          // number of multiplexed subchannel on RTPS connection - here the RTP channel
    RtpBuf[2]  = (RtpPacketSize & 0x0000FF00) >> 8;
    RtpBuf[3]  = (RtpPacketSize & 0x000000FF);
    // Prepare the 12 byte RTP header
    RtpBuf[4]  = 0x80;                                  // RTP version
    RtpBuf[5]  = 0x1a | (isLastFragment ? 0x80 : 0x00); // JPEG payload (26) and marker bit
    RtpBuf[7]  = m_SequenceNumber & 0x0FF;              // each packet is counted with a sequence counter
    RtpBuf[6]  = m_SequenceNumber >> 8;
    RtpBuf[8]  = (m_Timestamp & 0xFF000000) >> 24;      // each image gets a timestamp
    RtpBuf[9]  = (m_Timestamp & 0x00FF0000) >> 16;
    RtpBuf[10] = (m_Timestamp & 0x0000FF00) >> 8;
    RtpBuf[11] = (m_Timestamp & 0x000000FF);
    RtpBuf[12] = 0x13;                                  // 4 byte SSRC (sychronization source identifier)
    RtpBuf[13] = 0xf9;                                  // we just an arbitrary number here to keep it simple
    RtpBuf[14] = 0x7e;
    RtpBuf[15] = 0x67;

    // Prepare the 8 byte payload JPEG header
    RtpBuf[16] = 0x0;                                // type specific
    RtpBuf[17] = (fragmentOffset & 0x00FF0000) >> 16; // 3 byte fragmentation offset for fragmented images
    RtpBuf[18] = (fragmentOffset & 0x0000FF00) >> 8;
    RtpBuf[19] = (fragmentOffset & 0x000000FF);

    /* These sampling factors indicate that the chrominance components of
       type 0 video is downsampled horizontally by 2 (often called 4:2:2)
       while the chrominance components of type 1 video are downsampled both
       horizontally and vertically by 2 (often called 4:2:0). */
    RtpBuf[20] = info.type;       // type as described in https://tools.ietf.org/html/rfc2435
    RtpBuf[21] = q;               // quality scale factor was 0x5e
    RtpBuf[22] = info.width / 8;     // width  / 8
    RtpBuf[23] = info.height / 8;    // height / 8

    int headerLen = 24; // Inlcuding jpeg header but not qant table header
    if (includeQuantTbl)
    {
        // we need a quant header - but only in first packet of the frame
        //printf("inserting quanttbl\n");
        RtpBuf[24] = 0; // MBZ
        RtpBuf[25] = 0; // 8 bit precision
        RtpBuf[26] = 0; // MSB of lentgh

        int numQantBytes = 64; // Two 64 byte tables
        RtpBuf[27] = numOfQuantTbl * numQantBytes; // LSB of length

        headerLen += 4;

        memcpy(RtpBuf + headerLen, info.qTable0, numQantBytes);
        headerLen += numQantBytes;

        if (info.qTable1)
        {
            memcpy(RtpBuf + headerLen, info.qTable1, numQantBytes);
            headerLen += numQantBytes;
        }
    }
    // printf("Sending timestamp %d, seq %d, fragoff %d, fraglen %d, jpegLen %d\n", m_Timestamp, m_SequenceNumber, fragmentOffset, fragmentLen, jpegLen);

    // append the JPEG scan data to the RTP buffer
    memcpy(RtpBuf + headerLen, info.jpegPayload + fragmentOffset, fragmentLen);
    fragmentOffset += fragmentLen;

    // prepare the packet counter for the next packet
    m_SequenceNumber++;

    IPADDRESS otherip;
    IPPORT otherport;

    // RTP marker bit must be set on last fragment
    LinkedListElement* element = m_Clients.m_Next;
    CRtspSession* session = NULL;

    while (element != &m_Clients)
    {
        session = static_cast<CRtspSession*>(element);
        if (session->m_streaming && !session->m_stopped)
        {
            if (session->isTcpTransport())
            {
                // RTP over RTSP - we send the buffer + 4 byte additional header
                socketsend(session->getClient(), RtpBuf, RtpPacketSize + 4);
            }
            else if (m_RtpSocket)
            {
                // UDP - we send just the buffer by skipping the 4 byte RTP over RTSP header
                socketpeeraddr(session->getClient(), &otherip, &otherport);
                ssize_t sent = udpsocketsend(m_RtpSocket, &RtpBuf[4], RtpPacketSize, otherip, session->getRtpClientPort());
                if (sent == -1)
                {
                    return -1;
                }
            }
        }
        element = element->m_Next;
    }
    // printf("CStreamer::SendRtpPacket offset:%d - end\n", fragmentOffset);
    return isLastFragment ? 0 : fragmentOffset;
};

u_short CStreamer::GetRtpServerPort()
{
    return m_RtpServerPort;
};

u_short CStreamer::GetRtcpServerPort()
{
    return m_RtcpServerPort;
};

bool CStreamer::InitUdpTransport(void)
{
    printf("InitUdpTransport\n");
    if (m_udpRefCount != 0)
    {
        ++m_udpRefCount;
        printf("InitUdpTransport: LOL\n");
        return true;
    }

    for (u_short P = 6970; P < 0xFFFE; P += 2)
    {
        m_RtpSocket = udpsocketcreate(P);
        if (m_RtpSocket)
        {
            // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            m_RtcpSocket = udpsocketcreate(P + 1);
            if (m_RtcpSocket)
            {
                m_RtpServerPort  = P;
                m_RtcpServerPort = P+1;
                break;
            }
            else
            {
                udpsocketclose(m_RtpSocket);
                udpsocketclose(m_RtcpSocket);
            };
        }
    };
    ++m_udpRefCount;
    printf("UDP transport created\n");
    return true;
}

void CStreamer::ReleaseUdpTransport(void)
{
    if (m_udpRefCount)
    {
        --m_udpRefCount;
        printf("ReleaseUdpTransport, ref: %d\n", m_udpRefCount);
        if (m_udpRefCount == 0)
        {
            printf("ReleaseUdpTransport!!!\n");
            m_RtpServerPort  = 0;
            m_RtcpServerPort = 0;
            udpsocketclose(m_RtpSocket);
            udpsocketclose(m_RtcpSocket);

            m_RtpSocket = NULLSOCKET;
            m_RtcpSocket = NULLSOCKET;
        }
    }
}

/**
   Call handleRequests on all sessions
 */
bool CStreamer::handleRequests(uint32_t readTimeoutMs)
{
    bool retVal = true;
    LinkedListElement* element = m_Clients.m_Next;
    while(element != &m_Clients)
    {
        CRtspSession* session = static_cast<CRtspSession*>(element);
        retVal &= session->handleRequests(readTimeoutMs);

        element = element->m_Next;

        if (session->m_stopped) 
        {
            // remove session here, so we wont have to send to it
            delete session;
        }
    }

    return retVal;
}

void CStreamer::streamFrame(unsigned const char *data, uint32_t dataLen, uint32_t curMsec)
{
    if (m_prevMsec == 0)
    {
        // first frame init our timestamp
        m_prevMsec = curMsec;
    }

    // compute deltat (being careful to handle clock rollover with a little lie)
    uint32_t deltams = (curMsec >= m_prevMsec) ? curMsec - m_prevMsec : 100;
    m_prevMsec = curMsec;

    DecodedImageInfo info;

    mMetrics.DecodeStart();

    if (!decodeJPEGfile(data, dataLen, info))
    {
        mMetrics.DecodeStop();
        printf("can't decode jpeg data\n");
        return;
    }

    mMetrics.DecodeStop();

//    printf("Decoded image %dx%d, q0: %p, q1: %p, type: %d, payload size: %d, %p\n",
//           info.width, info.height, info.qTable0, info.qTable1, info.type, info.payloadLen, info.jpegPayload);

    int offset = 0;
    do 
    {
        offset = SendRtpPacket(info, offset);
        if (offset == -1)
        {
            printf("Sending of RTP packet aborted\n");
            break;
        }
    } while(offset != 0);

    // Increment ONLY after a full frame
    uint32_t units = 90000; // Hz per RFC 2435
    m_Timestamp += (units * deltams / 1000);    // fixed timestamp increment for a frame rate of 25fps

    m_SendIdx++;
    if (m_SendIdx > 1) m_SendIdx = 0;
};

///
/// STREAMER METRICS
///

void StreamerMetrics::DecodeStart()
{
    mDecodeStartTime = millis();
    if (mReportTime == 0)
    {
        mReportTime = mDecodeStartTime;
    }
}

void StreamerMetrics::DecodeStop()
{
    uint32_t now = millis();
    uint32_t timeDiff = now - mDecodeStartTime;

//    printf("diff: %d\n", timeDiff);

    ++jpegDecodeCount;
    jpegDecodeTimeAcc += timeDiff;
    if (timeDiff < jpegDecodeTimeMin)
    {
        jpegDecodeTimeMin = timeDiff;
    }
    if (timeDiff > jpegDecodeTimeMax)
    {
        jpegDecodeTimeMax = timeDiff;
    }
    mDecodeStartTime = 0;

    if (now - mReportTime >= 1000)
    {
        Report(now - mReportTime);
        mReportTime = now;
    }
}

void StreamerMetrics::Report(uint32_t timeDiff)
{
    float fps = jpegDecodeCount;
    float t = (timeDiff / 1000);
    fps /= t;
    printf("\n-------------------------\n");
    printf("Report (%d)\n", timeDiff);
    printf("-------------------------\n");
    printf("fps: %.1f\n", fps);
    printf("ava: %d\n", jpegDecodeTimeAcc / jpegDecodeCount);
    printf("max: %d\n", jpegDecodeTimeMax);
    printf("min: %d\n", jpegDecodeTimeMin);

    Reset();
}

void StreamerMetrics::Reset()
{
    mDecodeStartTime = 0;
    jpegDecodeCount = 0;
    jpegDecodeTimeAcc = 0;
    jpegDecodeTimeMin = 0xffffffff;
    jpegDecodeTimeMax = 0;

}
