#include "Common.h"
#include "Core.h"

namespace Core
{
	// Finds all locations of a signature
	std::vector<DWORD> FindSignature(LPBYTE lpBuffer, DWORD dwBufferSize, LPBYTE lpSignature, DWORD dwSignatureSize, LPBYTE lpWildCards)
	{
		std::vector<DWORD> addresses;
		for (DWORD i = 0; i < dwBufferSize; ++i) {
			bool bFound = true;
			// Loop through each byte in the signature
			for (DWORD j = 0; j < dwSignatureSize; ++j)	{
				// Check if the index overruns the buffer
				if (i + j >= dwBufferSize){
					bFound = false;
					break;
				}

				if (lpWildCards) {
					// Check if the buffer does not equal the signature and a wild card is not set
					if (lpBuffer[i + j] != lpSignature[j] && !lpWildCards[j]){
						bFound = false;
						break;
					}
				}
				else {
					if (lpBuffer[i + j] != lpSignature[j]){
						bFound = false;
						break;
					}
				}
			}

			if (bFound)
				addresses.push_back(i);
		}

		return addresses;
	}

	// Finds the location of a signature
	DWORD FindAddress(LPBYTE lpBuffer, DWORD dwBufferSize, LPBYTE lpSignature, DWORD dwSignatureSize, LPBYTE lpWildCards, DWORD dwIndex, DWORD dwOffset)
	{
		DWORD dwAddress = 0;
		std::vector<DWORD> addresses = FindSignature(lpBuffer, dwBufferSize, lpSignature, dwSignatureSize, lpWildCards);
		if (addresses.size() - 1 >= dwIndex)
			dwAddress = (DWORD)lpBuffer + addresses[dwIndex] + dwOffset;

		return dwAddress;
	}

	// Creates a code cave to a function at a specific address
	BOOL CreateCodeCave(DWORD dwAddress, BYTE cbSize, VOID (*pFunction)())
	{
		BOOL bResult = TRUE;

		if (cbSize < 5)
			return FALSE;

		// Calculate the offset from the function to the address
		DWORD dwOffset = PtrToUlong(pFunction) - dwAddress - 5;

		// Construct the call instruction to the offset
		BYTE patch[0xFF] = {0x90};
		patch[0] = 0xE8;
		memcpy(patch + 1, &dwOffset, sizeof(dwAddress));

		// Write the code cave to the address
		bResult &= Common::WriteBytes(dwAddress, patch, cbSize);

		return bResult;
	}
}