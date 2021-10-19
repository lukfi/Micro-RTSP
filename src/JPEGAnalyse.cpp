#include "JPEGAnalyse.h"

#include <assert.h>
#include <cstdio>

// search for a particular JPEG marker, moves *start to just after that marker
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped
// APP0 e0
// DQT db
// DQT db
// DHT c4
// DHT c4
// DHT c4
// DHT c4
// SOF0 c0 baseline (not progressive) 3 color 0x01 Y, 0x21 2h1v, 0x00 tbl0
// - 0x02 Cb, 0x11 1h1v, 0x01 tbl1 - 0x03 Cr, 0x11 1h1v, 0x01 tbl1
// therefore 4:2:2, with two separate quant tables (0 and 1)
// SOS da
// EOI d9 (no need to strip data after this RFC says client will discard)
bool findJPEGheader(BufPtr *start, uint32_t *len, uint8_t marker)
{
    // per https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
    unsigned const char *bytes = *start;

    // kinda skanky, will break if unlucky and the headers inxlucde 0xffda
    // might fall off array if jpeg is invalid
    // FIXME - return false instead
    while (bytes - *start < *len)
    {
        uint8_t framing = *bytes++; // better be 0xff
        if(framing != 0xff)
        {
            //printf("malformed jpeg, framing=%x\n", framing);
            return false;
        }
        uint8_t typecode = *bytes++;

        if(typecode == marker)
        {
            unsigned skipped = bytes - *start;
            //printf("found marker 0x%x, skipped %d\n", marker, skipped);

            *start = bytes;

            // shrink len for the bytes we just skipped
            *len -= skipped;

            return true;
        }
        else
        {
            // not the section we were looking for, skip the entire section
            switch(typecode)
            {
            case 0xd8:   // start of image
            {
                break;   // no data to skip
            }
            case 0xe0:   // app0
            case 0xdb:   // dqt
            case 0xc4:   // dht
            case 0xc0:   // sof0
            case 0xda:   // sos
            {
                // standard format section with 2 bytes for len.  skip that many bytes
                uint32_t len = bytes[0] * 256 + bytes[1];
                //printf("skipping section 0x%x, %d bytes\n", typecode, len);
                bytes += len;
                break;
            }
            default:
                printf("unexpected jpeg typecode 0x%x\n", typecode);
                break;
            }
        }
    }

    printf("failed to find jpeg marker 0x%x", marker);
    return false;
}

// the scan data uses byte stuffing to guarantee anything that starts with 0xff
// followed by something not zero, is a new section.  Look for that marker and return the ptr
// pointing there
void skipScanBytes(BufPtr *start)
{
    BufPtr bytes = *start;

    while(true)
    {
        // FIXME, check against length
        while(*bytes++ != 0xff);
        if(*bytes++ != 0)
        {
            *start = bytes - 2; // back up to the 0xff marker we just found
            return;
        }
    }
}
void  nextJpegBlock(BufPtr *bytes)
{
    uint32_t len = (*bytes)[0] * 256 + (*bytes)[1];
    //printf("going to next jpeg block %d bytes\n", len);
    *bytes += len;
}

// When JPEG is stored as a file it is wrapped in a container
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped
bool decodeJPEGfile(BufPtr start, uint32_t len, DecodedImageInfo &info)
{
    // per https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
    unsigned const char* bytes = start;
    uint32_t length = len;

    if (!findJPEGheader(&bytes, &length, 0xd8)) // better at least look like a jpeg file
    {
        return false; // FAILED!
    }

    info.type = 0;
    // Find SOF0
    // decoding done based on documentation found at:
    // http://vip.sugovica.hu/Sardi/kepnezo/JPEG%20File%20Layout%20and%20Format.htm
    // to determine the Type field as described in [4.1.  The Type Field]:
    // https://datatracker.ietf.org/doc/html/rfc2435
    {
        BufPtr markerData = nullptr;
        uint32_t markerLen;
        if ((markerData = findJPEGheader2(start, len, 0xC0, markerLen)))
        {
            int components = (markerLen - 8) / 3;
            markerData += 2; // skip len
            markerData += 1; // skip data precision
            info.height = markerData[0] << 8 | markerData[1];
            markerData += 2; // skip height
            info.width = markerData[0] << 8 | markerData[1];
            markerData += 2; // skip width
            uint32_t componentsCount = markerData[0];
            markerData += 1; // skip components count
//            printf("Found marker %02x length: %d, width: %d, height: %d, components:%d\n",
//                   0xC0, markerLen, info.width, info.height, components);

            for (int c = 0; c < components; ++c)
            {
                if (c == 0)
                {
                    if (markerData[1] == 0x22)
                    {
                        info.type = 1;
                    }
                    break;
                }
                printf("C%d: %02x %02x %02x, TYPE: %d\n",
                       c, markerData[0], markerData[1], markerData[2], info.type);
                markerData += 3;
            }
        }
    }

    // Look for quant tables if they are present
    info.qTable0 = nullptr;
    info.qTable1 = nullptr;

    BufPtr quantstart = start;
    uint32_t quantlen = len;
    if (!findJPEGheader(&quantstart, &quantlen, 0xdb))
    {
        printf("error can't find quant table 0\n");
    }
    else
    {
        // printf("found quant table %x\n", quantstart[2]);
        info.qTable0 = quantstart + 3;     // 3 bytes of header skipped
        nextJpegBlock(&quantstart);
        if(!findJPEGheader(&quantstart, &quantlen, 0xdb))
        {
            //printf("error can't find quant table 1\n");
            info.qTable1 = nullptr;
        }
        else
        {
            // printf("found quant table %x\n", quantstart[2]);
            info.qTable1 = quantstart + 3;
            nextJpegBlock(&quantstart);
        }
    }

    // find Start of Scan (SOS) marker to determine start of
    // JPEG payload
    if(!findJPEGheader(&start, &len, 0xda))
        return false; // FAILED!

    // Skip the header bytes of the SOS marker FIXME why doesn't this work?
    uint32_t soslen = (start)[0] * 256 + (start)[1];
    start += soslen;
    len -= soslen;

    // start scanning the data portion of the scan to find the end marker
    BufPtr endmarkerptr = start;
    uint32_t endlen =  len;

    skipScanBytes(&endmarkerptr);
    if(!findJPEGheader(&endmarkerptr, &endlen, 0xd9))
        return false; // FAILED!

    // endlen must now be the # of bytes between the start of our scan and
    // the end marker, tell the caller to ignore bytes afterwards
    len = endmarkerptr - start - 2; // LF# -2 to skip end marker

    info.jpegPayload = start;
    info.payloadLen = len;

    return true;
}
BufPtr findJPEGheader2(const BufPtr startData, uint32_t dataLen, uint8_t marker, uint32_t &length)
{
    BufPtr ret = nullptr;
    length = 0;

    for (int i = 0; i < dataLen - 1; ++i)
    {
        if (startData[i] == 0xff && startData[i + 1] == marker)
        {
            ret = &startData[i + 2];
            // not the section we were looking for, skip the entire section
            switch(marker)
            {
            case 0xd8:   // start of image
            case 0xd9:   // start of image
            {
                break;   // no data to skip
            }
            case 0xe0:   // app0
            case 0xdb:   // dqt
            case 0xc4:   // dht
            case 0xc0:   // sof0
            case 0xda:   // sos
            {
                if (dataLen < i + 4)
                {
                    ret = nullptr;
                    printf("not enaugh data to read length field\n");
                }
                else
                {
                    // standard format section with 2 bytes for len.  skip that many bytes
                    uint32_t len = startData[i + 2] * 256 + startData[i + 3];
                    if (i + 2 + len > dataLen)
                    {
                        ret = nullptr;
                        printf("not enaugh data to read whole marker");
                    }
                    else
                    {
                        length = len;
                    }
                }
                break;
            }
            default:
                printf("unexpected jpeg typecode 0x%x\n", marker);
                break;
            }
            break;
        }
    }

    return ret;
}
