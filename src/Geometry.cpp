#include "Geometry.h"

namespace Mus {
	GeometryData::GeometryData(RE::BSGeometry* a_geo)
	{
		CopyGeometryData(a_geo);
	}

	RE::BSFaceGenBaseMorphExtraData* GeometryData::GetMorphExtraData(RE::BSGeometry* a_geometry)
	{
		return netimmerse_cast<RE::BSFaceGenBaseMorphExtraData*>(a_geometry->GetExtraData("FOD"));
	}
	std::uint32_t GeometryData::GetVertexCount(RE::BSGeometry* a_geometry)
	{
		if (!a_geometry || a_geometry->name.empty())
			return 0;
		const RE::BSTriShape* triShape = a_geometry->AsTriShape();
		if (!triShape)
			return 0;
		std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		const RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(a_geometry);
		if (!skinPartition)
			return vertexCount;
		vertexCount = vertexCount > 0 ? vertexCount : skinPartition->vertexCount;
		return vertexCount;
	}

	bool GeometryData::GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info)
	{
		if (!a_geo || a_geo->name.empty())
			return false;

		info.desc = a_geo->GetGeometryRuntimeData().vertexDesc;
		info.name = a_geo->name.c_str();
		info.hasVertices = info.desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
		info.hasUVs = info.desc.HasFlag(RE::BSGraphics::Vertex::VF_UV);
		info.hasNormals = info.desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL);
		info.hasTangents = info.desc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT);
		info.hasBitangents = info.hasVertices && info.hasNormals && info.hasTangents;
		return true;
	}
	bool GeometryData::CopyGeometryData(RE::BSGeometry* a_geo)
	{
		if (!a_geo || a_geo->name.empty())
			return false;

		ObjectInfo newObjInfo;
		if (!GetGeometryInfo(a_geo, newObjInfo.info))
			return false;

		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + newObjInfo.info.name, false, false);

		const RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape();
		const RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return false;

		newObjInfo.info.vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		const RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(a_geo);
		if (!skinPartition)
			return false;

		newObjInfo.info.vertexCount = newObjInfo.info.vertexCount > 0 ? newObjInfo.info.vertexCount : skinPartition->vertexCount;
		if (dynamicTriShape)
		{
			if (Config::GetSingleton().GetRealtimeDetectHead() < 2)
			{
				const auto morphData = GetMorphExtraData(a_geo);
				if (morphData)
				{
					newObjInfo.dynamicBlockData1.resize(newObjInfo.info.vertexCount);
					std::memcpy(newObjInfo.dynamicBlockData1.data(), morphData->vertexData, sizeof(RE::NiPoint3) * newObjInfo.info.vertexCount);
				}
			}
			else
			{
				if (dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData)
				{
					newObjInfo.dynamicBlockData2.resize(newObjInfo.info.vertexCount);
					std::memcpy(newObjInfo.dynamicBlockData2.data(), dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData, sizeof(DirectX::XMVECTOR) * newObjInfo.info.vertexCount);
				}
			}
		}
		newObjInfo.geometryBlockData.resize((std::size_t)newObjInfo.info.vertexCount * newObjInfo.info.desc.GetSize());
		std::memcpy(newObjInfo.geometryBlockData.data(), skinPartition->partitions[0].buffData->rawVertexData, sizeof(std::uint8_t) * newObjInfo.info.vertexCount * newObjInfo.info.desc.GetSize());

		for (auto& partition : skinPartition->partitions)
		{
			const std::size_t indicesCount = (std::size_t)partition.triangles * 3;
			std::vector<std::uint16_t> indicesBlockData_(indicesCount);
			std::memcpy(indicesBlockData_.data(), partition.triList, sizeof(std::uint16_t) * indicesCount);
			newObjInfo.indicesBlockData.insert(newObjInfo.indicesBlockData.end(), indicesBlockData_.begin(), indicesBlockData_.end());
		}
		geometries.push_back(GeometriesInfo{ a_geo, newObjInfo });

		if (geometries.size() > 1) {
			if (geometries[mainGeometryIndex].objInfo.indicesBlockData.size() < geometries[geometries.size() - 1].objInfo.indicesBlockData.size())
			{
				mainGeometryIndex = geometries.size() - 1;
				mainInfo = geometries[mainGeometryIndex].objInfo.info;
			}
		}
		else
			mainInfo = newObjInfo.info;

		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + newObjInfo.info.name, true, false);

		return true;
	}
	void GeometryData::GetGeometryData()
	{
		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + mainInfo.name, false, false);

		for (auto& geo : geometries)
		{
			logger::debug("{}::{} : get geometry data...", __func__, geo.objInfo.info.name);

			const std::size_t beforeVertexCount = vertices.size();
			const std::size_t beforeUVCount = uvs.size();
			const std::size_t beforeNormalCount = normals.size();
			const std::size_t beforeTangentCount = tangents.size();
			const std::size_t beforeBitangentCount = bitangents.size();
			vertices.resize(beforeVertexCount + geo.objInfo.info.vertexCount);
			uvs.resize(beforeUVCount + geo.objInfo.info.vertexCount);
			/*if (geo.objInfo.info.hasNormals)
				normals.resize(beforeNormalCount + geo.objInfo.info.vertexCount);
			if (geo.objInfo.info.hasTangents)
				tangents.resize(beforeTangentCount + geo.objInfo.info.vertexCount);
			if (geo.objInfo.info.hasBitangents)
				bitangents.resize(beforeBitangentCount + geo.objInfo.info.vertexCount);*/

			const std::uint32_t vertexSize = geo.objInfo.info.desc.GetSize();

			//float colorConvert = 1.0f / 255.0f;
			for (std::uint32_t i = 0; i < geo.objInfo.info.vertexCount; i++) {
				std::uint8_t* block = &geo.objInfo.geometryBlockData[i * vertexSize];
				const std::uint32_t vi = beforeVertexCount + i;
				const std::uint32_t ui = beforeUVCount + i;
				/*const std::uint32_t ni = beforeNormalCount + i;
				const std::uint32_t ti = beforeTangentCount + i;
				const std::uint32_t bi = beforeBitangentCount + i;*/

				if (!geo.objInfo.dynamicBlockData1.empty())
				{
					vertices[vi].x = geo.objInfo.dynamicBlockData1[i].x;
					vertices[vi].y = geo.objInfo.dynamicBlockData1[i].y;
					vertices[vi].z = geo.objInfo.dynamicBlockData1[i].z;
				}
				else if (!geo.objInfo.dynamicBlockData2.empty())
				{
					vertices[vi].x = geo.objInfo.dynamicBlockData2[i].x;
					vertices[vi].y = geo.objInfo.dynamicBlockData2[i].y;
					vertices[vi].z = geo.objInfo.dynamicBlockData2[i].z;
					/*if (geo.objInfo.info.hasBitangents)
						bitangents[bi].x = geo.objInfo.dynamicBlockData[i].w*/
				}
				if (geo.objInfo.info.hasVertices)
				{
					vertices[vi] = *reinterpret_cast<DirectX::XMFLOAT3*>(block);
					block += 12;

					/*if (geo.objInfo.info.hasBitangents)
						bitangents[bi].x = *reinterpret_cast<float*>(block);*/
					block += 4;
				}
				if (geo.objInfo.info.hasUVs)
				{
					uvs[ui].x = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block));
					uvs[ui].y = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block + 2));
					block += 4;
				}

				/*if (geo.objInfo.info.hasNormals)
				{
					normals[ni].x = static_cast<float>(*block) * colorConvert;
					normals[ni].y = static_cast<float>(*(block + 1)) * colorConvert;
					normals[ni].z = static_cast<float>(*(block + 2)) * colorConvert;
					block += 3;

					if (geo.objInfo.info.hasBitangents)
						bitangents[bi].y = static_cast<float>(*block);
					block += 1;

					if (geo.objInfo.info.hasTangents)
					{
						tangents[ti].x = static_cast<float>(*block) * colorConvert;
						tangents[ti].y = static_cast<float>(*(block + 1)) * colorConvert;
						tangents[ti].z = static_cast<float>(*(block + 2)) * colorConvert;
						block += 3;

						if (geo.objInfo.info.hasBitangents)
							bitangents[bi].z = static_cast<float>(*block);
					}
				}*/
			}

			const std::size_t beforeIndices = indices.size();
			indices.resize(beforeIndices + geo.objInfo.indicesBlockData.size());
			for (std::uint32_t t = 0; t < geo.objInfo.indicesBlockData.size(); t += 3)
			{
				indices[beforeIndices + t + 0] = beforeVertexCount + geo.objInfo.indicesBlockData[t + 0];
				indices[beforeIndices + t + 1] = beforeVertexCount + geo.objInfo.indicesBlockData[t + 1];
				indices[beforeIndices + t + 2] = beforeVertexCount + geo.objInfo.indicesBlockData[t + 2];
			}

			geo.objInfo.vertexStart = beforeVertexCount;
			geo.objInfo.vertexEnd = vertices.size();
			geo.objInfo.uvStart = beforeUVCount;
			geo.objInfo.uvEnd = uvs.size();
			geo.objInfo.normalStart = beforeNormalCount;
			geo.objInfo.normalEnd = normals.size();
			geo.objInfo.tangentStart = beforeTangentCount;
			geo.objInfo.tangentEnd = tangents.size();
			geo.objInfo.bitangentStart = beforeBitangentCount;
			geo.objInfo.bitangentEnd = bitangents.size();
			geo.objInfo.indicesStart = beforeIndices;
			geo.objInfo.indicesEnd = indices.size();

			XXH64_state_t* state = XXH64_createState();
			XXH64_reset(state, 0);
			XXH64_update(state, vertices.data() + geo.objInfo.vertexStart, geo.objInfo.vertexCount() * sizeof(DirectX::XMFLOAT3));
			XXH64_update(state, uvs.data() + geo.objInfo.uvStart, geo.objInfo.uvCount() * sizeof(DirectX::XMFLOAT2));
			geo.hash = XXH64_digest(state);
			XXH64_freeState(state);

			logger::info("{}::{} : get geometry data => vertices {} / uvs {} / tris {}", __func__, geo.objInfo.info.name.c_str(),
						 geo.objInfo.vertexCount(), geo.objInfo.uvCount(), geo.objInfo.indicesCount() / 3);
		}
		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + mainInfo.name, true, false);
	}

	void GeometryData::UpdateMap() {
		if (vertices.empty() || indices.empty())
			return;

		const std::size_t triCount = indices.size() / 3;

		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + std::to_string(triCount), false, false);

		vertexMap.clear();
		positionMap.clear();
		faceNormals.clear();
		faceTangents.clear();
		boundaryEdgeMap.clear();
		vertexToFaceMap.clear();
		boundaryEdgeVertexMap.clear();

		faceNormals.resize(triCount);
		faceTangents.resize(triCount);
		vertexToFaceMap.resize(vertices.size());
		boundaryEdgeVertexMap.resize(vertices.size());

		std::vector<std::future<void>> processes;
		const std::size_t sub = std::max((std::size_t)1, std::min(triCount, processingThreads->GetThreads() * 4));
		const std::size_t unit = (triCount + sub - 1) / sub;
		for (std::size_t p = 0; p < sub; p++) {
			const std::size_t begin = p * unit;
			const std::size_t end = std::min(begin + unit, triCount);
			processes.push_back(processingThreads->submitAsync([&, begin, end]() {
				for (std::size_t i = begin; i < end; i++)
				{
					const std::size_t offset = i * 3;
					const std::uint32_t i0 = indices[offset + 0];
					const std::uint32_t i1 = indices[offset + 1];
					const std::uint32_t i2 = indices[offset + 2];

					const DirectX::XMFLOAT3& p0 = vertices[i0];
					const DirectX::XMFLOAT3& p1 = vertices[i1];
					const DirectX::XMFLOAT3& p2 = vertices[i2];

					const DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&p0);
					const DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&p1);
					const DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&p2);

					const DirectX::XMFLOAT2& uv0 = uvs[i0];
					const DirectX::XMFLOAT2& uv1 = uvs[i1];
					const DirectX::XMFLOAT2& uv2 = uvs[i2];

					// Normal
					const DirectX::XMVECTOR normalVec = DirectX::XMVector3Normalize(
						DirectX::XMVector3Cross(
							DirectX::XMVectorSubtract(v1, v0),
							DirectX::XMVectorSubtract(v2, v0)
						)
					);
					DirectX::XMFLOAT3 normal;
					DirectX::XMStoreFloat3(&normal, normalVec);
					faceNormals[i] = { i0, i1, i2, normal };

					vertexToFaceMap[i0].push_back(i);
					vertexToFaceMap[i1].push_back(i);
					vertexToFaceMap[i2].push_back(i);

					// Tangent / Bitangent
					const DirectX::XMVECTOR dp1 = DirectX::XMVectorSubtract(v1, v0);
					const DirectX::XMVECTOR dp2 = DirectX::XMVectorSubtract(v2, v0);

					const DirectX::XMFLOAT2 duv1 = { uv1.x - uv0.x, uv1.y - uv0.y };
					const DirectX::XMFLOAT2 duv2 = { uv2.x - uv0.x, uv2.y - uv0.y };

					float r = duv1.x * duv2.y - duv2.x * duv1.y;
					r = (fabs(r) < floatPrecision) ? 1.0f : 1.0f / r;

					const DirectX::XMVECTOR tangentVec = DirectX::XMVectorScale(
						DirectX::XMVectorSubtract(
							DirectX::XMVectorScale(dp1, duv2.y),
							DirectX::XMVectorScale(dp2, duv1.y)
						), r);

					const DirectX::XMVECTOR bitangentVec = DirectX::XMVectorScale(
						DirectX::XMVectorSubtract(
							DirectX::XMVectorScale(dp2, duv1.x),
							DirectX::XMVectorScale(dp1, duv2.x)
						), r);

					DirectX::XMFLOAT3 tangent, bitangent;
					DirectX::XMStoreFloat3(&tangent, tangentVec);
					DirectX::XMStoreFloat3(&bitangent, bitangentVec);
					faceTangents[i] = { i0, i1, i2, tangent, bitangent };

					vertexMap[MakeVertexKey(p0, uv0)][i];
					vertexMap[MakeVertexKey(p1, uv1)][i];
					vertexMap[MakeVertexKey(p2, uv2)][i];
					positionMap[MakePositionKey(p0)][i0];
					positionMap[MakePositionKey(p1)][i1];
					positionMap[MakePositionKey(p2)][i2];

					vertexMap[MakeBoundaryVertexKey(p0, uv0)][i];
					vertexMap[MakeBoundaryVertexKey(p1, uv1)][i];
					vertexMap[MakeBoundaryVertexKey(p2, uv2)][i];
					positionMap[MakeBoundaryPositionKey(p0)][i0];
					positionMap[MakeBoundaryPositionKey(p1)][i1];
					positionMap[MakeBoundaryPositionKey(p2)][i2];

					boundaryEdgeMap[{i0, i1}].push_back(i);
					boundaryEdgeMap[{i0, i2}].push_back(i);
					boundaryEdgeMap[{i1, i2}].push_back(i);

					boundaryEdgeVertexMap[i0].push_back({ i0, i1 });
					boundaryEdgeVertexMap[i0].push_back({ i0, i2 });
					boundaryEdgeVertexMap[i1].push_back({ i0, i1 });
					boundaryEdgeVertexMap[i1].push_back({ i1, i2 });
					boundaryEdgeVertexMap[i2].push_back({ i0, i2 });
					boundaryEdgeVertexMap[i2].push_back({ i1, i2 });
				}
			}));
		}
		for (auto& process : processes) {
			process.get();
		}
		logger::debug("{}::{} : vertex map updated, vertices {} / uvs {} / tris {}", __func__,
					 mainInfo.name, vertices.size(), uvs.size(), triCount);

		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + std::to_string(triCount), true, false);
	}

	void GeometryData::RecalculateNormals(float a_smoothDegree)
	{
		if (vertices.empty() || indices.empty() || a_smoothDegree <= floatPrecision)
			return;

		mainInfo.hasNormals = true;
		mainInfo.hasTangents = true;
		mainInfo.hasBitangents = true;
		for (auto& geo : geometries) {
			geo.objInfo.info.hasNormals = true;
			geo.objInfo.normalStart = geo.objInfo.vertexStart;
			geo.objInfo.normalEnd = geo.objInfo.vertexEnd;

			geo.objInfo.info.hasTangents = true;
			geo.objInfo.tangentStart = geo.objInfo.vertexStart;
			geo.objInfo.tangentEnd = geo.objInfo.vertexEnd;

			geo.objInfo.info.hasBitangents = true;
			geo.objInfo.bitangentStart = geo.objInfo.vertexStart;
			geo.objInfo.bitangentEnd = geo.objInfo.vertexEnd;
		}
		normals.resize(vertices.size());
		tangents.resize(vertices.size());
		bitangents.resize(vertices.size());

		logger::debug("{} : normals {} re-calculate...", __func__, normals.size());
		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + std::to_string(normals.size()), false, false);

		const float smoothCos = std::cosf(DirectX::XMConvertToRadians(a_smoothDegree));

		std::vector<std::future<void>> processes;
		const std::size_t sub = std::max((std::size_t)1, std::min(vertices.size(), processingThreads->GetThreads() * 4));
		const std::size_t unit = (vertices.size() + sub - 1) / sub;
		for (std::size_t p = 0; p < sub; p++) {
			const std::size_t begin = p * unit;
			const std::size_t end = std::min(begin + unit, vertices.size());
			processes.push_back(processingThreads->submitAsync([&, begin, end]() {
				for (std::size_t i = begin; i < end; i++)
				{
					const DirectX::XMFLOAT3& pos = vertices[i];
					const DirectX::XMFLOAT2& uv = uvs[i];

					const VertexKey vkey = MakeVertexKey(pos, uv);
					const auto it = vertexMap.find(vkey);
					if (it == vertexMap.end())
						continue;

					DirectX::XMVECTOR nSelf = emptyVector;
					std::uint32_t selfCount = 0;
					for (const auto& fi : it->second) {
						const auto& fn = faceNormals[fi.first];
						nSelf = DirectX::XMVectorAdd(nSelf, DirectX::XMLoadFloat3(&fn.normal));
						selfCount++;

					}
					if (selfCount == 0)
						continue;

					nSelf = DirectX::XMVector3Normalize(nSelf);

					const PositionKey pkey = MakePositionKey(pos);
					const auto posIt = positionMap.find(pkey);
					if (posIt == positionMap.end())
						continue;

					DirectX::XMVECTOR nSum = emptyVector;
					DirectX::XMVECTOR tSum = emptyVector;
					DirectX::XMVECTOR bSum = emptyVector;

					std::unordered_set<std::uint32_t> pastVertices;
					for (const auto& vi : posIt->second) {
						pastVertices.insert(vi.first);
						for (const auto& fi : vertexToFaceMap[vi.first]) {
							const auto& fn = faceNormals[fi];
							DirectX::XMVECTOR fnVec = DirectX::XMLoadFloat3(&fn.normal);
							float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(fnVec, nSelf));
							if (Config::GetSingleton().GetAllowInvertNormalSmooth())
							{
								if (dot < 0)
								{
									dot = -dot;
									fnVec = DirectX::XMVectorNegate(fnVec);
								}
							}
							if (dot < smoothCos)
								continue;
							const auto& ft = faceTangents[fi];
							nSum = DirectX::XMVectorAdd(nSum, fnVec);
							tSum = DirectX::XMVectorAdd(tSum, DirectX::XMLoadFloat3(&ft.tangent));
							bSum = DirectX::XMVectorAdd(bSum, DirectX::XMLoadFloat3(&ft.bitangent));
						}
					}

					if (IsBoundaryVertex(i))
					{
						const PositionKey altPkey = MakeBoundaryPositionKey(pos);
						const auto altPosIt = positionMap.find(altPkey);
						if (altPosIt != positionMap.end())
						{
							for (const auto& vi : altPosIt->second) {
								if (IsSameGeometry(i, vi.first) || pastVertices.find(vi.first) != pastVertices.end())
									continue;
								for (const auto& fi : vertexToFaceMap[vi.first]) {
									const auto& fn = faceNormals[fi];
									DirectX::XMVECTOR fnVec = DirectX::XMLoadFloat3(&fn.normal);
									float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(fnVec, nSelf));
									if (Config::GetSingleton().GetAllowInvertNormalSmooth())
									{
										if (dot < 0)
										{
											dot = -dot;
											fnVec = DirectX::XMVectorNegate(fnVec);
										}
									}
									if (dot < smoothCos)
										continue;
									const auto& ft = faceTangents[fi];
									nSum = DirectX::XMVectorAdd(nSum, fnVec);
									tSum = DirectX::XMVectorAdd(tSum, DirectX::XMLoadFloat3(&ft.tangent));
									bSum = DirectX::XMVectorAdd(bSum, DirectX::XMLoadFloat3(&ft.bitangent));
								}
							}
						}
					}

					if (DirectX::XMVector3Equal(nSum, emptyVector))
						continue;

					DirectX::XMVECTOR t = DirectX::XMVectorGetX(DirectX::XMVector3Length(tSum)) > floatPrecision ? DirectX::XMVector3Normalize(tSum) : emptyVector;
					DirectX::XMVECTOR b = DirectX::XMVectorGetX(DirectX::XMVector3Length(bSum)) > floatPrecision ? DirectX::XMVector3Normalize(bSum) : emptyVector;
					const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(nSum);

					if (!DirectX::XMVector3Equal(t, emptyVector)) {
						t = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t)))));
						b = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(n, t));
					}

					if (!DirectX::XMVector3Equal(t, emptyVector))
						DirectX::XMStoreFloat3(&tangents[i], t);
					if (!DirectX::XMVector3Equal(b, emptyVector))
						DirectX::XMStoreFloat3(&bitangents[i], b);
					DirectX::XMStoreFloat3(&normals[i], n);
				}
			}));
		}
		for (auto& process : processes) {
			process.get();
		}

		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + std::to_string(normals.size()), true, false);
		logger::debug("{}::{} : normals {} re-calculated", __func__, mainInfo.name, normals.size());
		return;
	}

	void GeometryData::Subdivision(std::uint32_t a_subCount, std::uint32_t a_triThreshold)
	{
		if (a_subCount == 0)
			return;

		std::string subID = std::to_string(vertices.size());
		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + subID, false, false);

		logger::debug("{} : {} subdivition({})...", __func__, vertices.size(), a_subCount);

		struct LocalDate {
			RE::BSGeometry* geometry;
			std::string geoName;
			GeometryInfo geoInfo;
			std::vector<DirectX::XMFLOAT3> vertices;
			std::vector<DirectX::XMFLOAT2> uvs;
			std::vector<std::uint32_t> indices;
			ObjectInfo objInfo;
		};

		std::vector<LocalDate> subdividedDatas;
		for (auto& geometry : geometries)
		{
			LocalDate data(
				geometry.geometry,
				geometry.objInfo.info.name,
				geometry.objInfo.info
			);

			data.vertices.assign(vertices.begin() + geometry.objInfo.vertexStart, vertices.begin() + geometry.objInfo.vertexEnd);
			data.uvs.assign(uvs.begin() + geometry.objInfo.uvStart, uvs.begin() + geometry.objInfo.uvEnd);
			data.indices.assign(indices.begin() + geometry.objInfo.indicesStart, indices.begin() + geometry.objInfo.indicesEnd);

			std::size_t triCount = data.indices.size() / 3;
			std::vector<std::future<void>> processes;
			const std::size_t sub = std::max((std::size_t)1, std::min(triCount, processingThreads->GetThreads()));
			const std::size_t unit = (triCount + sub - 1) / sub;
			for (std::size_t p = 0; p < sub; p++) {
				const std::size_t begin = p * unit;
				const std::size_t end = std::min(begin + unit, triCount);
				processes.push_back(processingThreads->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++)
					{
						std::size_t offset = i * 3;
						data.indices[offset + 0] -= geometry.objInfo.vertexStart;
						data.indices[offset + 1] -= geometry.objInfo.vertexStart;
						data.indices[offset + 2] -= geometry.objInfo.vertexStart;
					}
				}));
			}
			for (auto& process : processes) {
				process.get();
			}

			data.objInfo = geometry.objInfo;

			subdividedDatas.push_back(data);
		}

		vertices.clear();
		uvs.clear();
		indices.clear();
		geometries.clear();

		for (auto& data : subdividedDatas)
		{
			if (data.indices.size() / 3 < a_triThreshold)
			{
				for (std::uint32_t doSubdivision = 0; doSubdivision < a_subCount; doSubdivision++)
				{
					std::unordered_map<std::size_t, std::uint32_t> midpointMap;
					auto getMidpointIndex = [&](std::uint32_t i0, std::uint32_t i1) -> std::uint32_t {
						auto createKey = [](std::uint32_t i0, std::uint32_t i1) -> std::size_t {
							std::uint32_t a = (std::min)(i0, i1);
							std::uint32_t b = std::max(i0, i1);
							return (static_cast<std::size_t>(a) << 32) | b;
						};

						auto key = createKey(i0, i1);
						if (auto it = midpointMap.find(key); it != midpointMap.end())
							return it->second;

						std::size_t index = data.vertices.size();

						const auto& v0 = data.vertices[i0];
						const auto& v1 = data.vertices[i1];
						DirectX::XMFLOAT3 midVertex = {
							(v0.x + v1.x) * 0.5f,
							(v0.y + v1.y) * 0.5f,
							(v0.z + v1.z) * 0.5f
						};
						data.vertices.push_back(midVertex);

						const auto& u0 = data.uvs[i0];
						const auto& u1 = data.uvs[i1];
						DirectX::XMFLOAT2 midUV = {
							(u0.x + u1.x) * 0.5f,
							(u0.y + u1.y) * 0.5f
						};
						data.uvs.push_back(midUV);

						midpointMap[key] = index;
							return index;
						};

					auto oldIndices = data.indices;
					data.indices.resize(data.indices.size() * 4);
					for (std::size_t i = 0; i < oldIndices.size() / 3; i++)
					{
						std::size_t offset = i * 3;
						std::uint32_t v0 = oldIndices[offset + 0];
						std::uint32_t v1 = oldIndices[offset + 1];
						std::uint32_t v2 = oldIndices[offset + 2];

						std::uint32_t m01 = getMidpointIndex(v0, v1);
						std::uint32_t m12 = getMidpointIndex(v1, v2);
						std::uint32_t m20 = getMidpointIndex(v2, v0);

						std::size_t triOffset = offset * 4;
						data.indices[triOffset + 0] = v0;
						data.indices[triOffset + 1] = m01;
						data.indices[triOffset + 2] = m20;

						data.indices[triOffset + 3] = v1;
						data.indices[triOffset + 4] = m12;
						data.indices[triOffset + 5] = m01;

						data.indices[triOffset + 6] = v2;
						data.indices[triOffset + 7] = m20;
						data.indices[triOffset + 8] = m12;

						data.indices[triOffset + 9] = m01;
						data.indices[triOffset + 10] = m12;
						data.indices[triOffset + 11] = m20;
					}
				}
			}

			ObjectInfo newObjInfo;
			newObjInfo.info = data.geoInfo;
			newObjInfo.vertexStart = vertices.size();
			newObjInfo.uvStart = uvs.size();
			newObjInfo.indicesStart = indices.size();

			vertices.insert(vertices.end(), data.vertices.begin(), data.vertices.end());
			uvs.insert(uvs.end(), data.uvs.begin(), data.uvs.end());

			std::size_t triCount = data.indices.size() / 3;
			std::vector<std::future<void>> processes;
			const std::size_t sub = std::max((std::size_t)1, std::min(triCount, processingThreads->GetThreads()));
			const std::size_t unit = (triCount + sub - 1) / sub;
			for (std::size_t p = 0; p < sub; p++) {
				const std::size_t begin = p * unit;
				const std::size_t end = std::min(begin + unit, triCount);
				processes.push_back(processingThreads->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++)
					{
						std::size_t offset = i * 3;
						data.indices[offset + 0] += newObjInfo.vertexStart;
						data.indices[offset + 1] += newObjInfo.vertexStart;
						data.indices[offset + 2] += newObjInfo.vertexStart;
					}
				}));
			}
			for (auto& process : processes) {
				process.get();
			}

			indices.insert(indices.end(), data.indices.begin(), data.indices.end());

			newObjInfo.vertexEnd = vertices.size();
			newObjInfo.uvEnd = uvs.size();
			newObjInfo.indicesEnd = indices.size();

			geometries.push_back(GeometriesInfo{ data.geometry, newObjInfo });
		}

		logger::debug("{} : {} subdivition done", __func__, vertices.size());

		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + subID, true, false);
		return;
	}

	void GeometryData::VertexSmooth(float a_strength, std::uint32_t a_smoothCount)
	{
		if (vertices.empty() || a_smoothCount == 0 || a_strength <= floatPrecision)
			return;

		logger::debug("{} : {} vertex smooth({})...", __func__, vertices.size(), a_smoothCount);

		for (std::uint32_t doSmooth = 0; doSmooth < a_smoothCount; doSmooth++)
		{
			if (Config::GetSingleton().GetGeometryDataTime())
				PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), false, false);

			auto tempVertices = vertices;
			std::vector<std::future<void>> processes;
			const std::size_t sub = std::max((std::size_t)1, std::min(vertices.size(), processingThreads->GetThreads() * 8));
			const std::size_t unit = (vertices.size() + sub - 1) / sub;
			for (std::size_t p = 0; p < sub; p++) {
				const std::size_t begin = p * unit;
				const std::size_t end = std::min(begin + unit, vertices.size());
				processes.push_back(processingThreads->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++)
					{
						const DirectX::XMFLOAT3& pos = tempVertices[i];
						const PositionKey pkey = MakePositionKey(pos);
						const auto posIt = positionMap.find(pkey);
						if (posIt == positionMap.end())
							continue;

						std::unordered_set<std::uint32_t> connectedVertices;
						std::unordered_set<std::uint32_t> pastVertices;
						for (const auto& vi : posIt->second) {
							pastVertices.insert(vi.first);
							for (const auto& fi : vertexToFaceMap[vi.first]) {
								const auto& fn = faceNormals[fi];
								if (fn.v0 != vi.first)
									connectedVertices.insert(fn.v0);
								if (fn.v1 != vi.first)
									connectedVertices.insert(fn.v1);
								if (fn.v2 != vi.first)
									connectedVertices.insert(fn.v2);
							}
						}

						if (IsBoundaryVertex(i))
						{
							const PositionKey altPkey = MakeBoundaryPositionKey(pos);
							const auto altPosIt = positionMap.find(altPkey);
							if (altPosIt != positionMap.end())
							{
								for (const auto& vi : altPosIt->second) {
									if (IsSameGeometry(i, vi.first) || pastVertices.find(vi.first) != pastVertices.end())
										continue;
									for (const auto& fi : vertexToFaceMap[vi.first]) {
										const auto& fn = faceNormals[fi];
										if (fn.v0 != vi.first)
											connectedVertices.insert(fn.v0);
										if (fn.v1 != vi.first)
											connectedVertices.insert(fn.v1);
										if (fn.v2 != vi.first)
											connectedVertices.insert(fn.v2);
									}
								}
							}
						}

						if (connectedVertices.size() == 0)
							continue;

						DirectX::XMVECTOR avgPos = emptyVector;
						for (const auto& cvi : connectedVertices)
						{
							avgPos = DirectX::XMVectorAdd(avgPos, DirectX::XMLoadFloat3(&tempVertices[cvi]));
						}
						avgPos = DirectX::XMVectorScale(avgPos, 1.0f / connectedVertices.size());
						const DirectX::XMVECTOR original = DirectX::XMLoadFloat3(&pos);
						const DirectX::XMVECTOR smoothed = DirectX::XMVectorLerp(original, avgPos, a_strength);
						DirectX::XMStoreFloat3(&vertices[i], smoothed);
					}
				}));
			}
			for (auto& process : processes) {
				process.get();
			}

			if (Config::GetSingleton().GetGeometryDataTime())
				PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), true, false);

			UpdateMap();
		}

		logger::debug("{} : {} vertex smooth done", __func__, vertices.size());
		return;
	}

	void GeometryData::VertexSmoothByAngle(float a_smoothThreshold1, float a_smoothThreshold2, std::uint32_t a_smoothCount)
	{
		if (vertices.empty() || a_smoothCount == 0)
			return;
		if (a_smoothThreshold1 > a_smoothThreshold2)
			a_smoothThreshold1 = a_smoothThreshold2;

		logger::debug("{} : {} vertex smooth by angle({})...", __func__, vertices.size(), a_smoothCount);
		const float smoothCos1 = std::cosf(DirectX::XMConvertToRadians(a_smoothThreshold1));
		const float smoothCos2 = std::cosf(DirectX::XMConvertToRadians(a_smoothThreshold2));

		for (std::uint32_t doSmooth = 0; doSmooth < a_smoothCount; doSmooth++)
		{
			if (Config::GetSingleton().GetGeometryDataTime())
				PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), false, false);

			auto tempVertices = vertices;
			std::vector<std::future<void>> processes;
			const std::size_t sub = std::max((std::size_t)1, std::min(vertices.size(), processingThreads->GetThreads() * 8));
			const std::size_t unit = (vertices.size() + sub - 1) / sub;
			for (std::size_t p = 0; p < sub; p++) {
				const std::size_t begin = p * unit;
				const std::size_t end = std::min(begin + unit, vertices.size());
				processes.push_back(processingThreads->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++)
					{
						const DirectX::XMFLOAT3& pos = tempVertices[i];
						const PositionKey pkey = MakePositionKey(pos);
						const auto posIt = positionMap.find(pkey);
						if (posIt == positionMap.end())
							continue;

						DirectX::XMVECTOR nSelf = emptyVector;
						std::uint32_t selfCount = 0;
						std::unordered_set<std::uint32_t> pastVertices;
						for (const auto& vi : posIt->second) {
							pastVertices.insert(vi.first);
							for (const auto& fi : vertexToFaceMap[vi.first]) {
								const auto& fn = faceNormals[fi];
								nSelf = DirectX::XMVectorAdd(nSelf, DirectX::XMLoadFloat3(&fn.normal));
								selfCount++;
							}
						}

						if (IsBoundaryVertex(i))
						{
							const PositionKey altPkey = MakeBoundaryPositionKey(pos);
							const auto altPosIt = positionMap.find(altPkey);
							if (altPosIt != positionMap.end())
							{
								for (const auto& vi : altPosIt->second) {
									if (IsSameGeometry(i, vi.first) || pastVertices.find(vi.first) != pastVertices.end())
										continue;
									for (const auto& fi : vertexToFaceMap[vi.first]) {
										const auto& fn = faceNormals[fi];
										nSelf = DirectX::XMVectorAdd(nSelf, DirectX::XMLoadFloat3(&fn.normal));
										selfCount++;
									}
								}
							}
						}
						if (selfCount == 0)
							continue;
						nSelf = DirectX::XMVector3Normalize(nSelf);

						std::unordered_set<std::uint32_t> connectedVertices;
						pastVertices.clear();
						float dotTotal = 0.0f;
						std::uint32_t dotCount = 0;
						for (const auto& vi : posIt->second) {
							pastVertices.insert(vi.first);
							for (const auto& fi : vertexToFaceMap[vi.first]) {
								const auto& fn = faceNormals[fi];
								const float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(nSelf, DirectX::XMLoadFloat3(&fn.normal)));
								if (dot > smoothCos1)
									continue;
								if (fn.v0 != vi.first)
									connectedVertices.insert(fn.v0);
								if (fn.v1 != vi.first)
									connectedVertices.insert(fn.v1);
								if (fn.v2 != vi.first)
									connectedVertices.insert(fn.v2);
								dotTotal += dot;
								dotCount++;
							}
						}
						if (IsBoundaryVertex(i))
						{
							const PositionKey altPkey = MakeBoundaryPositionKey(pos);
							const auto altPosIt = positionMap.find(altPkey);
							if (altPosIt != positionMap.end())
							{
								for (const auto& vi : altPosIt->second) {
									if (IsSameGeometry(i, vi.first) || pastVertices.find(vi.first) != pastVertices.end())
										continue;
									for (const auto& fi : vertexToFaceMap[vi.first]) {
										const auto& fn = faceNormals[fi];
										const float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(nSelf, DirectX::XMLoadFloat3(&fn.normal)));
										if (dot > smoothCos1)
											continue;
										if (fn.v0 != vi.first)
											connectedVertices.insert(fn.v0);
										if (fn.v1 != vi.first)
											connectedVertices.insert(fn.v1);
										if (fn.v2 != vi.first)
											connectedVertices.insert(fn.v2);
										dotTotal += dot;
										dotCount++;
									}
								}
							}
						}
						if (connectedVertices.size() == 0)
							continue;

						DirectX::XMVECTOR avgPos = emptyVector;
						const float avgDot = dotTotal / dotCount;
						const float strength = SmoothStepRange(avgDot, smoothCos1, smoothCos2);
						for (const auto& cvi : connectedVertices)
						{
							avgPos = DirectX::XMVectorAdd(avgPos, DirectX::XMLoadFloat3(&tempVertices[cvi]));
						}
						avgPos = DirectX::XMVectorScale(avgPos, 1.0f / connectedVertices.size());
						const DirectX::XMVECTOR original = DirectX::XMLoadFloat3(&pos);
						const DirectX::XMVECTOR smoothed = DirectX::XMVectorLerp(original, avgPos, strength);
						DirectX::XMStoreFloat3(&vertices[i], smoothed);
					}
				}));
			}
			for (auto& process : processes) {
				process.get();
			}

			if (Config::GetSingleton().GetGeometryDataTime())
				PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), true, false);

			UpdateMap();
		}
	}

	float GeometryData::SmoothStepRange(float x, float A, float B)
	{
		if (x > A)
			return 0.0f;
		else if (x <= B)
			return 1.0f;
		const float t = (A - x) / (A - B);
		return t * t * (3.0f - 2.0f * t);
	}
}
