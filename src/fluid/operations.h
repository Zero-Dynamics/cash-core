// Copyright (c) 2017 Duality Blockchain Solutions Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "base58.h"

#include <algorithm>
#include <stdint.h>
#include <string.h>
#include <vector>

class HexFunctions
{
public:
    std::string StringToHex(const std::string& input)
    {
        static const char* const lut = "0123456789ABCDEF";
        size_t len = input.length();
        std::string output;
        output.reserve(2 * len);
        for (size_t i = 0; i < len; ++i) {
            const unsigned char c = input[i];
            output.push_back(lut[c >> 4]);
            output.push_back(lut[c & 15]);
        }

        return output;
    }

    std::string HexToString(const std::string& hex)
    {
        int len = hex.length();
        std::string newString;
        for (int i = 0; i < len; i += 2) {
            std::string byte = hex.substr(i, 2);
            char chr = (char)(int)strtol(byte.c_str(), nullptr, 16);
            newString.push_back(chr);
        }

        return newString;
    }

    void ConvertToHex(std::string& input)
    {
        std::string output = StringToHex(input);
        input = output;
    }
    void ConvertToString(std::string& input)
    {
        std::string output = HexToString(input);
        input = output;
    }
};

void ScrubString(std::string& input, bool forInteger = false);
void SeparateString(const std::string& input, std::vector<std::string>& output, bool subDelimiter = false);
void SeparateFluidOpString(const std::string& input, std::vector<std::string>& output);
std::string StitchString(const std::string& stringOne, const std::string& stringTwo, const bool subDelimiter = false);
std::string StitchString(const std::string& stringOne, const std::string& stringTwo, const std::string& stringThree, const bool subDelimiter = false);
std::string GetRidOfScriptStatement(const std::string& input, const int& position = 1);

extern std::string PrimaryDelimiter;
extern std::string SubDelimiter;
extern std::string SignatureDelimiter;

class COperations : public HexFunctions
{
public:
    bool VerifyAddressOwnership(const CDebitAddress& debitAddress);
    bool SignTokenMessage(const CDebitAddress& address, std::string unsignedMessage, std::string& stitchedMessage, bool stitch = true);
    bool GenericSignMessage(const std::string& message, std::string& signedString, const CDebitAddress& signer);
};

#endif // OPERATIONS_H
