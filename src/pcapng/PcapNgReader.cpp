#include "mmpr/pcapng/PcapNgReader.h"

#include "mmpr/pcapng/PcapNgBlockParser.h"
#include "util.h"

using namespace std;

namespace mmpr {

bool PcapNgReader::readNextPacket(Packet& packet) {
    if (isExhausted()) {
        // nothing more to read
        return false;
    }

    // make sure there are enough bytes to read
    if (mFileSize - mOffset < 8) {
        throw runtime_error("Expected to read at least one more block (8 bytes at "
                            "least), but there are only " +
                            to_string(mFileSize - mOffset) + " bytes left in the file");
    }

    uint32_t blockType = *(uint32_t*)&mData[mOffset];
    uint32_t blockTotalLength = *(uint32_t*)&mData[mOffset + 4];

    // TODO add support for Simple Packet Blocks
    while (blockType != MMPR_ENHANCED_PACKET_BLOCK && blockType != MMPR_PACKET_BLOCK) {
        if (blockType == MMPR_SECTION_HEADER_BLOCK) {
            SectionHeaderBlock shb{};
            PcapNgBlockParser::readSHB(&mData[mOffset], shb);
            mMetadata.comment = shb.options.comment;
            mMetadata.os = shb.options.os;
            mMetadata.hardware = shb.options.hardware;
            mMetadata.userApplication = shb.options.userApplication;
        } else if (blockType == MMPR_INTERFACE_DESCRIPTION_BLOCK) {
            InterfaceDescriptionBlock idb{};
            PcapNgBlockParser::readIDB(&mData[mOffset], idb);
            mDataLinkType = idb.linkType;
            mMetadata.timestampResolution = idb.options.timestampResolution;
            mTraceInterfaces.emplace_back(idb.options.name, idb.options.description,
                                          idb.options.filter, idb.options.os);
        }

        mOffset += blockTotalLength;

        if (isExhausted()) {
            // we have reached the end of the file
            return false;
        }

        // make sure there are enough bytes to read
        if (mFileSize - mOffset < 8) {
            throw runtime_error("Expected to read at least one more block (8 bytes at "
                                "least), but there are only " +
                                to_string(mFileSize - mOffset) +
                                " bytes left in the file");
        }

        // try to read next block type
        blockType = *(const uint32_t*)&mData[mOffset];
        blockTotalLength = *(const uint32_t*)&mData[mOffset + 4];
    }

    switch (blockType) {
    case MMPR_ENHANCED_PACKET_BLOCK: {
        EnhancedPacketBlock epb{};
        PcapNgBlockParser::readEPB(&mData[mOffset], epb);
        util::calculateTimestamps(mMetadata.timestampResolution, epb.timestampHigh,
                                  epb.timestampLow, &(packet.timestampSeconds),
                                  &(packet.timestampMicroseconds));
        packet.captureLength = epb.capturePacketLength;
        packet.length = epb.originalPacketLength;
        packet.data = epb.packetData;
        packet.interfaceIndex = epb.interfaceId;

        mOffset += epb.blockTotalLength;
        break;
    }
    case MMPR_PACKET_BLOCK: {
        PacketBlock pb{};
        PcapNgBlockParser::readPB(&mData[mOffset], pb);
        util::calculateTimestamps(mMetadata.timestampResolution, pb.timestampHigh,
                                  pb.timestampLow, &(packet.timestampSeconds),
                                  &(packet.timestampMicroseconds));
        packet.captureLength = pb.capturePacketLength;
        packet.length = pb.originalPacketLength;
        packet.data = pb.packetData;
        packet.interfaceIndex = pb.interfaceId;

        mOffset += pb.blockTotalLength;
        break;
    }
    }

    return true;
}

/**
 * 3.1.  General Block Structure
 *
 *                        1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 0 |                          Block Type                           |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 4 |                      Block Total Length                       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 8 /                          Block Body                           /
 *   /              variable length, padded to 32 bits               /
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                      Block Total Length                       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
uint32_t PcapNgReader::readBlock() {
    const auto blockType = *(const uint32_t*)&mData[mOffset];
    const auto blockTotalLength = *(const uint32_t*)&mData[mOffset + 4];

    switch (blockType) {
    case MMPR_SECTION_HEADER_BLOCK: {
        SectionHeaderBlock shb{};
        PcapNgBlockParser::readSHB(&mData[mOffset], shb);
        mMetadata.comment = shb.options.comment;
        mMetadata.os = shb.options.os;
        mMetadata.hardware = shb.options.hardware;
        mMetadata.userApplication = shb.options.userApplication;
        break;
    }
    case MMPR_INTERFACE_DESCRIPTION_BLOCK: {
        InterfaceDescriptionBlock idb{};
        PcapNgBlockParser::readIDB(&mData[mOffset], idb);
        mDataLinkType = idb.linkType;
        mMetadata.timestampResolution = idb.options.timestampResolution;
        mTraceInterfaces.emplace_back(idb.options.name, idb.options.description,
                                      idb.options.filter, idb.options.os);
        break;
    }
    case MMPR_ENHANCED_PACKET_BLOCK: {
        EnhancedPacketBlock epb{};
        PcapNgBlockParser::readEPB(&mData[mOffset], epb);
        break;
    }
    case MMPR_PACKET_BLOCK: {
        // deprecated in newer versions of PcapNG
        PacketBlock pb{};
        PcapNgBlockParser::readPB(&mData[mOffset], pb);
        break;
    }
    case MMPR_SIMPLE_PACKET_BLOCK: {
        MMPR_WARN("Parsing of Simple Packet Blocks not implemented, skipping\n");
        break;
    }
    case MMPR_NAME_RESOLUTION_BLOCK: {
        MMPR_WARN("Parsing of Name Resolution Blocks not implemented, skipping\n");
        break;
    }
    case MMPR_INTERFACE_STATISTICS_BLOCK: {
        InterfaceStatisticsBlock isb{};
        PcapNgBlockParser::readISB(&mData[mOffset], isb);
        break;
    }
    case MMPR_DECRYPTION_SECRETS_BLOCK: {
        MMPR_WARN("Parsing of Decryption Secrets Blocks not implemented, skipping\n");
        break;
    }
    case MMPR_CUSTOM_CAN_COPY_BLOCK: {
        MMPR_WARN("Parsing of Custom (Can Copy) Blocks not implemented, skipping\n");
        break;
    }
    case MMPR_CUSTOM_DO_NOT_COPY_BLOCK: {
        MMPR_WARN("Parsing of Custom (Do Not Copy) Blocks not implemented, skipping\n");
        break;
    }
    default: {
        MMPR_WARN_1("Encountered unknown block type: %u, skipping\n", blockType);
        break;
    }
    }

    // skip to next block
    mOffset += (size_t)blockTotalLength;

    return blockType;
}

} // namespace mmpr