#ifndef CAMERASTREAMER_H
#define CAMERASTREAMER_H

#include "CStreamer.h"
#include "video/videodevice.h"
#include "threads/iothread.h"

#if M_OS == M_OS_WINDOWS
#include "graphics/jpeg.h"
#endif

class CameraStreamer : public CStreamer
{
public:
    CameraStreamer();
    virtual ~CameraStreamer();

    virtual void streamImage(uint32_t curMsec) override;
    virtual bool handleRequests(uint32_t readTimeoutMs) override;

private:
    void OnNewFrame(LF::video::VideoDevice* device);
    void OnNewJPEGFrame(LF::video::VideoDevice* device);
    LF::video::VideoDevice* mDevice {nullptr};
    LF::graphic::RawImage mImage;

    LF::threads::IOThread mCameraThread;

#if M_OS == M_OS_WINDOWS
    LF::graphic::JPEGEncoder mEncoder;
#endif
};

#endif // CAMERASTREAMER_H
