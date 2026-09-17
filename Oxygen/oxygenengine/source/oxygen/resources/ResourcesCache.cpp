/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2026 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "oxygen/pch.h"
#include "oxygen/resources/ResourcesCache.h"
#include "oxygen/resources/PaletteCollection.h"
#include "oxygen/resources/RawDataCollection.h"
#include "oxygen/application/Configuration.h"
#include "oxygen/engine/modding/ModManager.h"
#include "oxygen/file/ZipFileProvider.h"
#include "oxygen/helper/Logging.h"
#include "oxygen/platform/PlatformFunctions.h"


namespace
{
	// Minimal read-only ISO 9660 directory walker, just enough to locate one file by name
	// anywhere on the disc (case-insensitive, ignoring path). No Joliet/Rock Ridge support
	// needed -- PC game install discs from this era use plain ISO 9660 8.3 names.
	struct Iso9660Reader
	{
		const std::vector<uint8>& mData;
		static const constexpr uint32 SECTOR_SIZE = 2048;

		explicit Iso9660Reader(const std::vector<uint8>& data) : mData(data) {}

		uint32 readLE32(size_t offset) const
		{
			if (offset + 4 > mData.size())
				return 0;
			return (uint32)mData[offset] | ((uint32)mData[offset+1] << 8) | ((uint32)mData[offset+2] << 16) | ((uint32)mData[offset+3] << 24);
		}

		bool findFile(const std::string& targetNameLower, uint32 dirExtentLBA, uint32 dirExtentSize, std::vector<uint8>& outContent, int depth) const
		{
			if (depth > 8)
				return false;

			const size_t dirOffset = (size_t)dirExtentLBA * SECTOR_SIZE;
			if (dirOffset + dirExtentSize > mData.size())
				return false;

			size_t pos = 0;
			while (pos < dirExtentSize)
			{
				const size_t recordOffset = dirOffset + pos;
				if (recordOffset >= mData.size())
					break;
				const uint8 recordLength = mData[recordOffset];
				if (recordLength == 0)
				{
					// Padding to next sector boundary
					pos = ((pos / SECTOR_SIZE) + 1) * SECTOR_SIZE;
					continue;
				}

				const uint8 fileFlags = mData[recordOffset + 25];
				const uint32 extentLBA = readLE32(recordOffset + 2);
				const uint32 extentSize = readLE32(recordOffset + 10);
				const uint8 nameLength = mData[recordOffset + 32];
				const bool isDirectory = (fileFlags & 0x02) != 0;

				if (nameLength > 0 && recordOffset + 33 + nameLength <= mData.size())
				{
					std::string name((const char*)&mData[recordOffset + 33], nameLength);
					// Strip ";1" version suffix and lowercase for comparison
					const size_t semicolon = name.find(';');
					if (semicolon != std::string::npos)
						name = name.substr(0, semicolon);
					std::string nameLower = name;
					std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });

					if (isDirectory)
					{
						if (name != "\0" && name != "\1" && nameLength > 1)		// Skip "." and ".." entries (single 0x00 / 0x01 byte names)
						{
							if (findFile(targetNameLower, extentLBA, extentSize, outContent, depth + 1))
								return true;
						}
					}
					else if (nameLower == targetNameLower)
					{
						const size_t fileOffset = (size_t)extentLBA * SECTOR_SIZE;
						if (fileOffset + extentSize <= mData.size())
						{
							outContent.resize(extentSize);
							memcpy(&outContent[0], &mData[fileOffset], extentSize);
							return true;
						}
					}
				}

				pos += recordLength;
			}
			return false;
		}

		bool findFileByName(const std::string& targetNameLower, std::vector<uint8>& outContent) const
		{
			// Primary Volume Descriptor is always at sector 16
			const size_t pvdOffset = (size_t)16 * SECTOR_SIZE;
			if (pvdOffset + SECTOR_SIZE > mData.size() || mData[pvdOffset] != 1)		// Type 1 = Primary Volume Descriptor
				return false;

			// Root directory record is embedded at offset 156 within the PVD, 34 bytes long
			const size_t rootRecordOffset = pvdOffset + 156;
			const uint32 rootExtentLBA = readLE32(rootRecordOffset + 2);
			const uint32 rootExtentSize = readLE32(rootRecordOffset + 10);
			return findFile(targetNameLower, rootExtentLBA, rootExtentSize, outContent, 0);
		}
	};
}


bool ResourcesCache::loadRom()
{
	mRom.clear();

	// Load ROM content
	Configuration& config = Configuration::instance();
	const GameProfile& gameProfile = GameProfile::instance();
	std::wstring romPath;
	bool loaded = false;
	bool saveRom = false;

	// First have a look at the game's app data, where the ROM gets copied to after it was found once
	if (!loaded && !config.mAppDataPath.empty())
	{
		for (const GameProfile::RomInfo& romInfo : gameProfile.mRomInfos)
		{
			romPath = config.mGameAppDataPath + romInfo.mSteamRomName;
			loaded = loadRomFile(romPath, romInfo);
			if (loaded)
				break;
		}

		// If ROM is not found yet, but in one of the next steps, then make sure to copy it into the app data folder afterwards
		saveRom = !loaded;
	}

#if !defined(PLATFORM_ANDROID)
	// Try at last known ROM location, if there is one
	//  -> Do this only for the S3AIR executable, it won't work when switching between projects in OxygenApp
	if (!loaded && !config.mLastRomPath.empty() && gameProfile.mIdentifier == "Sonic3AIR")
	{
		romPath = config.mLastRomPath;
		loaded = loadRomFile(romPath);
	}

	// Then check at the configuration ROM path
	if (!loaded && !config.mRomPath.empty())
	{
		romPath = config.mRomPath;
		loaded = loadRomFile(romPath);
	}
#endif

	// Or is it the Steam ROM right inside the installation directory?
	if (!loaded)
	{
		for (const GameProfile::RomInfo& romInfo : gameProfile.mRomInfos)
		{
			romPath = romInfo.mSteamRomName;
			loaded = loadRomFile(romPath, romInfo);
			if (loaded)
				break;
		}
	}

#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_LINUX)
	// If still not loaded, search for Steam installation of the game
	if (!loaded && !gameProfile.mRomInfos.empty())
	{
		RMX_LOG_INFO("Trying to find Steam ROM");
		for (const GameProfile::RomInfo& romInfo : gameProfile.mRomInfos)
		{
			romPath = PlatformFunctions::tryGetSteamRomPath(romInfo.mSteamRomName);
			if (!romPath.empty())
			{
				loaded = loadRomFile(romPath, romInfo);
				if (loaded)
					break;
			}
		}
	}
#endif

	// If ROM was still not loaded, it's time to give up now...
	if (!loaded)
	{
		return false;
	}

	if (saveRom)
	{
		// Note that this updates the config
		saveRomToAppData();
	}
	else
	{
		// Update config if there was a change
		if (romPath != config.mLastRomPath)
		{
			config.mLastRomPath = romPath;
			config.saveSettings();
		}
	}

	// Done
	return true;
}

bool ResourcesCache::loadRomFromFile(const std::wstring& filename)
{
	if (!loadRomFile(filename))
		return false;

	saveRomToAppData();
	return true;
}

bool ResourcesCache::loadRomFromMemory(const std::vector<uint8>& content)
{
	if (!loadRomMemory(content))
		return false;

	saveRomToAppData();
	return true;
}

void ResourcesCache::loadAllResources()
{
	PaletteCollection::instance().loadPalettes();
	RawDataCollection::instance().loadRawData();
}

bool ResourcesCache::tryUnwrapContainer(const std::wstring& filename, std::vector<uint8>& content)
{
	// If the user selected a ZIP or ISO (e.g. a PC Collection install disc image or its
	// download archive) instead of the raw executable, locate and extract the executable
	// from inside it, so it can be used exactly like a directly-selected .exe from here on.
	const size_t dotPos = filename.find_last_of(L'.');
	if (dotPos == std::wstring::npos)
		return false;
	std::wstring extension = filename.substr(dotPos + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return (wchar_t)std::tolower((int)c); });

	if (extension != L"zip" && extension != L"iso")
		return false;

	std::vector<std::wstring> targetNames;
	for (const GameProfile::RomInfo& romInfo : GameProfile::instance().mRomInfos)
	{
		if (romInfo.mRomType == GameProfile::RomType::PC && !romInfo.mSteamRomName.empty())
			targetNames.push_back(romInfo.mSteamRomName);
	}
	if (targetNames.empty())
		return false;

	if (extension == L"zip")
	{
		ZipFileProvider zipProvider(filename);
		if (!zipProvider.isLoaded())
			return false;

		for (const std::wstring& targetName : targetNames)
		{
			// Search recursively for the target filename anywhere in the archive
			std::vector<rmx::FileIO::FileEntry> entries;
			if (!zipProvider.listFilesByMask(L"*" + targetName, true, entries))
				continue;

			for (const rmx::FileIO::FileEntry& entry : entries)
			{
				std::vector<uint8> extracted;
				if (zipProvider.readFile(entry.mPath + entry.mFilename, extracted))
				{
					content = std::move(extracted);
					RMX_LOG_INFO("Extracted PC executable from ZIP archive");
					return true;
				}
			}
		}
		return false;
	}
	else	// extension == "iso"
	{
		std::vector<uint8> isoContent;
		if (!FTX::FileSystem->readFile(filename, isoContent))
			return false;

		Iso9660Reader reader(isoContent);
		for (const std::wstring& targetName : targetNames)
		{
			std::string targetLower(targetName.begin(), targetName.end());		// ASCII-only filenames expected
			std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });

			std::vector<uint8> extracted;
			if (reader.findFileByName(targetLower, extracted))
			{
				content = std::move(extracted);
				RMX_LOG_INFO("Extracted PC executable from ISO image");
				return true;
			}
		}
		return false;
	}
}

bool ResourcesCache::loadRomFile(const std::wstring& filename)
{
	const GameProfile::RomCheck& romCheck = GameProfile::instance().mRomCheck;
	std::vector<uint8> content;
	content.reserve(romCheck.mSize > 0 ? romCheck.mSize : 0x400000);
	if (!tryUnwrapContainer(filename, content))
	{
		if (!FTX::FileSystem->readFile(filename, content))
			return false;
	}

	return loadRomMemory(content);
}

bool ResourcesCache::loadRomFile(const std::wstring& filename, const GameProfile::RomInfo& romInfo)
{
	const GameProfile::RomCheck& romCheck = GameProfile::instance().mRomCheck;
	mRom.reserve(romCheck.mSize > 0 ? romCheck.mSize : 0x400000);

	std::vector<uint8> fileContent;
	if (!tryUnwrapContainer(filename, fileContent))
	{
		if (!FTX::FileSystem->readFile(filename, fileContent))
			return false;
	}

	// Handle PC executable format
	if (romInfo.mRomType == GameProfile::RomType::PC)
	{
		// Extract game data from PC executable
		if (!extractPCGameData(fileContent, romInfo))
			return false;
	}
	else
	{
		// For Genesis ROMs, use the file content directly
		mRom = fileContent;
	}

	// If ROM info defines a required header checksum, make sure it fits (this is meant to be an early-out before doing the potentially expensive code below)
	const uint64 headerChecksum = getHeaderChecksum(mRom);
	if (romInfo.mHeaderChecksum != 0 && romInfo.mHeaderChecksum != headerChecksum)
		return false;

	if (applyRomModifications(romInfo))
	{
		if (checkRomContent(&romInfo))
		{
			mLoadedRomInfo = &romInfo;
			return true;
		}
	}
	return false;
}

bool ResourcesCache::loadRomMemory(const std::vector<uint8>& content)
{
	if (GameProfile::instance().mRomInfos.empty())
	{
		mRom = content;
		if (checkRomContent())
			return true;
	}
	else
	{
		for (const GameProfile::RomInfo& romInfo : GameProfile::instance().mRomInfos)
		{
			// PC-type content needs its game data extracted from the executable first --
			// its header checksum (if any) applies to the extracted data, not the raw file,
			// so it can only be checked afterwards.
			if (romInfo.mRomType == GameProfile::RomType::PC)
			{
				if (!extractPCGameData(content, romInfo))
					continue;
			}
			else
			{
				// If ROM info defines a required header checksum, make sure it fits (this is meant to be an early-out before doing the potentially expensive code below)
				const uint64 headerChecksum = getHeaderChecksum(content);
				if (romInfo.mHeaderChecksum != 0 && romInfo.mHeaderChecksum != headerChecksum)
					continue;

				mRom = content;
			}

			const uint64 extractedChecksum = getHeaderChecksum(mRom);
			if (romInfo.mHeaderChecksum != 0 && romInfo.mHeaderChecksum != extractedChecksum)
				continue;

			if (applyRomModifications(romInfo))
			{
				if (checkRomContent(&romInfo))
				{
					mLoadedRomInfo = &romInfo;
					return true;
				}
			}
		}
	}
	return false;
}

uint64 ResourcesCache::getHeaderChecksum(const std::vector<uint8>& content)
{
	if (content.empty())
		return 0;

	// Regard the first 512 byte as header
	return rmx::getMurmur2_64(&content[0], std::min<size_t>(512, content.size()));
}

bool ResourcesCache::applyRomModifications(const GameProfile::RomInfo& romInfo)
{
	if (!romInfo.mDiffFileName.empty())
	{
		// Load diff file if needed
		std::vector<uint8>* content = nullptr;
		const auto it = mDiffFileCache.find(&romInfo);
		if (it == mDiffFileCache.end())
		{
			content = &mDiffFileCache[&romInfo];
			FTX::FileSystem->readFile(romInfo.mDiffFileName, *content);
		}
		else
		{
			content = &it->second;
		}

		// Apply diff file by XORing it in
		if (content->size() != mRom.size())
			return false;

		uint64* ptr = (uint64*)&mRom[0];
		uint64* diff = (uint64*)&(*content)[0];
		const size_t count = content->size() / 8;
		for (size_t i = 0; i < count; ++i)
		{
			ptr[i] ^= diff[i];
		}
	}

	for (auto& pair : romInfo.mBlankRegions)
	{
		if (pair.first > pair.second || pair.second >= mRom.size())
			return false;
		memset(&mRom[pair.first], 0, pair.second - pair.first + 1);
	}

	for (auto& pair : romInfo.mOverwrites)
	{
		if (pair.first >= mRom.size())
			return false;
		mRom[pair.first] = pair.second;
	}

	return true;
}

bool ResourcesCache::checkRomContent(const GameProfile::RomInfo* romInfo)
{
	// Check that it's the right ROM
	const GameProfile::RomCheck& romCheck = GameProfile::instance().mRomCheck;
	if (romCheck.mSize > 0)
	{
		if (mRom.size() != romCheck.mSize)
			return false;
	}

	// The content checksum is computed against the Genesis ROM's exact bytes.
	// A PC Collection executable's data region is byte-identical to the Genesis
	// ROM only at the addresses skc_disasm actually documents -- unused code
	// regions differ, since the PC port never executes 68k code -- so the whole
	// content checksum can't apply there; its own (skippable) HeaderChecksum is
	// the verification hook for that RomType instead.
	const bool isPcRom = (nullptr != romInfo && romInfo->mRomType == GameProfile::RomType::PC);
	if (romCheck.mChecksum != 0 && !isPcRom)
	{
		const uint64 checksum = rmx::getMurmur2_64(&mRom[0], mRom.size());
		if (checksum != romCheck.mChecksum)
			return false;
	}

	// ROM check succeeded
	mDiffFileCache.clear();		// This cache is not needed again now
	return true;
}

void ResourcesCache::saveRomToAppData()
{
	if (nullptr != mLoadedRomInfo && !mLoadedRomInfo->mSteamRomName.empty())
	{
		const std::wstring filepath = Configuration::instance().mGameAppDataPath + mLoadedRomInfo->mSteamRomName;
		const bool success = FTX::FileSystem->saveFile(filepath, mRom);
		if (success)
		{
			Configuration::instance().mLastRomPath = filepath;
		}
		else
		{
			RMX_ERROR("Failed to store a copy of the ROM in the app data folder", );
		}
	}
}

bool ResourcesCache::extractPCGameData(const std::vector<uint8>& exeContent, const GameProfile::RomInfo& romInfo)
{
	// PC executable contains game data at a specific offset
	// The game data is stored in the same format as the Genesis ROM
	
	if (exeContent.size() < romInfo.mPCDataOffset + romInfo.mPCDataSize)
	{
		RMX_LOG_ERROR("PC executable is too small to contain game data");
		return false;
	}

	// Extract game data from executable
	mRom.resize(romInfo.mPCDataSize);
	memcpy(&mRom[0], &exeContent[romInfo.mPCDataOffset], romInfo.mPCDataSize);

	RMX_LOG_INFO("Extracted " << romInfo.mPCDataSize << " bytes of game data from PC executable at offset " << rmx::hexString(romInfo.mPCDataOffset));

	applyPCPatchRanges(romInfo);
	return true;
}

void ResourcesCache::applyPCPatchRanges(const GameProfile::RomInfo& romInfo)
{
	// Some small address ranges in a PC executable's data blob are occupied by native code
	// instead of the original Genesis-address-mapped data (boot-time pointer tables that were
	// interleaved with 68k code the PC port replaced). If the user configured a patch source
	// ROM and it's actually present, overlay those known-bad ranges from it. This never
	// bundles or requires such a file -- it's purely optional and user-supplied.
	if (romInfo.mPatchSourceRomName.empty() || romInfo.mPatchRanges.empty())
		return;

	std::vector<uint8> patchSource;
	if (!FTX::FileSystem->readFile(romInfo.mPatchSourceRomName, patchSource))
		return;

	for (const GameProfile::AddressRange& range : romInfo.mPatchRanges)
	{
		const uint32 start = range.first;
		const uint32 end = range.second;		// Inclusive
		if (end < start || end >= patchSource.size() || end >= mRom.size())
			continue;

		memcpy(&mRom[start], &patchSource[start], (size_t)(end - start + 1));
	}

	RMX_LOG_INFO("Applied " << romInfo.mPatchRanges.size() << " patch range(s) from '" << WString(romInfo.mPatchSourceRomName).toStdString() << "'");
}

