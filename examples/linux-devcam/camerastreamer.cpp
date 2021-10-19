#include "camerastreamer.h"

#include <algorithm>

/********** DEBUG SETUP **********/
#define ENABLE_SDEBUG
#define DEBUG_PREFIX "CameraStreamer: "
#include "utils/screenlogger.h"
/*********************************/

CameraStreamer::CameraStreamer() :
    CStreamer()
{

    int chosenDeviceId;
    int chosenFormatId;
    if (!FindDevice(LF::graphic::ColorSpace_t::MJPG, chosenDeviceId, chosenFormatId))
    {
        FindDevice(LF::graphic::ColorSpace_t::RGB24, chosenDeviceId, chosenFormatId);
    }

    if (chosenDeviceId != -1 && chosenFormatId != -1)
    {
        SDEB("Opening device %d format %d", chosenDeviceId, chosenFormatId);
        mDevice = LF::video::VideoDevice::GetVideoDevice(chosenDeviceId, chosenFormatId);

        std::string threadName = mDevice->GetName() + "Thread";
        threadName.erase(remove_if(threadName.begin(), threadName.end(), isspace), threadName.end());
        mCameraThread.SetName(threadName);

        mCameraThread.RegisterWaitable(mDevice);
        mCameraThread.Start();

        mDevice->SetSpecialOption(0, 0);
        CONNECT(mDevice->NEW_FRAME_AVAILABLE, CameraStreamer, OnNewFrame);
        CONNECT(mDevice->NEW_FRAME_AVAILABLE_JPEG, CameraStreamer, OnNewJPEGFrame);
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
    if (anySessionsStreaming())
    {
        uint8_t* data = nullptr;
        uint8_t* dataToSend = nullptr;

        uint32_t len = 0;

        if (mImage.GetColorSpace() != LF::graphic::ColorSpace_t::MJPG)
        {
            mEncoder.Encode(data, len, mImage);
            dataToSend = data;
        }
        else
        {
            dataToSend = mImage.GetNonConstRawData();
            len = mImage.GetDataSize();
        }

        if (mSink)
        {
            mSink->StreamFrame(dataToSend, len, curMsec);
        }
        else
        {
            streamFrame(dataToSend, len, curMsec);
            delete[] data;
        }
    }
}

bool CameraStreamer::handleRequests(uint32_t readTimeoutMs)
{
    bool ret = CStreamer::handleRequests(readTimeoutMs);

    if (ret && !mDevice->IsRunning() && anySessionsStreaming())
    {
        mDevice->Start();
    }

    return ret;
}

bool CameraStreamer::FindDevice(LF::graphic::ColorSpace_t colorspace, int& device, int& format)
{
    device = -1;
    format = -1;

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
            if (f.colorspace == colorspace)
            {
                device = d.mId;
                format = formatId;
                break;
            }
        }
        if (device != -1) break;
    }
    return device != -1 && format != -1;
}

void CameraStreamer::OnNewFrame(LF::video::VideoDevice *device)
{
//    SDEB("OnNewFrame");
    if (!anySessionsStreaming())
    {
        mDevice->Stop();
        return;
    }
    device->GetFrame(mImage);
    streamImage(millis());
}

//#define DEBUG_JPEG
#ifdef DEBUG_JPEG
#include "fs/file.h"
#endif

void CameraStreamer::OnNewJPEGFrame(LF::video::VideoDevice* device)
{
//    SDEB("OnNewJPEGFrame");
    if (!anySessionsStreaming())
    {
        mDevice->Stop();
        return;
    }
    device->GetFrame(mImage);
#ifdef DEBUG_JPEG
    static int i = 0;
    if (i++ == 0)
    {
        LF::fs::File f("/home/pi/my.jpeg");
        f.CreateOpen();
        f.Put(mImage.GetNonConstRawData(), mImage.GetDataSize());
        f.Close();
    }
#endif
    streamImage(millis());
}
