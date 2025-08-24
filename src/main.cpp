// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "stdafx.h"
#include <CommandLine.h>
#include <binstream.h>
#include <bspfile.h>
#include <rmem.h>
#include <versions.h>
#include <filesystem>
#include <vector>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

// Function to scan recursively for .bsp files
std::vector<std::string> FindBspFiles(const std::string& directory = ".")
{
    std::vector<std::string> bspFiles;
    
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(directory))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                std::string extension = entry.path().extension().string();
                std::string fullPath = entry.path().string();
                
                // Skip files in depot folders
                if (fullPath.find("depot") != std::string::npos ||
                    fullPath.find("Depot") != std::string::npos ||
                    fullPath.find("DEPOT") != std::string::npos)
                {
                    continue;
                }
                
                // Convert extension to lowercase for comparison
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".bsp")
                {
                    bspFiles.push_back(fullPath);
                    printf("Found BSP file: %s\n", fullPath.c_str());
                }
            }
        }
    }
    catch (const fs::filesystem_error& ex)
    {
        printf("Error scanning directory: %s\n", ex.what());
    }
    
    return bspFiles;
}

// Function to replace ALL .new files in a directory with their originals
bool ReplaceNewFilesInDirectory(const std::string& directory)
{
    bool success = true;
    int filesReplaced = 0;
    
    try
    {
        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                
                // Check if this is a .new file
                if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".new")
                {
                    fs::path newFilePath = entry.path();
                    std::string originalFilename = filename.substr(0, filename.length() - 4); // Remove .new extension
                    fs::path originalFilePath = fs::path(directory) / originalFilename;
                    
                    try
                    {
                        // Delete original file if it exists
                        if (fs::exists(originalFilePath))
                        {
                            fs::remove(originalFilePath);
                        }
                        
                        // Rename .new file to original name
                        fs::rename(newFilePath, originalFilePath);
                        printf("Replaced: %s -> %s\n", filename.c_str(), originalFilename.c_str());
                        filesReplaced++;
                    }
                    catch (const fs::filesystem_error& ex)
                    {
                        printf("Error replacing file %s: %s\n", filename.c_str(), ex.what());
                        success = false;
                    }
                }
            }
        }
        
        if (filesReplaced > 0)
        {
            printf("Successfully replaced %d files in %s\n", filesReplaced, directory.c_str());
        }
    }
    catch (const fs::filesystem_error& ex)
    {
        printf("Error accessing directory %s: %s\n", directory.c_str(), ex.what());
        success = false;
    }
    
    return success;
}

// Function to replace original files with .new versions in a directory
bool ReplaceWithNewFiles(const std::string& directory = ".")
{
    bool success = true;
    int filesReplaced = 0;
    
    try
    {
        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                
                // Check if this is a .new file
                if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".new")
                {
                    fs::path newFilePath = entry.path();
                    std::string originalFilename = filename.substr(0, filename.length() - 4); // Remove .new extension
                    fs::path originalFilePath = fs::path(directory) / originalFilename;
                    
                    try
                    {
                        // Delete original file if it exists
                        if (fs::exists(originalFilePath))
                        {
                            fs::remove(originalFilePath);
                            printf("Deleted original: %s\n", originalFilename.c_str());
                        }
                        
                        // Rename .new file to original name
                        fs::rename(newFilePath, originalFilePath);
                        printf("Replaced: %s -> %s\n", filename.c_str(), originalFilename.c_str());
                        filesReplaced++;
                    }
                    catch (const fs::filesystem_error& ex)
                    {
                        printf("Error replacing file %s: %s\n", filename.c_str(), ex.what());
                        success = false;
                    }
                }
            }
        }
        
        printf("Successfully replaced %d original files with converted versions.\n", filesReplaced);
    }
    catch (const fs::filesystem_error& ex)
    {
        printf("Error accessing directory: %s\n", ex.what());
        success = false;
    }
    
    return success;
}

// Function to process a single BSP file
bool ProcessSingleBsp(const std::string& bspPath, bool shouldPack)
{
    printf("\n=== Processing: %s ===\n", bspPath.c_str());
    
    if (!FILE_EXISTS(bspPath.c_str()))
    {
        printf("ERROR: File not found: %s\n", bspPath.c_str());
        return false;
    }
    
    CIOStream bspIn;
    if (!bspIn.Open(bspPath, CIOStream::READ | CIOStream::BINARY))
    {
        printf("ERROR: Failed to open BSP file: %s\n", bspPath.c_str());
        return false;
    }
    
    const size_t fileSize = bspIn.GetSize();
    
    if (fileSize < sizeof(BSPHeader_t))
    {
        printf("ERROR: Input file is too small (must be at least 0x%x bytes): %s\n", 
               sizeof(BSPHeader_t), bspPath.c_str());
        return false;
    }
    
    // Read full BSP file into buffer
    char* const buf = new char[fileSize];
    bspIn.Read(buf, fileSize);
    
    try
    {
        ConvertBSP(bspPath, buf, shouldPack);
        delete[] buf;
        printf("SUCCESS: Converted %s\n", bspPath.c_str());
        return true;
    }
    catch (...)
    {
        delete[] buf;
        printf("ERROR: Failed to convert %s\n", bspPath.c_str());
        return false;
    }
}

// Function to perform batch conversion
bool BatchConvert(bool shouldPack)
{
    printf("\n=== RECURSIVE BATCH CONVERSION MODE ===\n");
    printf("Scanning recursively for .bsp files...\n\n");
    
    std::vector<std::string> bspFiles = FindBspFiles();
    
    if (bspFiles.empty())
    {
        printf("No .bsp files found in directory tree.\n");
        return true; // Not an error
    }
    
    printf("\nFound %zu .bsp file(s). Starting recursive batch conversion...\n", bspFiles.size());
    
    int successCount = 0;
    int failureCount = 0;
    int replacedCount = 0;
    
    // Process each BSP file
    for (size_t i = 0; i < bspFiles.size(); ++i)
    {
        const std::string& bspFile = bspFiles[i];
        printf("\n[%zu/%zu] ", i + 1, bspFiles.size());
        
        if (ProcessSingleBsp(bspFile, shouldPack))
        {
            successCount++;
            // Get the directory containing the BSP file and replace ALL .new files there
            std::string bspDirectory = fs::path(bspFile).parent_path().string();
            printf("Replacing .new files in: %s\n", bspDirectory.c_str());
            if (ReplaceNewFilesInDirectory(bspDirectory))
            {
                replacedCount++;
            }
        }
        else
        {
            failureCount++;
        }
    }
    
    printf("\n=== RECURSIVE BATCH CONVERSION COMPLETE ===\n");
    printf("Total files found: %zu\n", bspFiles.size());
    printf("Successfully converted: %d\n", successCount);
    printf("Successfully replaced: %d\n", replacedCount);
    printf("Failed conversions: %d\n", failureCount);
    
    return failureCount == 0;
}

int main(int argc, char** argv)
{
    printf("bspconv - Copyright (c) %s, rexx\n", &__DATE__[7]);

    const CommandLine cmdline(argc, argv);

    // Check for batch mode
    if (cmdline.HasParam("-batch"))
    {
        printf("\n");
        bool shouldPack = cmdline.HasParam("-pack") || cmdline.HasParam("1");
        return BatchConvert(shouldPack) ? 0 : 1;
    }

    // Original single file mode
    if (argc < 2)
    {
        printf("\nUsage:\n");
        printf("  Single file: bspconv <fileName> [shouldPack]\n");
        printf("  Batch mode:  bspconv -batch [-pack]\n");
        printf("\n");
        printf("Options:\n");
        printf("  -batch       Process all .bsp files recursively\n");
        printf("  -pack        Pack all lumps (optional, works in both modes)\n");
        printf("  shouldPack   1 to pack lumps (single file mode only)\n");
        printf("\n");
        Error("Invalid usage. See usage information above.\n");
    }

    if (!FILE_EXISTS(argv[1]))
        Error("couldn't find input file \"%s\"\n", argv[1]);

    const std::string bspPath = argv[1];

    CIOStream bspIn;
    if (!bspIn.Open(bspPath, CIOStream::READ | CIOStream::BINARY))
        Error("failed to open BSP file \"%s\"\n", bspPath.c_str());

    const size_t fileSize = bspIn.GetSize();

    if (fileSize < sizeof(BSPHeader_t))
        Error("input file is too small (must be at least 0x%x bytes\n", sizeof(BSPHeader_t));

    // read full bsp file into buffer to pass to each version func
    char* const buf = new char[fileSize];
    bspIn.Read(buf, fileSize);

    ConvertBSP(bspPath, buf, (argc > 2));
    
    printf("\nConversion completed successfully.\n");
    return 0;
}