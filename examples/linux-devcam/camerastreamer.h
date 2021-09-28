#ifndef CAMERASTREAMER_H
#define CAMERASTREAMER_H

#include "CStreamer.h"
#include "video/videodevice.h"
#include "threads/iothread.h"

class CameraStreamer : public CStreamer
{
public:
    CameraStreamer();
    virtual ~CameraStreamer();

    virtual void streamImage(uint32_t curMsec);

private:
    void OnNewFrame(LF::video::VideoDevice* device);
    LF::video::VideoDevice* mDevice {nullptr};
    LF::graphic::RawImage mImage;
    std::atomic_bool mNewFrame { false };

    LF::threads::IOThread mCameraThread;
};

#endif // CAMERASTREAMER_H
