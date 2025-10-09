#ifndef STL_READER_H
#define STL_READER_H

#include "core/common/common_types.h"
#include <iostream>

#include <cstdint>
#include <fstream>
#pragma pack(push, 1)

namespace  STL_reader{


std::vector<Triangle> readBinaryStl(const std::string& filePath){

    std::vector<Triangle> triangles;
    // Open the file in binary mode
        std::ifstream file(filePath, std::ios::binary);

        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filePath << std::endl;
            return triangles; // Return an empty vector
        }

        // 1. Read the header (80 bytes)
        char header[80];
        file.read(header, 80);
        // We can choose to ignore the header content
        // std::cout << "File Header: " << std::string(header, 80) << std::endl;

        // 2. Read the number of triangles (4 bytes)
        uint32_t numTriangles = 0;
        file.read(reinterpret_cast<char*>(&numTriangles), sizeof(numTriangles));

        if (numTriangles == 0) {
            std::cout << "No triangles found in the file." << std::endl;
            return triangles;
        }

        std::cout << "Expecting to read " << numTriangles << " triangles..." << std::endl;

        // Pre-allocate memory for efficiency
        triangles.resize(numTriangles);

        // 3. Read all triangle data in one go
        // We read the file content directly into the vector's memory
        file.read(reinterpret_cast<char*>(triangles.data()), numTriangles * sizeof(Triangle));

        // Check if the read was successful and complete
        if (file.gcount() != numTriangles * sizeof(Triangle)) {
             std::cerr << "Error: File size does not match the triangle count in the header." << std::endl;
             triangles.clear(); // Clear the data as it might be corrupt
        } else {
             std::cout << "Successfully read " << triangles.size() << " triangles." << std::endl;
        }

        file.close();
        return triangles;
}

}
#endif // STL_READER_H
