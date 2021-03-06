#ifndef MMPR_MMPCAPREADER_H
#define MMPR_MMPCAPREADER_H

#include "mmpr/mmpr.h"
#include "mmpr/pcap/PcapReader.h"
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace mmpr {
class MMPcapReader : public PcapReader {
public:
    explicit MMPcapReader(const std::string& filepath);

    void open() override;
    bool isExhausted() const override;
    bool readNextPacket(Packet& packet) override;
    void close() override;

    size_t getFileSize() const override { return mFileSize; }
    size_t getCurrentOffset() const override { return mOffset; }

private:
    int mFileDescriptor{0};
    size_t mFileSize{0};
    size_t mMappedSize{0};
    const uint8_t* mMappedMemory{nullptr};
    size_t mOffset{0};
    FileHeader::TimestampFormat mTimestampFormat{FileHeader::MICROSECONDS};
};
} // namespace mmpr

#endif // MMPR_MMPCAPREADER_H
