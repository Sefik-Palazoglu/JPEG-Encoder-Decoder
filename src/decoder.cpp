#include <fstream>
#include <iostream>

#include "jpg.h"

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
    byte first = inFile.get();
    byte second = inFile.get();
    while (header->valid) {
        if (!inFile) {
            std::cout << "Error - file ended prematurely --" << filename << "--\n";
        }
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

        // TODO: Decode Huffman Encoded Bitstream

        delete header;
    }
    return 0;
}