#include <fstream>
#include <iostream>

#include "jpg.h"

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

        if (second == COM) {
            readComment(inFile, header);
        }

        if (second == DQT) {
            readQuantizationTable(inFile, header);
            break;
        }

        if (APP0 <= second && second <= APP15) {
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