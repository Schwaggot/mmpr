#include "mmpr/pcap/MMPcapReader.h"

#include "mmpr/pcap/PcapParser.h"
#include "util.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

using namespace std;

namespace mmpr {
MMPcapReader::MMPcapReader(const string& filepath) : PcapReader(filepath) {
    uint32_t magicNumber = util::read32bitsFromFile(filepath);
    if (magicNumber != MMPR_MAGIC_NUMBER_PCAP_MICROSECONDS &&
        magicNumber != MMPR_MAGIC_NUMBER_PCAP_NANOSECONDS) {
        stringstream sstream;
        sstream << std::hex << magicNumber;
        string hex = sstream.str();
        std::transform(hex.begin(), hex.end(), hex.begin(), ::toupper);
        throw std::runtime_error("Expected PCAP format to start with appropriate magic "
                                 "numbers, instead got: 0x" +
                                 hex + ", possibly little/big endian issue");
    }
}

void MMPcapReader::open() {
    mFileDescriptor = ::open(mFilepath.c_str(), O_RDONLY, 0);
    if (mFileDescriptor < 0) {
        throw runtime_error("Error while reading file " +
                            std::filesystem::absolute(mFilepath).string() + ": " +
                            strerror(errno));
    }

    mFileSize = lseek(mFileDescriptor, 0, SEEK_END);
    mMappedSize = (mFileSize / MMPR_PAGE_SIZE + 1) * MMPR_PAGE_SIZE;

    auto mmapResult =
        mmap(nullptr, mMappedSize, PROT_READ, MAP_SHARED, mFileDescriptor, 0);
    if (mmapResult == MAP_FAILED) {
        ::close(mFileDescriptor);
        throw runtime_error("Error while mapping file " +
                            std::filesystem::absolute(mFilepath).string() + ": " +
                            strerror(errno));
    }

    mOffset = 0;
    mMappedMemory = reinterpret_cast<const uint8_t*>(mmapResult);

    FileHeader fileHeader{};
    PcapParser::readFileHeader(mMappedMemory, fileHeader);
    mDataLinkType = fileHeader.linkType;
    mTimestampFormat = fileHeader.timestampFormat;
    mOffset += 24;
}

bool MMPcapReader::isExhausted() const {
    return mOffset >= mFileSize;
}

bool MMPcapReader::readNextPacket(Packet& packet) {
    if (isExhausted()) {
        // nothing more to read
        return false;
    }

    // make sure there are enough bytes to read
    if (mFileSize - mOffset < 16) {
        throw runtime_error("Expected to read at least one more packet record (16 bytes "
                            "at least), but there are only " +
                            to_string(mFileSize - mOffset) + " bytes left in the file");
    }

    PacketRecord packetRecord{};
    PcapParser::readPacketRecord(&mMappedMemory[mOffset], packetRecord);
    packet.timestampSeconds = packetRecord.timestampSeconds;
    packet.timestampMicroseconds = mTimestampFormat == FileHeader::MICROSECONDS
                                       ? packetRecord.timestampSubSeconds
                                       : packetRecord.timestampSubSeconds / 1000;
    packet.captureLength = packetRecord.captureLength;
    packet.length = packetRecord.length;
    packet.data = packetRecord.data;

    mOffset += 16 + packetRecord.captureLength;

    return true;
}

void MMPcapReader::close() {
    munmap((void*)mMappedMemory, mMappedSize);
    ::close(mFileDescriptor);
}

} // namespace mmpr
