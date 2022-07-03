#include "mmpr/mmpr.h"

#include "mmpr/pcap/MMPcapReader.h"
#include "mmpr/pcapng/MMPcapNgReader.h"
#include "mmpr/pcapng/ZstdPcapNgReader.h"
#include "util.h"
#include <filesystem>
#include <iostream>
#include <memory>

namespace mmpr {

FileReader::FileReader(const std::string& filepath) : mFilepath(filepath) {}

std::unique_ptr<FileReader> FileReader::getReader(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        throw std::runtime_error("FileReader: could not find file \"" + filepath + "\"");
    }

    uint32_t magicNumber = util::read32bitsFromFile(filepath);
    switch (magicNumber) {
    case MMPR_MAGIC_NUMBER_PCAP_MICROSECONDS:
    case MMPR_MAGIC_NUMBER_PCAP_NANOSECONDS:
        return std::unique_ptr<MMPcapReader>(new MMPcapReader(filepath));
    case MMPR_MAGIC_NUMBER_PCAPNG:
        return std::unique_ptr<MMPcapNgReader>(new MMPcapNgReader(filepath));
    case MMPR_MAGIC_NUMBER_ZSTD:
        return std::unique_ptr<ZstdPcapNgReader>(new ZstdPcapNgReader(filepath));
    default:
        throw std::runtime_error("Failed to determine file type based on first 32 bits");
    }
}

} // namespace mmpr