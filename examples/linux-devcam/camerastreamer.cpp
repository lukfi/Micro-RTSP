#include "camerastreamer.h"

/********** DEBUG SETUP **********/
#define ENABLE_SDEBUG
#define DEBUG_PREFIX "CameraStreamer: "
#include "utils/screenlogger.h"
/*********************************/

CameraStreamer::CameraStreamer() :
    CStreamer(640, 480)
{
    int chosenDeviceId = -1;
    int chosenFormatId = -1;

    auto devices = LF::video::VideoDevice::GetDevicesList();

    int deviceId = -1;
    for (auto d : devices)
    {
        SDEB("> %s", d.mName.c_str());
        ++deviceId;

        int formatId = -1;
        auto formats = d.mFormats;
        for (auto f : formats)
        {
            std::stringstream ss;
            ss << f;
            SDEB(">>> %s", ss.str().c_str());
            ++formatId;
            if (f.colorspace == LF::graphic::ColorSpace_t::MJPG &&
                f.resolution.width == 640 &&
                f.resolution.height == 480)
            {
                chosenDeviceId = deviceId;
                chosenFormatId = formatId;
                break;
            }
        }
        if (chosenDeviceId != -1) break;
    }
    if (chosenDeviceId != -1 && chosenFormatId != -1)
    {
        SDEB("Opening device %d format %d", chosenDeviceId, chosenFormatId);
        mDevice = LF::video::VideoDevice::GetVideoDevice(chosenDeviceId, chosenFormatId);

        mCameraThread.RegisterWaitable(mDevice);
        mCameraThread.Start();

        mDevice->SetSpecialOption(0, 0);
        CONNECT(mDevice->NEW_FRAME_AVAILABLE_JPEG, CameraStreamer, OnNewFrame);

        mDevice->Start();
    }
    else
    {
        SERR("No appropariete device found");
    }
}

CameraStreamer::~CameraStreamer()
{
    delete mDevice;
}

void CameraStreamer::streamImage(uint32_t curMsec)
{
    //SDEB("streamImage");
    if (mDevice)
    {
        mNewFrame = false;
        SDEB("Sending frame (size: %d)", mImage.GetDataSize());
        streamFrame(mImage.GetRawData(), mImage.GetDataSize(), curMsec);
    }
}

void CameraStreamer::OnNewFrame(LF::video::VideoDevice* device)
{
    SDEB("OnNewFrame");
    device->GetFrame(mImage);
    mNewFrame = true;
}
