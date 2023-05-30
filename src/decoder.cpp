#include <fstream>
#include <iostream>

#include "jpg.h"

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
        if (huffmanTablesRead == 4) {
            break;
        }

        if (second == DHT) {
            readHuffmanTable(inFile, header);
            huffmanTablesRead++;
        }
        else if (second == SOF0) {
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

        first = inFile.get();
        second = inFile.get();
    }

    return header;
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

        delete header;
    }
    return 0;
}