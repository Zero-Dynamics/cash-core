// Copyright (c) 2019-2021 Duality Blockchain Solutions Developers

#ifndef FLUID_DB_H
#define FLUID_DB_H

#include "amount.h"

class CDebitAddress;
class CFluidMasternode;
class CFluidMining;
class CFluidMint;
class CFluidSovereign;

CAmount GetFluidMasternodeReward(const int nHeight);
CAmount GetFluidMiningReward(const int nHeight);
bool GetMintingInstructions(const int nHeight, CFluidMint& fluidMint);
bool IsSovereignAddress(const CDebitAddress& inputAddress);
bool GetAllFluidMasternodeRecords(std::vector<CFluidMasternode>& masternodeEntries);
bool GetAllFluidMiningRecords(std::vector<CFluidMining>& miningEntries);
bool GetAllFluidMintRecords(std::vector<CFluidMint>& mintEntries);
bool GetAllFluidSovereignRecords(std::vector<CFluidSovereign>& sovereignEntries);
bool GetLastFluidSovereignAddressStrings(std::vector<std::string>& sovereignAddresses);
bool CheckSignatureQuorum(const std::vector<unsigned char>& vchFluidScript, std::string& errMessage, bool individual = false);

#endif // FLUID_DB_H
