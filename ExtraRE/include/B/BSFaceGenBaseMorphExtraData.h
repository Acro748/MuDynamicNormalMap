#pragma once

namespace RE 
{
	class BSFaceGenBaseMorphExtraData : public NiExtraData
	{
	public:
		NiPoint3* vertexData;		// 18
		std::uint32_t	modelVertexCount;	// 20 - Same as 24 if the tri model is correct
		std::uint32_t	vertexCount;		// 24

		static BSFaceGenBaseMorphExtraData* Create(NiGeometryData* geometryData, bool copy = false) {
			BSFaceGenBaseMorphExtraData* data = (BSFaceGenBaseMorphExtraData*)NiExtraData::Create(sizeof(BSFaceGenBaseMorphExtraData), RE::VTABLE_BSFaceGenBaseMorphExtraData.data()->address());
			data->name = BSFixedString("FOD");
			data->_refCount = 0;
			data->modelVertexCount = 0;
			data->vertexCount = 0;
			data->vertexData = NULL;

			if (geometryData) {
				data->vertexCount = geometryData->vertices;
				data->modelVertexCount = geometryData->vertices;

				data->vertexData = (NiPoint3*)MemoryManager::GetSingleton()->Allocate(sizeof(NiPoint3) * data->vertexCount, 0, false);
				if (copy)
					memcpy(data->vertexData, geometryData->vertex, sizeof(NiPoint3) * data->vertexCount);
				else
					memset(data->vertexData, 0, sizeof(NiPoint3) * data->vertexCount);
			}
			return data;
		};
	};
}