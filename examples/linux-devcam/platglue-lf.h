#pragma once

//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "net/socketmaster.h"
#include "utils/datetime.h"

typedef uint32_t IPADDRESS; // On linux use uint32_t in network byte order (per getpeername)
typedef uint16_t IPPORT; // on linux use network byte order

class WiFiClient
{
public:
    WiFiClient(TCP_Socket* socket) :
        mSocket(socket)
    {

    }
    WiFiClient(LF::net::SocketMaster* master) :
        mMaster(master)
    {
        mCircular = new LF::utils::CircularBuffer(8092);
    }

    ~WiFiClient()
    {
        delete mCircular;
    }

    void Close()
    {
        if (mSocket)
        {
            mSocket->Close();
        }
        else
        {
            mMaster->SetUserData(nullptr);
//            mMaster->Close();
        }
    }

    void GetPeedAddress(IPADDRESS* addr, IPPORT* port)
    {
        IP_Address a;
        if (mSocket)
        {
            mSocket->GetRemoteAddress(a);
        }
        else
        {
            mMaster->GetRemoteAddress(a);
        }
        *addr = a.GetHost().IPv4Host();
        *port = a.GetPort();
    }

    ssize_t Send(const void *buf, size_t len)
    {
        ssize_t wrote = 0;
        if (mSocket)
        {
            wrote = mSocket->Write((const char*)buf, len);
        }
        else
        {
            MessageBuffer* buffer = new MessageBuffer(reinterpret_cast<const uint8_t*>(buf), len);
            buffer->Rewind();
            wrote = buffer->Size();
            mMaster->ScheduleBuffer(buffer);
        }
        return wrote;
    }

    int Read(char *buf, size_t buflen, int timeoutmsec)
    {
        int ret = 0;
        if (mSocket)
        {
            ret = mSocket->Read(buf, buflen);
            if (ret == 0)
            {
                ret = -1;
            }
            else if (ret == -1)
            {
                ret = 0;
            }
        }
        else
        {
            mMaster->Read(*mCircular);
            ret = mCircular->Get(reinterpret_cast<uint8_t*>(buf), std::min((uint32_t)buflen, mCircular->Size()));
            if (ret <= 0)
            {
                printf("RREETTT = %d\n", ret);
            }
            if (ret == 0)
            {
                ret = -1;
            }
        }

        return ret;
    }
private:
    TCP_Socket* mSocket {nullptr};
    LF::net::SocketMaster* mMaster {nullptr};
    LF::utils::CircularBuffer* mCircular { nullptr };
};

class WiFiUdp
{
public:
    WiFiUdp(uint16_t port)
    {
        mSocket = new UDP_Socket();
        UDP_OpenSocketStatus success = mSocket->Open(port);
        if (success != UDP_OpenSocketStatus::OpenOk)
        {
            printf("failed to open UDP socket\n");
        }
    }

    void Close()
    {
        mSocket->Close();
        delete mSocket;
        mSocket = nullptr;
    }

    ssize_t Send(const void *buf, size_t len,
                 IPADDRESS destaddr, uint16_t destport)
    {
        ssize_t wrote = 0;
        if (mSocket)
        {
            IP_Address addr(destaddr, destport);
            wrote = mSocket->Write((const char*)buf, len, addr);
        }
        else
        {
            printf("ERROR WRITING UPD DATA\n");
        }
        return wrote;
    }

    int Read(char *buf, size_t buflen, int timeoutmsec)
    {
        //return mSocket->Read(buf, buflen);
    }
private:
    UDP_Socket* mSocket {nullptr};
};

typedef WiFiClient* TCPSOCKET;
typedef WiFiUdp* UDPSOCKET;

#define NULLSOCKET 0

inline void closesocket(TCPSOCKET s)
{
    // close(s);
    s->Close();
}

#define getRandom() rand()

inline void socketpeeraddr(TCPSOCKET s, IPADDRESS *addr, IPPORT *port)
{
    if (s)
    {
        s->GetPeedAddress(addr, port);
    }
}

inline void udpsocketclose(UDPSOCKET s)
{
    if (s)
    {
        s->Close();
    }
}

inline UDPSOCKET udpsocketcreate(unsigned short portNum)
{
    WiFiUdp* newSocket = new WiFiUdp(portNum);
    return newSocket;
}

// TCP sending
inline ssize_t socketsend(TCPSOCKET sockfd, const void *buf, size_t len)
{
    return sockfd->Send(buf, len);
}

inline ssize_t udpsocketsend(UDPSOCKET sockfd, const void *buf, size_t len,
                             IPADDRESS destaddr, uint16_t destport)
{
    return sockfd->Send(buf, len, destaddr, destport);
}

/**
   Read from a socket with a timeout.

   Return 0=socket was closed by client, -1=timeout, >0 number of bytes read
 */
inline int socketread(TCPSOCKET sock, char *buf, size_t buflen, int timeoutmsec)
{
    return sock->Read(buf, buflen, timeoutmsec);
}

inline unsigned long millis()
{
    static uint64_t first = LF::utils::DateTime::TimestampMs();
    uint64_t now = LF::utils::DateTime::TimestampMs();
    return now - first;
}
