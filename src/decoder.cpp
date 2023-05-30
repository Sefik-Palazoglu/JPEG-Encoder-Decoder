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
    return nullptr;
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
    }
    return 0;
}