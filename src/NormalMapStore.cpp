#include "NormalMapStore.h"
#include "lz4.h"

namespace Mus {
	void NormalMapStore::onEvent(const FrameEvent& e)
	{
		if (lastTickTime + 100 > currentTime) //0.1sec
			return;
		lastTickTime = currentTime;

		memoryManageThreads->submitAsync([&] {
			ClearMemory();
		});
	}
	void NormalMapStore::onEvent(const QuitGameEvent& e)
	{
		ClearDiskCache();
	}

	void NormalMapStore::Init()
	{
		baseFolder = "";
		GetCacheFileFolder();
	}
	void NormalMapStore::ClearMemory()
	{
		lock.lock_shared();
		auto map_ = map;
		lock.unlock_shared();

		std::uint32_t garbageCount = 0;
		for (auto& m : map_) {
			if (!m.second->normalmapTexture2D || !m.second->normalmapShaderResourceView)
            {
                lock.lock();
                map.erase(m.first);
                lock.unlock();
                garbageCount++;
				continue;
			}
			if (GetRefCount(m.second->normalmapTexture2D.Get()) <= 1 || GetRefCount(m.second->normalmapShaderResourceView.Get()) <= 1)
            {
                lock.lock();
                map.erase(m.first);
                lock.unlock();
                garbageCount++;
				continue;
			}
		}

		if (garbageCount == 0)
			return;

        lock.lock_shared();
        const std::size_t mapSize = map.size();
        lock.unlock_shared();

		logger::debug("Removed {} RAM cache / Current remain {} RAM cache", garbageCount, mapSize);
	}

	void NormalMapStore::AddResource(std::uint64_t a_hash, TextureResourcePtr a_resource)
	{
		lock.lock_shared();
		map[a_hash] = a_resource;
		lock.unlock_shared();

		if (Config::GetSingleton().GetDiskCache())
		{
			CreateDiskCache(a_hash, a_resource);
			CheckDiskCacheCapacity();
		}
	}
	bool NormalMapStore::GetResource(std::uint64_t a_hash, TextureResourcePtr& a_resource, bool& isDiskCache)
	{
		TextureResourcePtr resource = nullptr;
		isDiskCache = false;
		lock.lock_shared();
		auto found = map.find(a_hash);
		if (found != map.end())
			resource = found->second;
		lock.unlock_shared();
		if (resource)
			logger::debug("Found exist resource {:x}", a_hash);
		else 
		{
			resource = GetDiskCache(a_hash);
			if (resource) {
				isDiskCache = true;
				lock.lock_shared();
				map[a_hash] = resource;
				lock.unlock_shared();
				logger::debug("Found disk cache resource {:x}", a_hash);
			}
		}
		if (resource)
			UpdateAccessTime(a_hash);
		a_resource = resource;
		return a_resource ? true : false;
	}

	void NormalMapStore::AddHashPair(std::uint64_t a_hash, std::uint64_t b_hash)
	{
		hashPairsLock.lock();
		auto found = std::find_if(hashPairs.begin(), hashPairs.end(), [&](HashPair& pair) {
			return pair.find(a_hash) != pair.end() || pair.find(b_hash) != pair.end();
		});
		if (found != hashPairs.end() && (found->find(a_hash) == found->end() || found->find(b_hash) == found->end()))
		{
			found->insert(a_hash);
			found->insert(b_hash);
			logger::debug("Add hash pair {:x} - {:x}", a_hash, b_hash);
		}
		else
		{
			HashPair newPair;
			newPair.insert(a_hash);
			newPair.insert(b_hash);
			hashPairs.push_back(newPair);
			logger::debug("Create and add hash pair {:x} - {:x}", a_hash, b_hash);
		}
		hashPairsLock.unlock();
	}
	bool NormalMapStore::IsPairHashes(std::uint64_t a_hash, std::uint64_t b_hash)
	{
		hashPairsLock.lock_shared();
		bool isPair = std::find_if(hashPairs.cbegin(), hashPairs.cend(), [&](const HashPair& pair) {
			return pair.find(a_hash) != pair.cend() && pair.find(b_hash) != pair.cend();
		}) != hashPairs.cend();
		hashPairsLock.unlock_shared();
		logger::debug("Is pair hashes : {:x} - {:x} => {}", a_hash, b_hash, isPair ? "O" : "X");
		return isPair;
	}
	void NormalMapStore::InitHashPair(std::uint64_t a_hash)
	{
		hashPairsLock.lock();
		auto found = std::find_if(hashPairs.begin(), hashPairs.end(), [&](HashPair& pair) {
			return pair.find(a_hash) != pair.end();
		});
		if (found != hashPairs.end()) {
			found->clear();
			found->insert(a_hash);
		}
		hashPairsLock.unlock();
	}

	void NormalMapStore::CreateDiskCache(std::uint64_t a_hash, TextureResourcePtr a_resource)
	{
		if (!a_resource->normalmapTexture2D)
			return;

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();

		DiskCacheInfo info;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> cacheTexture;
		a_resource->normalmapTexture2D->GetDesc(&info.texDesc);
		a_resource->normalmapShaderResourceView->GetDesc(&info.srvDesc);
		D3D11_TEXTURE2D_DESC desc = info.texDesc;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		info.mipLevels = info.texDesc.MipLevels;
		
		auto hr = device->CreateTexture2D(&desc, nullptr, &cacheTexture);
		if (FAILED(hr))
		{
			logger::error("Failed to create disk cache {:x}", a_hash);
			return;
		}

        {
			Shader::ShaderLocker sl(context);
            Shader::ShaderLockGuard slg(sl);
            context->CopyResource(cacheTexture.Get(), a_resource->normalmapTexture2D.Get());
        }

		RemoveDiskCache(a_hash);
		for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++) {
			const UINT height = std::max(desc.Height >> mipLevel, 1u);
			std::size_t blockHeight = 0;

			Shader::MapGuard mg(context, cacheTexture.Get(), mipLevel, D3D11_MAP_READ);
			if (!mg.IsValid())
			{
				logger::error("Failed to map copy texture : {}", mg.GetHR());
				return;
			}

			const std::uint32_t rowPitch = mg.GetRowPitch();

			if (desc.Format == DXGI_FORMAT_BC7_UNORM || desc.Format == DXGI_FORMAT_BC3_UNORM || desc.Format == DXGI_FORMAT_BC1_UNORM)
			{
				blockHeight = std::size_t(height + 3) / 4;
			}
			else if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
			{
				blockHeight = height;
			}
			else
			{
				blockHeight = height;
			}

			std::vector<std::uint8_t> buffer((std::size_t)rowPitch * blockHeight);
			for (std::size_t y = 0; y < blockHeight; y++) {
				memcpy(buffer.data() + y * rowPitch, mg.Get<std::uint8_t>() + y * rowPitch, rowPitch);
			}

			int maxSize = LZ4_compressBound(buffer.size());
			std::vector<char> compressed(maxSize);

			std::uint64_t compressedSize = LZ4_compress_default((const char*)buffer.data(), compressed.data(), buffer.size(), maxSize);

			std::filesystem::path filePath = GetCacheFileName(a_hash, mipLevel);
			std::filesystem::create_directory(filePath.parent_path());

			std::ofstream ofs(filePath, std::ios::binary);
			if (!ofs) {
				logger::error("Unable to write {} file {:x}", filePath.string(), a_hash);
				return;
			}
			ofs.exceptions(std::ios::failbit | std::ios::badbit);
			try {
				std::uint64_t origSize = buffer.size();
				ofs.write(reinterpret_cast<const char*>(&origSize), 8);
				ofs.write(reinterpret_cast<const char*>(&compressedSize), 8);
				ofs.write(compressed.data(), compressedSize);
				ofs.close();
				info.rowPitch.push_back(rowPitch);
				AddDiskCacheSize(GetFileSize(filePath));
			}
			catch (...) {
				logger::error("Unable to write {} file {:x}", filePath.string(), a_hash);
				return;
			}
		}
		info.lastAccessTime = currentTime;

		diskCacheLock.lock();
		diskCacheInfo[a_hash] = info;
		diskCacheLock.unlock();
		logger::info("Created disk cache {:x}", a_hash);
		if (Config::GetSingleton().GetClearDiskCache())
			return;

		std::filesystem::path infoPath = GetCacheFileName(a_hash, -1);
		std::ofstream ofs(infoPath, std::ios::binary);
		if (!ofs) {
			logger::error("Unable to write {} file {:x}", infoPath.string(), a_hash);
			return;
		}
		ofs.exceptions(std::ios::failbit | std::ios::badbit);
		try {
			ofs.write(reinterpret_cast<const char*>(&info.mipLevels), sizeof(info.mipLevels));
			std::size_t rowPitchSize = info.rowPitch.size();
			ofs.write(reinterpret_cast<const char*>(&rowPitchSize), sizeof(rowPitchSize));
			for (auto& rowPitch : info.rowPitch) {
				ofs.write(reinterpret_cast<const char*>(&rowPitch), sizeof(rowPitch));
			}
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.Width), sizeof(info.texDesc.Width));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.Height), sizeof(info.texDesc.Height));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.MipLevels), sizeof(info.texDesc.MipLevels));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.ArraySize), sizeof(info.texDesc.ArraySize));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.Format), sizeof(info.texDesc.Format));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.SampleDesc.Count), sizeof(info.texDesc.SampleDesc.Count));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.SampleDesc.Quality), sizeof(info.texDesc.SampleDesc.Quality));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.Usage), sizeof(info.texDesc.Usage));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.BindFlags), sizeof(info.texDesc.BindFlags));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.CPUAccessFlags), sizeof(info.texDesc.CPUAccessFlags));
			ofs.write(reinterpret_cast<const char*>(&info.texDesc.MiscFlags), sizeof(info.texDesc.MiscFlags));

			ofs.write(reinterpret_cast<const char*>(&info.srvDesc.Format), sizeof(info.srvDesc.Format));
			ofs.write(reinterpret_cast<const char*>(&info.srvDesc.ViewDimension), sizeof(info.srvDesc.ViewDimension));
			ofs.write(reinterpret_cast<const char*>(&info.srvDesc.Texture2D.MipLevels), sizeof(info.srvDesc.Texture2D.MipLevels));
			ofs.write(reinterpret_cast<const char*>(&info.srvDesc.Texture2D.MostDetailedMip), sizeof(info.srvDesc.Texture2D.MostDetailedMip));
			ofs.close();
			AddDiskCacheSize(GetFileSize(infoPath));
		}
		catch (...) {
			logger::error("Unable to write {} file {:x}", infoPath.string(), a_hash);
			return;
		}
		logger::info("Created disk cache info {:x}", a_hash);
	}
	TextureResourcePtr NormalMapStore::GetDiskCache(std::uint64_t a_hash)
	{
		if (!Config::GetSingleton().GetDiskCache())
			return nullptr;

		bool isFound = false;
		DiskCacheInfo info;
		diskCacheLock.lock_shared();
		{
			auto found = diskCacheInfo.find(a_hash);
			if (found != diskCacheInfo.end())
			{
				isFound = true;
				info = found->second;
			}
		}
		diskCacheLock.unlock_shared();

		if (!isFound)
		{
			if (Config::GetSingleton().GetClearDiskCache())
				return nullptr;
			std::filesystem::path infoPath = GetCacheFileName(a_hash, -1);
			if (!std::filesystem::exists(infoPath))
			{
				logger::error("Disk cache does not exists {:x}", a_hash);
				return nullptr;
			}
			std::ifstream ifs(infoPath, std::ios::binary);
			if (!ifs)
			{
				logger::error("Unable to read disk cache {:x}", a_hash);
				return nullptr;
			}
			ifs.exceptions(std::ios::failbit | std::ios::badbit);
			try {
				ifs.read(reinterpret_cast<char*>(&info.mipLevels), sizeof(info.mipLevels));
				std::size_t rowPitchSize = 0;
				ifs.read(reinterpret_cast<char*>(&rowPitchSize), sizeof(rowPitchSize));
				for (std::size_t i = 0; i < rowPitchSize; i++) {
					std::uint32_t rowPitch = 0;
					ifs.read(reinterpret_cast<char*>(&rowPitch), sizeof(rowPitch));
					info.rowPitch.push_back(rowPitch);
				}
				ifs.read(reinterpret_cast<char*>(&info.texDesc.Width), sizeof(info.texDesc.Width));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.Height), sizeof(info.texDesc.Height));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.MipLevels), sizeof(info.texDesc.MipLevels));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.ArraySize), sizeof(info.texDesc.ArraySize));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.Format), sizeof(info.texDesc.Format));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.SampleDesc.Count), sizeof(info.texDesc.SampleDesc.Count));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.SampleDesc.Quality), sizeof(info.texDesc.SampleDesc.Quality));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.Usage), sizeof(info.texDesc.Usage));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.BindFlags), sizeof(info.texDesc.BindFlags));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.CPUAccessFlags), sizeof(info.texDesc.CPUAccessFlags));
				ifs.read(reinterpret_cast<char*>(&info.texDesc.MiscFlags), sizeof(info.texDesc.MiscFlags));

				ifs.read(reinterpret_cast<char*>(&info.srvDesc.Format), sizeof(info.srvDesc.Format));
				ifs.read(reinterpret_cast<char*>(&info.srvDesc.ViewDimension), sizeof(info.srvDesc.ViewDimension));
				ifs.read(reinterpret_cast<char*>(&info.srvDesc.Texture2D.MipLevels), sizeof(info.srvDesc.Texture2D.MipLevels));
				ifs.read(reinterpret_cast<char*>(&info.srvDesc.Texture2D.MostDetailedMip), sizeof(info.srvDesc.Texture2D.MostDetailedMip));
			}
			catch (...) {
				logger::error("Unable to read disk cache {:x}", a_hash);
				return nullptr;
			}
			diskCacheLock.lock();
			diskCacheInfo[a_hash] = info;
			diskCacheLock.unlock();
			logger::info("Found disk cache {:x}", a_hash);
		}

		std::vector<D3D11_SUBRESOURCE_DATA> initDatas(info.mipLevels);
		std::vector<std::vector<std::uint8_t>> buffers(info.mipLevels);

		for (UINT mipLevel = 0; mipLevel < info.mipLevels; mipLevel++) {
			std::filesystem::path filePath = GetCacheFileName(a_hash, mipLevel);
			std::ifstream ifs(filePath, std::ios::binary);
			if (!std::filesystem::exists(filePath))
			{
				logger::error("{} file does not exists {:x}", filePath.string(), a_hash);
				return nullptr;
			}
			if (!ifs)
			{
				logger::error("Unable to read {} file {:x}", filePath.string(), a_hash);
				return nullptr;
			}
			ifs.exceptions(std::ios::failbit | std::ios::badbit);

			std::uint64_t origSize = 0;
			std::uint64_t compressedSize = 0;
			std::vector<char> compressed;
			try {

				ifs.read(reinterpret_cast<char*>(&origSize), sizeof(origSize));
				ifs.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));
				compressed.resize(compressedSize);
				ifs.read(compressed.data(), compressedSize);
			}
			catch (...) {
				logger::error("Unable to read {} file {:x}", filePath.string(), a_hash);
				return nullptr;
			}

			buffers[mipLevel].resize(origSize);
			int result = LZ4_decompress_safe(compressed.data(),
				reinterpret_cast<char*>(buffers[mipLevel].data()),
				static_cast<int>(compressedSize),
				static_cast<int>(origSize)
			);

			if (result < 0) {
				logger::error("Failed to decompress {} file {:x}", filePath.string(), a_hash);
				return nullptr;
			}

			initDatas[mipLevel].pSysMem = buffers[mipLevel].data();
			initDatas[mipLevel].SysMemPitch = info.rowPitch[mipLevel];
			initDatas[mipLevel].SysMemSlicePitch = 0;
		}

		TextureResourcePtr result = std::make_shared<TextureResource>();
		auto hr = Shader::ShaderManager::GetSingleton().GetDevice()->CreateTexture2D(&info.texDesc, initDatas.data(), &result->normalmapTexture2D);
		if (FAILED(hr)) {
			logger::error("Failed to create dst texture {:x} : {}", a_hash, hr);
			return nullptr;
		}
		hr = Shader::ShaderManager::GetSingleton().GetDevice()->CreateShaderResourceView(result->normalmapTexture2D.Get(), &info.srvDesc, &result->normalmapShaderResourceView);
		if (FAILED(hr)) {
			logger::error("Failed to create dst shader resource view {:x} : {}", a_hash, hr);
			return nullptr;
		}
		return result;
	}
	void NormalMapStore::RemoveDiskCache(std::uint64_t a_hash)
	{
		DiskCacheInfo info;
		bool isFound = false;
		diskCacheLock.lock_shared();
		auto found = diskCacheInfo.find(a_hash);
		if (found != diskCacheInfo.end())
		{
			info = found->second;
			isFound = true;
		}
		diskCacheLock.unlock_shared();
		if (!isFound)
			return;

		std::error_code ec;
		if (!Config::GetSingleton().GetClearDiskCache())
		{
			std::filesystem::path filePath = GetCacheFileName(a_hash, -1);
			RemoveDiskCacheSize(GetFileSize(filePath));
			std::filesystem::remove(filePath, ec);
		}
		for (UINT mipLevel = 0; mipLevel < info.mipLevels; mipLevel++) {
			std::filesystem::path filePath = GetCacheFileName(a_hash, mipLevel);
			RemoveDiskCacheSize(GetFileSize(filePath));
			std::filesystem::remove(filePath, ec);
		}

		diskCacheLock.lock();
		diskCacheInfo.erase(a_hash);
		diskCacheLock.unlock();
		logger::info("Removed old disk cache {:x}", a_hash);
	}
	void NormalMapStore::ClearDiskCache()
	{
		if (!Config::GetSingleton().GetClearDiskCache())
			return;
		diskCacheLock.lock();
		diskCacheInfo.clear();
		diskCacheLock.unlock();

		std::filesystem::path folder = GetCacheFileFolder();
		if (!std::filesystem::exists(folder))
			return;
		if (!std::filesystem::is_directory(folder))
			return;

		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			std::filesystem::path file = entry.path();
			std::u8string filename_utf8 = file.filename().u8string();
			std::string filename(filename_utf8.begin(), filename_utf8.end());
			if (filename == "." || filename == "..")
				return;
			if (!stringEndsWith(filename, diskCacheExtension))
				return;
			std::filesystem::remove(file, ec);
			logger::debug("disk cache file {} removed", filename);
		}
		SetDiskCacheZeroSize();
	}

	std::string NormalMapStore::GetCacheFileFolder()
	{
		if (baseFolder.empty())
			baseFolder = Config::GetSingleton().GetDiskCacheFolder() + "\\";
		return baseFolder;
	}
	std::string NormalMapStore::GetCacheFileName(std::uint64_t a_hash, std::int32_t a_mipLevel)
	{
		if (a_mipLevel < 0)
			return GetCacheFileFolder() + GetHexStr(a_hash) + diskCacheExtension;
		return GetCacheFileFolder() + GetHexStr(a_hash) + "_" + std::to_string(a_mipLevel) + diskCacheExtension;
	}

	std::uint32_t NormalMapStore::GetRefCount(ID3D11Texture2D* texture)
	{
		if (!texture)
			return 0;

		texture->AddRef();
		return texture->Release();
	}
	std::uint32_t NormalMapStore::GetRefCount(ID3D11ShaderResourceView* texture)
	{
		if (!texture)
			return 0;

		texture->AddRef();
		return texture->Release();
	}

	bool NormalMapStore::CheckDiskCacheCapacity()
	{
		bool isRemoved = false;
		while (GetSizeAsMB(GetTotalDiskCacheSize()) > Config::GetSingleton().GetDiskCacheLimitMB()) {
			RemoveOldDiskCache();
			isRemoved = true;
		}
		return isRemoved;
	}
	void NormalMapStore::UpdateAccessTime(std::uint64_t a_hash)
	{
		if (Config::GetSingleton().GetDiskCache())
			return;
		diskCacheLock.lock_shared();
		auto found = diskCacheInfo.find(a_hash);
		if (found != diskCacheInfo.end())
		{
			found->second.lastAccessTime = currentTime;
		}
		diskCacheLock.unlock_shared();
	}
	void NormalMapStore::RemoveOldDiskCache()
	{
		diskCacheLock.lock_shared();
		auto diskCacheInfo_ = diskCacheInfo;
		diskCacheLock.unlock_shared();

		if (diskCacheInfo_.empty())
			return;

		auto minIt = std::min_element(diskCacheInfo_.begin(), diskCacheInfo_.end(), 
						 [] (std::pair<std::uint64_t, DiskCacheInfo> a, std::pair<std::uint64_t, DiskCacheInfo> b) {
			return a.second.lastAccessTime < b.second.lastAccessTime;
		});
		RemoveDiskCache(minIt->first);
	}
}
