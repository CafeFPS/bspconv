#pragma once

// string:
std::string FormatV(const char* const szFormat, const va_list args);
std::string Format(const char* const szFormat, ...);
std::string GetExtension(const std::string& svInput, const bool bReturnOriginal, const bool bKeepDelimiter);
std::string RemoveExtension(const std::string& svInput);
//std::string Base64Encode(const std::string& svInput);
std::string Base64Encode(const char* const buffer, const size_t size);
//std::string Base64Decode(const std::string& svInput);
std::vector<unsigned char> Base64Decode(const std::string& svInput);
std::vector<std::string> StringSplit(std::string svInput, const char cDelim, const size_t nMax = SIZE_MAX);
