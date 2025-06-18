#include "stdafx.h"
#include "stltools.h"

///////////////////////////////////////////////////////////////////////////////
// For formatting a STL string using C-style format specifiers (va_list version).
std::string FormatV(const char* const szFormat, const va_list args)
{
    // Initialize use of the variable argument array.
    va_list argsCopy;
    va_copy(argsCopy, args);

    // Dry run to obtain required buffer size.
    const int iLen = std::vsnprintf(nullptr, 0, szFormat, argsCopy);
    va_end(argsCopy);

    assert(iLen >= 0);
    std::string result;

    if (iLen < 0)
    {
        result.clear();
    }
    else
    {
        result.resize(iLen);
        std::vsnprintf(&result[0], iLen + sizeof(char), szFormat, args);
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////
// For formatting a STL string using C-style format specifiers.
std::string Format(const char* const szFormat, ...)
{
    std::string result;

    va_list args;
    va_start(args, szFormat);
    result = FormatV(szFormat, args);
    va_end(args);

    return result;
}

///////////////////////////////////////////////////////////////////////////////
// For removing file names from the extension.
std::string GetExtension(const std::string& svInput, const bool bReturnOriginal, const bool bKeepDelimiter)
{
    std::string::size_type nPos = svInput.rfind('.');

    if (nPos != std::string::npos)
    {
        if (!bKeepDelimiter)
        {
            nPos += 1;
        }
        return svInput.substr(nPos);
    }
    if (bReturnOriginal)
    {
        return svInput;
    }
    return "";
}

///////////////////////////////////////////////////////////////////////////////
// For removing extensions from file names.
std::string RemoveExtension(const std::string& svInput)
{
    const std::string::size_type nPos = svInput.find_last_of('.');

    if (nPos == std::string::npos)
    {
        return svInput;
    }
    return svInput.substr(0, nPos);
}

///////////////////////////////////////////////////////////////////////////////
// For encoding data in Base64.
std::string Base64Encode(const char* const buffer, const size_t size)
{
    std::string result;
    int val = 0, valb = -6;

    size_t i = 0;

    while (i < size)
    {
        unsigned char c = buffer[i];
        i++;

        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            result.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
    {
        result.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (result.size() % 4)
    {
        result.push_back('=');
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////
// For decoding data in Base64.
std::vector<unsigned char> Base64Decode(const std::string& svInput)
{
    std::vector<unsigned char> result;
    int val = 0, valb = -8;

    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
    {
        T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    }

    for (unsigned char c : svInput)
    {
        if (T[c] == -1)
        {
            break;
        }
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0)
        {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////
// For splitting a string into substrings by delimiter.
std::vector<std::string> StringSplit(std::string svInput, const char cDelim, const size_t nMax)
{
    std::string svSubString;
    std::vector<std::string> vSubStrings;

    svInput = svInput + cDelim;

    for (size_t i = 0; i < svInput.size(); i++)
    {
        if ((i != (svInput.size() - 1) && vSubStrings.size() >= nMax)
            || svInput[i] != cDelim)
        {
            svSubString += svInput[i];
        }
        else
        {
            if (svSubString.size() != 0)
            {
                vSubStrings.push_back(svSubString);
            }
            svSubString.clear();
        }
    }
    return vSubStrings;
}