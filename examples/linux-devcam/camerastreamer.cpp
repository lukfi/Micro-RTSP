#include "camerastreamer.h"

/********** DEBUG SETUP **********/
#define ENABLE_SDEBUG
#define DEBUG_PREFIX "CameraStreamer: "
#include "utils/screenlogger.h"
/*********************************/

CameraStreamer::CameraStreamer() :
    CStreamer()
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
            if ((f.colorspace == LF::graphic::ColorSpace_t::MJPG ||
                 f.colorspace == LF::graphic::ColorSpace_t::RGB24) &&
                f.resolution.width == 640 &&
                f.resolution.height == 480)
            {
                chosenDeviceId = d.mId;
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

    #if M_OS == M_OS_WINDOWS
        mEncoder.Encode(data, len, mImage);
        dataToSend = data;
    #else
        //image = mImage;
        dataToSend = mImage.GetNonConstRawData();
        len = mImage.GetDataSize();
    #endif
        streamFrame(dataToSend, len, curMsec);

        delete[] data;
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

void CameraStreamer::OnNewFrame(LF::video::VideoDevice *device)
{
    if (!anySessionsStreaming())
    {
        mDevice->Stop();
        return;
    }
    device->GetFrame(mImage);
    streamImage(millis());
}

void CameraStreamer::OnNewJPEGFrame(LF::video::VideoDevice* device)
{
    SDEB("OnNewJPEGFrame");
    device->GetFrame(mImage);
}
