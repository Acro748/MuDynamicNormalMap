#pragma once

namespace Mus{
	class NormalMapStore :
		public IEventListener<FrameEvent>, 
		public IEventListener<QuitGameEvent> {
	public:
		NormalMapStore() {};
		~NormalMapStore() { ClearDiskCache(); };

		[[nodiscard]] static NormalMapStore& GetSingleton() {
			static NormalMapStore instance;
			return instance;
		}

		void Init();

		void AddResource(std::uint64_t a_hash, TextureResourcePtr a_resource);
		TextureResourcePtr GetResource(std::uint64_t a_hash);
		void ClearMemory();

		void CreateDiskCache(std::uint64_t a_hash, TextureResourcePtr a_resource);
		TextureResourcePtr GetDiskCache(std::uint64_t a_hash);
		void RemoveDiskCache(std::uint64_t a_hash);
		void ClearDiskCache();

		bool CheckDiskCacheCapacity(); //false = remain, true = removed

	protected:
		void onEvent(const FrameEvent& e) override;
		void onEvent(const QuitGameEvent& e) override;

	private:
		const std::string diskCacheExtension = ".mdncache";
		std::clock_t lastTickTime = 0;

		std::string baseFolder = "";
		std::string GetCacheFileFolder();
		std::string GetCacheFileName(std::uint64_t a_hash, std::int32_t a_mipLevel);

		std::uint32_t GetRefCount(ID3D11Texture2D* texture);
		std::uint32_t GetRefCount(ID3D11ShaderResourceView* texture);

		std::shared_mutex lock;
		concurrency::concurrent_unordered_map<std::uint64_t, TextureResourcePtr> map;

		std::shared_mutex diskCacheLock;
		struct DiskCacheInfo {
			std::clock_t lastAccessTime = 0;
			std::uint32_t mipLevels;
			std::vector<std::uint32_t> rowPitch;
			D3D11_TEXTURE2D_DESC texDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		};
		concurrency::concurrent_unordered_map<std::uint64_t, DiskCacheInfo> diskCacheInfo;
		void UpdateAccessTime(std::uint64_t a_hash);

		inline std::uint64_t GetSizeAsMB(std::uint64_t fileSize) {
			return fileSize >> 20;
		}

		std::mutex currentDiskCacheSizeLock;
		std::uint64_t currentDiskCacheSize = 0;
		inline void AddDiskCacheSize(std::uint64_t fileMB) {
			std::lock_guard lg(currentDiskCacheSizeLock);
			currentDiskCacheSize += fileMB;
			logger::debug("Current Disk Cache Capacity : {}MB", GetSizeAsMB(currentDiskCacheSize));
		}
		inline void RemoveDiskCacheSize(std::uint64_t fileMB) {
			std::lock_guard lg(currentDiskCacheSizeLock);
			if (currentDiskCacheSize < fileMB)
				currentDiskCacheSize = 0;
			else
				currentDiskCacheSize -= fileMB;
			logger::debug("Current Disk Cache Capacity : {}MB", GetSizeAsMB(currentDiskCacheSize));
		}
		inline std::uint64_t GetTotalDiskCacheSize() {
			std::lock_guard lg(currentDiskCacheSizeLock);
			return currentDiskCacheSize;
		}
		inline void SetDiskCacheZeroSize() {
			std::lock_guard lg(currentDiskCacheSizeLock);
			currentDiskCacheSize = 0;
			logger::debug("Current Disk Cache Capacity : {}MB", GetSizeAsMB(currentDiskCacheSize));
		}
		void RemoveOldDiskCache();
	};
}