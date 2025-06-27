#pragma once

namespace MDNM {
	class Interface
	{
	public:
		Interface() {};
		virtual ~Interface() {};

		virtual std::uint32_t GetVersion() = 0; //current version is 6
	};

	struct InterfaceExchangeMessage
	{
		enum
		{
			kMessage_ExchangeInterface = 0x4D444E4D
		};

		Interface* Interface = nullptr;
	};

	class IDynamicNormalMap :
		public Interface
	{
	public:
		virtual void QBakeObjectNormalmap(RE::Actor* a_actor, std::uint32_t bipedSlot) = 0;
		virtual void QBakeObjectNormalmap(RE::Actor* a_actor, RE::BSGeometry** a_geometries, std::uint32_t a_geometryCount, std::uint32_t bipedSlot) = 0;
	};

	class InterfaceManager {
	public:
		InterfaceManager() {};
		~InterfaceManager() {};

		inline bool load() {
			if (!load_Check)
			{
				if (IsLoaded())
				{
					const auto messagingInterface = SKSE::GetMessagingInterface();
					if (messagingInterface)
						messagingInterface->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface, &exchangeMessage, 0, "MuDynamicNormalMap");
				}
				load_Check = true;
			}
			return exchangeMessage.Interface ? true : false;
		}

		IDynamicNormalMap* GetInterface() {
			if (load())
			{
				return reinterpret_cast<IDynamicNormalMap*>(exchangeMessage.Interface);
			}
			return nullptr;
		}

	protected:

	private:
		inline bool IsLoaded() { return GetModuleHandle(L"MuDynamicNormalMap.dll"); }

		bool load_Check = false;
		InterfaceExchangeMessage exchangeMessage;
	};
	static InterfaceManager MDNMAPI;
}
