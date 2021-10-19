#pragma once
#include <cstdint>

typedef unsigned const char* BufPtr;

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

// When JPEG is stored as a file it is wrapped in a container
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped
// returns true if the file seems to be valid jpeg
// If quant tables can be found they will be stored in qtable0/1
extern bool decodeJPEGfile(BufPtr start, uint32_t len, DecodedImageInfo& info);
extern bool findJPEGheader(BufPtr *start, uint32_t *len, uint8_t marker);

///
/// \brief findJPEGHeader
/// \param startData - pointer to data to search the marker, if marker found - points to the marker start byte
/// \param dataLen - size of the data startData points at (after the header)
/// \param marker - marker to search
/// \param length - marker length
/// \return - pointer to the marker data
///
extern BufPtr findJPEGheader2(const BufPtr startData, uint32_t dataLen, uint8_t marker, uint32_t& length);

// Given a jpeg ptr pointing to a pair of length bytes, advance the pointer to
// the next 0xff marker byte
extern void nextJpegBlock(BufPtr *start);
