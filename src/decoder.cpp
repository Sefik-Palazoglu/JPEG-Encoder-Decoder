#include <fstream>
#include <iostream>

#include "jpg.h"

void readStartOfScan(std::ifstream& inFile, Header* header) {
    std::cout << "Reading SOS marker\n";
    if (header->numOfComponents == 0) {
        std::cout << "Error - SOS detected before SOF\n";
        header->valid = false;
        return;
    }

    uint length = ((inFile.get() << 8) + inFile.get());

    // recall that reading start of frame we set all of the used flags on our color components to true 
    // to keep track of which color components we successfully read the quantization table id for
    // we want to use that used flag the same way here when reading the huffman table ids but before
    // we do that we have to set all of the used flags back to false
    for (int i = 0; i < header->numOfComponents; i++) {
        header->colorComponents[i].used = false;
    }

    byte numComponents = inFile.get();
    for (int i = 0; i < numComponents; i++) {
        byte componentID = inFile.get();
        if (header->zeroBased) {
            componentID += 1;
        }

        // color components are from 1 to 3 but our indexes are 0 to 2
        ColorComponent* component = &header->colorComponents[componentID - 1];
        if (component->used) {
            std::cout << "Error - Duplicate Color Component ID\n";
            header->valid = false;
            return;
        }
        component->used = true;

        byte huffmanTableIDs = inFile.get();
        std::cout << "xdd-- component: " << (uint)componentID << " huffmanTableIDs: " << std::hex << (uint)huffmanTableIDs << std::dec << "\n";
        component->huffmanDCTableID = (huffmanTableIDs >> 4);
        component->huffmanACTableID = (huffmanTableIDs & 0x0F);
        std::cout << "xdd-- component: " << (uint)componentID << " huffmanTableIDs: " << std::hex << (uint)component->huffmanDCTableID << std::dec << "\n";
        std::cout << "xdd-- component: " << (uint)componentID << " huffmanTableIDs: " << std::hex << (uint)component->huffmanACTableID << std::dec << "\n";
        
        if (component->huffmanACTableID > 3 || component->huffmanDCTableID > 3) {
            std::cout << "Error - Invalit Huffman DC or AC TableID\n";
            header->valid = false;
            return;
        }
    }
    header->startOfSelection = inFile.get();
    header->endOfSelection = inFile.get();
    byte successiveApproximation = inFile.get();
    header->successiveApproximationHigh = successiveApproximation >> 4;
    header->successiveApproximationLow = (successiveApproximation & 4);

    // Baseline JPGs do not use spectral selection of successive approximation
    if (header->startOfSelection != 0 || header->endOfSelection != 63) {
        std::cout << "Error - Invalid spectral selection for baseline jpeg\n";
        header->valid = false;
        return;
    }

    if (header->successiveApproximationHigh != 0 || header->successiveApproximationLow != 0) {
        std::cout << "Error - Invalid successive approximation fo r baseline jpeg\n";
        header->valid = false;
        return;
    }

    if (length - 6 - (2 * numComponents) != 0) {
        std::cout << "Error - Invalid SOS Marker\n";
        header->valid = false;
        return;
    }
}

void readStartOfFrame(std::ifstream& inFile, Header* header) {
    std::cout << "Reading SOF marker\n";
    if (header->numOfComponents != 0) {
        std::cout << "Error - Multiple SOFs are deteceted.\n";
        header->valid = false;
        return;
    }

    uint length = ((inFile.get() << 8) + inFile.get());
    std::cout << "length: " << (uint)length << '\n';

    byte precision = inFile.get();
    if (precision != 8) {
        std::cout << "Error - Invalid precision\n";
        header->valid = false;
        return;
    }

    header->height = ((inFile.get() << 8) + inFile.get());
    header->width = ((inFile.get() << 8) + inFile.get());
    if (header->height == 0 || header->width == 0) {
        std::cout << "Error - Invalid height or width\n";
        header->valid = false;
        return;
    }

    header->numOfComponents = inFile.get();
    if (header->numOfComponents == 4) {
        std::cout << "Error - CMYK components not supported.\n";
        header->valid = false;
        return; 
    }
    if (header->numOfComponents == 0) {
        std::cout << "Error - 0 components not supported.\n";
        header->valid = false;
        return; 
    }

    for (int i = 0; i < header->numOfComponents; i++) {
        byte componentID = inFile.get();
        if (componentID == 0) {     // component id is illegally start from 0, allow this
            header->zeroBased = true;   // check this flag in the future to add 1 to componentIDs
        }
        if (header->zeroBased) {
            componentID += 1;
        }
        if (componentID == 4 || componentID == 5) {
            std::cout << "Error - YIQ format not supported\n";
            header->valid = false;
            return; 
        }
        if (componentID == 0 || componentID > 3) {
            std::cout << "Error - invalid component id\n";
            header->valid = false;
            return; 
        }

        // color components for Y Cr Cb format is 1, 2, 3. So we subtract 1 from those
        ColorComponent* component = &header->colorComponents[componentID - 1];
        if (component->used) {
            std::cout << "Error - duplicate color component\n";
            header->valid = false;
            return; 
        }
        component->used = true;
        byte samplingFactor = inFile.get();
        component->horizontalSamplingFactor = samplingFactor >> 4;
        component->verticalSamplingFactor = samplingFactor & 0x0F;

        component->quantizationTableID = inFile.get();
        if (component->quantizationTableID > 3) {
            std::cout << "Error - invalid quantization table id for color component\n";
            header->valid = false;
            return; 
        }
    }

    // check if length lines up
    if (length - 8 - (header->numOfComponents * 3) != 0) {
        std::cout << "Error - invalid SOF marker\n";
        header->valid = false;
        return; 
    }

}

void readAPPN(std::ifstream& inFile, Header* header) {
    std::cout << "Reading APPN marker\n";
    uint length = ((inFile.get() << 8) + inFile.get());
    std::cout << "length: " << (uint)length << '\n';
    
    // length which is 2 bytes is included in length
    for (int i = 0; i < length - 2; i++) {
        inFile.get();
    }
}

void readQuantizationTable(std::ifstream& inFile, Header* header) {
    std::cout << "Reading DQT marker\n";
    // using in length, length should be signed
    int length = ((inFile.get() << 8) + inFile.get());
    length -= 2;

    while (length > 0) {
        byte tableInfo = inFile.get();
        length -= 1;
        byte tableID = tableInfo & 0x0F;
        tableID = (tableInfo & 0x0F);

        if (tableID > 3) {
            std::cout << "Error - Table id cannot be greater than 3. tableID: " << (uint)tableID << "\n";
            header->valid = false;
            return;
        }

        header->quantizationTables[tableID].set = true;

        // 0 -> 8 bits; 1 -> 16 bits
        byte entrySize = ((tableInfo >> 4) & 0x0F);

        if (entrySize == 0) {   // 8 bits per data
            for (int i = 0; i < 64; i++) {
                header->quantizationTables[tableID].table[zigZagMap[i]] = inFile.get();
            }
            length -= 64;
        } else if (entrySize == 1) { // 16 bits per data
            for (int i = 0; i < 64; i++) {
                header->quantizationTables[tableID].table[zigZagMap[i]] = ((inFile.get() << 8) + inFile.get());
            }
            length -= 128;
        }
    }

    if (length != 0) {
        std::cout << "Error - invalid DQT Marker\n";
        header->valid = false;
        return;
    }
}

void readComment(std::ifstream& inFile, Header* header) {
    std::cout << "Reading COM marker\n";
    uint length = ((inFile.get() << 8) + inFile.get());
    std::cout << "length: " << (uint)length << '\n';

    // length which is 2 bytes is included in length
    for (int i = 0; i < length - 2; i++) {
        inFile.get();
    }
}

void readHuffmanTable(std::ifstream& inFile, Header* header) {
    std::cout << "Reading DHT marker\n";
    int length = ((inFile.get() << 8) + inFile.get());
    std::cout << "length: " << (uint)length << '\n';
    length -= 2;

    while (length > 0) {
        byte tableInfo = inFile.get();

        byte tableID = tableInfo & 0x0F;
        tableID = (tableInfo & 0x0F);
        bool ACTable = tableInfo >> 4;

        if (tableID > 3) {
            std::cout << "Error - Table id cannot be greater than 3. tableID: " << (uint)tableID << "\n";
            header->valid = false;
            return;
        }

        HuffmanTable* hTable;
        if (ACTable) {
            hTable = &header->huffmanACTables[tableID];
        } else {    // DCTable
            hTable = &header->huffmanDCTables[tableID];
        }
        hTable->set = true;

        hTable->offsets[0] = 0;
        uint allSymbols = 0;        // running sum of symbol count
        for (int i = 1; i <= 16; i++) {
            allSymbols += inFile.get();
            hTable->offsets[i] = allSymbols;
        }

        if (allSymbols > 162) {
            std::cout << "Error - Too many symbols in Huffman Table\n";
            header->valid = false;
            return;
        }

        for (int i = 0; i < allSymbols; i++) {
            hTable->symbols[i] = inFile.get();
        }

        length -= (1 + 16 + allSymbols);      // subtract read bytes 1: TableInfo, 16: symbolCounts, allSymbols: symbols
    }
    
    if (length != 0) {
        std::cout << "Error - DHT invalid\n";
        return;
    }
}

void readRestartInterval(std::ifstream& inFile, Header* header) {
    std::cout << "Reading DRI marker\n";
    uint length = ((inFile.get() << 8) + inFile.get());
    std::cout << "length: " << (uint)length << '\n';

    header->restartInterval = ((inFile.get() << 8) + inFile.get());

    if (length != 4) {
        std::cout << "Error - invalid DRI Marker\n";
        header->valid = false;
        return;
    }
}

void printHeader(const Header* const header) {
    if (header == nullptr) return;
    if (header->valid == false) return;
    std::cout << "DQT ====================\n";
    for (int i = 0; i < 4; i++) {
        if (header->quantizationTables[i].set) {
            std::cout << "Table ID: " << i << "\n";
            std::cout << "Table Data:";
            for (int j = 0; j < 64; j++) {
                if (j % 8 == 0) {
                    std::cout << "\n";
                }
                std::cout << header->quantizationTables[i].table[j] << '\t';
            }
            std::cout << "\n";
        }
    }

    std::cout << "SOF ======================\n";
    std::cout << "Frame Type 0x" << std::hex << (uint) header->frameType << std::dec << "\n";
    std::cout << "Height: " << header->height << "\n";
    std::cout << "Width: " << header->width << "\n";
    std::cout << "Color Components: \n"; 
    for (int i = 0; i < header->numOfComponents; i++) {
        std::cout << "Component ID: " << (i + 1) << "\n";
        std::cout << "horizontalSamplingFactor: " << (uint)header->colorComponents[i].horizontalSamplingFactor << "\n";
        std::cout << "verticalSamplingFactor: " << (uint)header->colorComponents[i].verticalSamplingFactor << "\n";
        std::cout << "quantizationTableID: " << (uint)header->colorComponents[i].quantizationTableID << "\n";
    }
    std::cout << "DHT =====================\n";
    std::cout << "DC Tables :\n";
    for (int i = 0; i < 4; i++) {
        if (header->huffmanDCTables[i].set) {
            std::cout << "Table ID: " << i << '\n';
            std::cout << "Symbols:\n";
            for (int j = 0; j < 16; j++) {  // for all possible 16 lengths
                std::cout << (j + 1) << ": ";
                for (int k = header->huffmanDCTables[i].offsets[j]; k < header->huffmanDCTables[i].offsets[j + 1]; k++) {
                    std::cout << std::hex << (uint)header->huffmanDCTables[i].symbols[k] << ' ' << std::dec;
                }
                std::cout << '\n';
            }
        }
    }

    std::cout << "AC Tables :\n";
    for (int i = 0; i < 4; i++) {
        if (header->huffmanACTables[i].set) {
            std::cout << "Table ID: " << i << '\n';
            std::cout << "Symbols:\n";
            for (int j = 0; j < 16; j++) {  // for all possible 16 lengths
                std::cout << (j + 1) << ": ";
                for (int k = header->huffmanACTables[i].offsets[j]; k < header->huffmanACTables[i].offsets[j + 1]; k++) {
                    std::cout << std::hex << (uint)header->huffmanACTables[i].symbols[k] << ' ' << std::dec;
                }
                std::cout << '\n';
            }
        }
    }

    std::cout << "SOS==========================\n";
    std::cout << "Start of Selection: " << (uint)header->startOfSelection << "\n";
    std::cout << "End of Selection: " << (uint)header->endOfSelection << "\n";
    std::cout << "Successive Approximation High: " << (uint)header->successiveApproximationHigh << "\n";
    std::cout << "Successive Approximation Low: " << (uint)header->successiveApproximationLow << "\n";
    std::cout << "Color Components: \n";
    for (int i = 0; i < header->numOfComponents; i++) {
        // color component ids are from 1 to 3.
        std::cout << "Component id: " << (i + 1) << "\n";
        std::cout << "Huffman DC TableID: " << (uint)header->colorComponents[i].huffmanDCTableID << "\n";
        std::cout << "Huffman AC TableID: " << (uint)header->colorComponents[i].huffmanACTableID << "\n";
    }

    std::cout << "Length of the huffman data: " << header->huffmanData.size() << "\n";
    std::cout << "DRI==============================\n";
    std::cout << "Restart Interval: " << (uint)header->restartInterval << "\n";
}

Header* readJPG(const std::string& filename)
{
    // Open file in input and binary format
    std::ifstream inFile = std::ifstream(filename, std::ios::in | std::ios::binary);
    if (!inFile.is_open()) {
        std::cout << "Error, input file cannot be opened --" << filename << "--\n";
        return nullptr;
    }

    Header* header = new (std::nothrow) Header;
    if (header == nullptr) {
        std::cout << "Error, memory could not be allocated for Header.\n";
        inFile.close();
        return nullptr;
    }

    // read 2 bytes
    byte first = inFile.get();
    byte second = inFile.get();
    // verify
    if (first != 0xFF || second != SOI) {
        header->valid = false;
        inFile.close();
        return header;
    }

    int huffmanTablesRead = 0;
    // read 2 bytes
    first = inFile.get();
    second = inFile.get();
    while (header->valid) {
        if (!inFile) {
            std::cout << "Error - file ended prematurely --" << filename << "--\n";
            header->valid = false;
            inFile.close();
            return header;
        }
        if (first != 0xFF) {
            std::cout << "Error - Marker was expected --" << filename << "--\n";
            header->valid = false;
            inFile.close();
            return header;
        }

        if (second == SOS) {
            readStartOfScan(inFile, header);
            break;
        } else if (second == DHT) {
            readHuffmanTable(inFile, header);
        } else if (second == SOF0) {
            header->frameType = SOF0;
            readStartOfFrame(inFile, header);
        } else if (second == COM) {
            readComment(inFile, header);
        } else if (second == DQT) {
            readQuantizationTable(inFile, header);
        } else if (second == DRI) {
            readRestartInterval(inFile, header);
        } else if (APP0 <= second && second <= APP15) {
            readAPPN(inFile, header);
        }
        // unused markers that can be skipped
        else if ((second >= JPG0 && second <= JPG13) || second == DNL || second == DHP ||
                second == EXP) {
            readComment(inFile, header);
        }
        else if (second == TEM) {
            // TEM has no size
        }
        // any number of 0xFF in a row are allowed and should be skipped
        else if (second == 0xFF) {
            second = inFile.get();
            continue;
        }
        else if (second == SOI) {
            std::cout << "Error - Start of Image not supported\n";
            header->valid = false;
            inFile.close();
            return header;
        }
        else if (second == EOI) {
            std::cout << "Error - EOI encountered before SOS\n";
            header->valid = false;
            inFile.close();
            return header;
        }
        else if (second == DAC) {
            std::cout << "Error - Arithmetic encoding not supported\n";
            header->valid = false;
            inFile.close();
            return header;
        }
        else if (second >= SOF1 && second <= SOF15) {
            std::cout << "Error - Given SOF not supported SOF 0x" << std::hex << (uint)second << std::dec << '\n';
            header->valid = false;
            inFile.close();
            return header;
        }
        else {
            std::cout << "Error - Unknown marker 0x" << std::hex << (uint)second << std::dec << '\n';
            header->valid = false;
            inFile.close();
            return header;

        }

        first = inFile.get();
        second = inFile.get();
    }

    // After Start of Scan (SOS)

    if (header->valid) {
        second = inFile.get();

        while (true) {
            if (!inFile) {
                std::cout << "Error - File ended prematurely\n";
                header->valid = false;
                inFile.close();
                return header;
            }
            first = second;
            second = inFile.get();

            // if marker is found
            if (first == 0xFF) {
                // end of image
                if (second == EOI) {
                    break;
                }
                // actual 0xFF value to be stored
                else if (second == 0x00) {
                    header->huffmanData.push_back(first);
                    // overwrite 0x00 with next byte
                    second = inFile.get();
                }
                // restart marker
                else if (RST0 <= second && second <= RST7) {
                    // overwrite marker with next byte
                    second = inFile.get();
                }
                //ignore multiple FFs in a row
                else if (second == 0xFF) {
                    // do nothing
                    continue;
                }
                else {
                    std::cout << "Error - invalid marker during compressed data scan 0x" << std::hex << (uint)second << std::dec << '\n';
                    header->valid = false;
                    inFile.close();
                    return header;
                }
            } else {    // if first != 0xFF
                // just store the data
                header->huffmanData.push_back(first);
            }
        }
    }

    // validate header info
    if (header->numOfComponents != 0 && header->numOfComponents != 3) {
        std::cout << "Error - " << (uint)header->numOfComponents << " color components given (1 or 3 required)\n";
        header->valid = false;
        inFile.close();
        return header;
    }

    for (int i = 0; i < header->numOfComponents; i++) {
        if (header->quantizationTables[header->colorComponents[i].quantizationTableID].set == false) {
            std::cout << "Error - Color component using uninitialized quantization table";
            header->valid = false;
            inFile.close();
            return header;
        }
        if (header->huffmanDCTables[header->colorComponents[i].huffmanDCTableID].set == false) {
            std::cout << "Error - Color component using uninitialized huffman DC table";
            header->valid = false;
            inFile.close();
            return header;
        }
        if (header->huffmanACTables[header->colorComponents[i].huffmanACTableID].set == false) {
            std::cout << "Error - Color component using uninitialized huffman AC table";
            header->valid = false;
            inFile.close();
            return header;
        }
    }

    inFile.close();
    return header;
}

void generateCodes(HuffmanTable& hTable) {
    uint code = 0;
    // i is current code length - 1
    for (uint i = 0; i < 16; i++) {
        for (uint j = hTable.offsets[i]; j < hTable.offsets[i + 1]; j++) {
            hTable.codes[j] = code;
            code += 1;
        }
        // append a 0 to right end of the code candidate.
        code <<= 1;
    }
}

MCU* decodeHuffmanData(Header* const header) {
    const int mcuHeight = (header->height + 7) / 8;
    const int mcuWidth = (header->width + 7) / 8;
    MCU* mcus = new (std::nothrow) MCU[mcuHeight * mcuWidth];
    if (mcus == nullptr) {
        std::cout << "Error - memory error.\n";
        return nullptr;
    }

    // generate codes for huffman tables
    for (int i = 0; i < 4; i++) {
        if (header->huffmanDCTables[i].set) {
            generateCodes(header->huffmanDCTables[i]);
        }
    }

    return mcus;
}

// little endian
void putInt(std::ofstream& outFile, const int v) {
    outFile.put((v >> 0) & 0xFF);
    outFile.put((v >> 8) & 0xFF);
    outFile.put((v >> 16) & 0xFF);
    outFile.put((v >> 24) & 0xFF);
}

// little endian
void putShort(std::ofstream& outFile, const int v) {
    outFile.put((v >> 0) & 0xFF);
    outFile.put((v >> 8) & 0xFF);
}

void writeBMP(const Header* const header, const MCU* const mcus, const std::string& filename) {
    // open file
    std::ofstream outFile = std::ofstream(filename, std::ios::out | std::ios::binary);
    if (!outFile.is_open()) {
        std::cout << "Output file couldn't be opened\n";
        return;
    }

    const int mcuHeight = (header->height + 7) / 8;
    const int mcuWidth = (header->width + 7) / 8;
    const int paddingSize = header->width % 4;
    const int size = 12 + 14 + (header->height * header->width) * 3 + paddingSize * header->height;

    outFile.put('B');
    outFile.put('M');

    putInt(outFile, size);
    putInt(outFile, 0);
    putInt(outFile, 0x1A);

    putInt(outFile, 12);
    putShort(outFile, header->width);
    putShort(outFile, header->height);
    putShort(outFile, 1);   // planes
    putShort(outFile, 24);  // bits per pixel

    for (int y = header->height - 1; y >= 0; y--) {
        const int mcuRow = y / 8;
        const int pixelRow = y % 8;
        for (int x = 0; x < header->height; x++) {
            const int mcuCol = x / 8;
            const int pixelCol = x % 8;
            const int mcuIndex = mcuRow * mcuWidth + mcuCol; 
            const int pixelIndex = pixelRow * 8 + pixelCol; 
            outFile.put(mcus[mcuIndex].b[pixelIndex]);
            outFile.put(mcus[mcuIndex].g[pixelIndex]);
            outFile.put(mcus[mcuIndex].r[pixelIndex]);
        }
        for (int i = 0; i < paddingSize; i++) {
            outFile.put(0);
        }
    }

    outFile.close();
}

int main(int argc, char** argv) 
{
    if (argc < 2) {
        std::cout << "Error, invalid number of arguments\n";
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        const std::string filename{argv[i]};
        Header* header = readJPG(filename);

        if (header == nullptr) {
            continue;
        }
        else if (header->valid == false) {
            std::cout << "Error - invalid header in --" << filename << "--\n";
            delete header;
            continue;
        }

        printHeader(header);

        // TODO: Decode Huffman Encoded Bitstream
        MCU* mcus = decodeHuffmanData(header);
        if (mcus == nullptr) {
            delete header;
            continue;
        }

        // write the BMP file
        const std::size_t pos = filename.find_last_of('.');
        const std::string outFilename = (pos == std::string::npos) ? (filename + ".bmp") : (filename.substr(0, pos) + ".bmp");

        writeBMP(header, mcus, outFilename);


        delete[] mcus;
        delete header;
    }
    return 0;
}