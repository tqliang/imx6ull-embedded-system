#include "mjpegrecorder.h"
#include <QDebug>
#include <QDateTime>
#include <QImage>
#include <QBuffer>

#pragma pack(push, 1)
struct AviMainHeader {
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
};

struct AviStreamHeader {
    uint8_t  fccType[4];
    uint8_t  fccHandler[4];
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    int16_t  rcFrame[4];
};

struct BitmapInfoHeader {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint8_t  biCompression[4];
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

static void writeFourCC(QFile &f, const char *fourcc)
{
    f.write(fourcc, 4);
}

static void writeU32(QFile &f, uint32_t v)
{
    f.write((const char *)&v, 4);
}

MjpegRecorder::MjpegRecorder(QObject *parent)
    : QObject(parent)
    , m_recording(false)
    , m_width(640)
    , m_height(480)
    , m_fps(30)
    , m_quality(85)
    , m_frameCount(0)
    , m_moviSize(0)
    , m_headerPos_moviSize(0)
    , m_headerPos_totalSize(0)
{
}

MjpegRecorder::~MjpegRecorder()
{
    if (m_recording)
        stopRecording();
}

bool MjpegRecorder::startRecording(const QString &filePath,
                                    int width, int height, int fps, int quality)
{
    if (m_recording)
        return false;

    m_width   = width;
    m_height  = height;
    m_fps     = fps;
    m_quality = quality;
    m_frameCount = 0;
    m_moviSize   = 0;
    m_index.clear();

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        qWarning("MjpegRecorder: cannot open %s", qPrintable(filePath));
        return false;
    }

    writeAviHeader();

    m_recording = true;
    m_elapsed.start();
    emit recordingStarted();
    return true;
}

void MjpegRecorder::stopRecording()
{
    if (!m_recording)
        return;

    finalizeAvi();
    m_file.close();
    m_recording = false;
    emit recordingStopped(m_file.fileName());
}

void MjpegRecorder::writeFrame(const unsigned char *rgbData, int width, int height)
{
    if (!m_recording)
        return;

    QByteArray jpeg;
    if (!encodeJpeg(rgbData, width, height, m_quality, jpeg))
        return;

    if (jpeg.size() % 2 != 0)
        jpeg.append('\0');

    writeFourCC(m_file, "00dc");
    writeU32(m_file, (uint32_t)jpeg.size());
    m_file.write(jpeg);

    m_index.append({ m_moviSize, (uint32_t)jpeg.size() });
    m_moviSize += 8 + (uint32_t)jpeg.size();
    m_frameCount++;

    qint64 sec = m_elapsed.elapsed() / 1000;
    emit durationChanged((int)sec);
}

void MjpegRecorder::writeAviHeader()
{
    QFile &f = m_file;

    writeFourCC(f, "RIFF");
    m_headerPos_totalSize = f.pos();
    writeU32(f, 0);

    writeFourCC(f, "AVI ");

    writeFourCC(f, "LIST");
    writeU32(f, 192);
    writeFourCC(f, "hdrl");

    writeFourCC(f, "avih");
    writeU32(f, 56);

    AviMainHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.dwMicroSecPerFrame   = (uint32_t)(1000000.0 / m_fps);
    hdr.dwMaxBytesPerSec     = m_width * m_height * 3 * m_fps;
    hdr.dwPaddingGranularity = 0;
    hdr.dwFlags              = 0x10;
    hdr.dwTotalFrames        = 0;
    hdr.dwInitialFrames      = 0;
    hdr.dwStreams            = 1;
    hdr.dwSuggestedBufferSize = m_width * m_height * 3;
    hdr.dwWidth              = m_width;
    hdr.dwHeight             = m_height;
    hdr.dwReserved[0] = 0;
    hdr.dwReserved[1] = 0;
    hdr.dwReserved[2] = 0;
    hdr.dwReserved[3] = 0;
    f.write((const char *)&hdr, sizeof(hdr));

    writeFourCC(f, "LIST");
    writeU32(f, 116);
    writeFourCC(f, "strl");

    writeFourCC(f, "strh");
    writeU32(f, 56);

    AviStreamHeader strh;
    memset(&strh, 0, sizeof(strh));
    memcpy(strh.fccType,    "vids", 4);
    memcpy(strh.fccHandler, "MJPG", 4);
    strh.dwFlags             = 0;
    strh.wPriority           = 0;
    strh.wLanguage           = 0;
    strh.dwInitialFrames     = 0;
    strh.dwScale             = 1;
    strh.dwRate              = m_fps;
    strh.dwStart             = 0;
    strh.dwLength            = 0;
    strh.dwSuggestedBufferSize = m_width * m_height * 3;
    strh.dwQuality           = (uint32_t)(m_quality * 100);
    strh.dwSampleSize        = 0;
    memset(strh.rcFrame, 0, sizeof(strh.rcFrame));
    f.write((const char *)&strh, sizeof(strh));

    writeFourCC(f, "strf");
    writeU32(f, 40);

    BitmapInfoHeader bmp;
    memset(&bmp, 0, sizeof(bmp));
    bmp.biSize        = 40;
    bmp.biWidth       = m_width;
    bmp.biHeight      = m_height;
    bmp.biPlanes      = 1;
    bmp.biBitCount    = 24;
    memcpy(bmp.biCompression, "MJPG", 4);
    bmp.biSizeImage   = m_width * m_height * 3;
    bmp.biXPelsPerMeter = 0;
    bmp.biYPelsPerMeter = 0;
    bmp.biClrUsed     = 0;
    bmp.biClrImportant = 0;
    f.write((const char *)&bmp, sizeof(bmp));

    writeFourCC(f, "LIST");
    uint32_t moviSizePlaceholder = 0;
    m_headerPos_moviSize = f.pos();
    writeU32(f, moviSizePlaceholder);
    writeFourCC(f, "movi");
}

void MjpegRecorder::finalizeAvi()
{
    QFile &f = m_file;

    qint64 currentPos = f.pos();

    f.seek(m_headerPos_totalSize);
    uint32_t totalSize = (uint32_t)(currentPos - 8);
    writeU32(f, totalSize);

    f.seek(0x40);
    AviMainHeader hdr;
    f.read((char *)&hdr, sizeof(hdr));
    hdr.dwTotalFrames = m_frameCount;
    f.seek(0x40);
    f.write((const char *)&hdr, sizeof(hdr));

    f.seek(0xA4);
    AviStreamHeader strh;
    f.read((char *)&strh, sizeof(strh));
    strh.dwLength = m_frameCount;
    f.seek(0xA4);
    f.write((const char *)&strh, sizeof(strh));

    f.seek(m_headerPos_moviSize);
    writeU32(f, m_moviSize + 4);

    f.seek(currentPos);

    writeFourCC(f, "idx1");
    writeU32(f, (uint32_t)(m_index.size() * 16));

    for (const auto &entry : m_index) 
    {
        writeFourCC(f, "00dc");
        writeU32(f, 0x10);
        writeU32(f, entry.offset + 4);
        writeU32(f, entry.size);
    }
}

bool MjpegRecorder::encodeJpeg(const unsigned char *rgbData, int width, int height,
                                int quality, QByteArray &outJpeg)
{
    QImage image(rgbData, width, height, QImage::Format_RGB32);
    QBuffer buffer(&outJpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPEG", quality)) 
    {
        qWarning("MjpegRecorder: JPEG encode failed");
        return false;
    }
    return true;
}