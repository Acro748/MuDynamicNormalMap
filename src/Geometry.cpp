#include "Geometry.h"

namespace Mus {
//#define GEOMETRY_TEST

	GeometryData::GeometryData(RE::BSGeometry* a_geo)
	{
		CopyGeometryData(a_geo);
	}

	RE::BSFaceGenBaseMorphExtraData* GeometryData::GetMorphExtraData(RE::BSGeometry* a_geometry)
	{
		return netimmerse_cast<RE::BSFaceGenBaseMorphExtraData*>(a_geometry->GetExtraData("FOD"));
	}

	bool GeometryData::GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info)
	{
		if (!a_geo || a_geo->name.empty())
			return false;

		info.desc = a_geo->GetGeometryRuntimeData().vertexDesc;
		info.name = a_geo->name.c_str();
		info.hasVertices = a_geo->AsDynamicTriShape() ? true : info.desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
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

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + newObjInfo.info.name, false, false);
#endif // GEOMETRY_TEST


		const RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape();
		const RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return false;

		if (!newObjInfo.info.hasVertices || !newObjInfo.info.hasUVs)
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
		newObjInfo.geometryBlockData.resize(newObjInfo.info.vertexCount * newObjInfo.info.desc.GetSize());
		std::memcpy(newObjInfo.geometryBlockData.data(), skinPartition->partitions[0].buffData->rawVertexData, sizeof(std::uint8_t) * newObjInfo.info.vertexCount * newObjInfo.info.desc.GetSize());

		std::uint32_t indexOffset = 0;
		for (auto& partition : skinPartition->partitions)
		{
			indexOffset += partition.triangles * 3;
		}
		newObjInfo.indicesBlockData.resize(indexOffset);
		indexOffset = 0;
		for (auto& partition : skinPartition->partitions)
		{
			std::memcpy(&newObjInfo.indicesBlockData[indexOffset], partition.triList, sizeof(std::uint16_t) * partition.triangles * 3);
			indexOffset += partition.triangles * 3;
		}
		geometries.push_back(std::make_pair(std::string(a_geo->name.c_str()), newObjInfo));

		mainInfo.hasVertices |= newObjInfo.info.hasVertices;
		mainInfo.hasUVs |= newObjInfo.info.hasUVs;
		mainInfo.hasNormals |= newObjInfo.info.hasNormals;
		mainInfo.hasTangents |= newObjInfo.info.hasTangents;
		mainInfo.hasBitangents |= newObjInfo.info.hasBitangents;

		if (geometries.size() > 1) {
			if (geometries[mainGeometryIndex].second.indicesBlockData.size() < geometries[geometries.size() - 1].second.indicesBlockData.size())
				mainGeometryIndex = geometries.size() - 1;
		}

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + newObjInfo.info.name, true, false);
#endif // GEOMETRY_TEST

		return true;
	}
	void GeometryData::GetGeometryData()
	{
#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + mainInfo.name, false, false);
#endif // GEOMETRY_TEST

		for (auto& geo : geometries)
		{
			logger::debug("{}::{} : get geometry data...", __func__, geo.second.info.name);

			const std::size_t beforeVertexCount = vertices.size();
			const std::size_t beforeUVCount = uvs.size();
			const std::size_t beforeNormalCount = normals.size();
			const std::size_t beforeTangentCount = tangents.size();
			const std::size_t beforeBitangentCount = bitangents.size();
			vertices.resize(beforeVertexCount + geo.second.info.vertexCount);
			uvs.resize(beforeUVCount + geo.second.info.vertexCount);
			/*if (geo.second.info.hasNormals)
				normals.resize(beforeNormalCount + geo.second.info.vertexCount);
			if (geo.second.info.hasTangents)
				tangents.resize(beforeTangentCount + geo.second.info.vertexCount);
			if (geo.second.info.hasBitangents)
				bitangents.resize(beforeBitangentCount + geo.second.info.vertexCount);*/

			const std::uint32_t vertexSize = geo.second.info.desc.GetSize();

			float colorConvert = 1.0f / 255.0f;
			for (std::uint32_t i = 0; i < geo.second.info.vertexCount; i++) {
				std::uint8_t* block = &geo.second.geometryBlockData[i * vertexSize];
				const std::uint32_t vi = beforeVertexCount + i;
				const std::uint32_t ui = beforeUVCount + i;
				const std::uint32_t ni = beforeNormalCount + i;
				const std::uint32_t ti = beforeTangentCount + i;
				const std::uint32_t bi = beforeBitangentCount + i;

				if (!geo.second.dynamicBlockData1.empty())
				{
					vertices[vi].x = geo.second.dynamicBlockData1[i].x;
					vertices[vi].y = geo.second.dynamicBlockData1[i].y;
					vertices[vi].z = geo.second.dynamicBlockData1[i].z;
				}
				else if (!geo.second.dynamicBlockData2.empty())
				{
					DirectX::XMStoreFloat3(&vertices[vi], geo.second.dynamicBlockData2[i]);
					/*if (geo.second.info.hasBitangents)
						bitangents[bi].x = geo.second.dynamicBlockData[i].m128_f32[3];*/
				}
				else
				{
					vertices[vi] = *reinterpret_cast<DirectX::XMFLOAT3*>(block);
					block += 12;

					/*if (geo.second.info.hasBitangents)
						bitangents[bi].x = *reinterpret_cast<float*>(block);*/
					block += 4;
				}

				uvs[ui].x = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block));
				uvs[ui].y = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block + 2));
				block += 4;

				/*if (geo.second.info.hasNormals)
				{
					normals[ni].x = static_cast<float>(*block) * colorConvert;
					normals[ni].y = static_cast<float>(*(block + 1)) * colorConvert;
					normals[ni].z = static_cast<float>(*(block + 2)) * colorConvert;
					block += 3;

					if (geo.second.info.hasBitangents)
						bitangents[bi].y = static_cast<float>(*block);
					block += 1;

					if (geo.second.info.hasTangents)
					{
						tangents[ti].x = static_cast<float>(*block) * colorConvert;
						tangents[ti].y = static_cast<float>(*(block + 1)) * colorConvert;
						tangents[ti].z = static_cast<float>(*(block + 2)) * colorConvert;
						block += 3;

						if (geo.second.info.hasBitangents)
							bitangents[bi].z = static_cast<float>(*block);
					}
				}*/
			}

			const std::size_t beforeIndices = indices.size();
			indices.resize(beforeIndices + geo.second.indicesBlockData.size());
			for (std::uint32_t t = 0; t < geo.second.indicesBlockData.size(); t += 3)
			{
				indices[beforeIndices + t + 0] = beforeVertexCount + geo.second.indicesBlockData[t + 0];
				indices[beforeIndices + t + 1] = beforeVertexCount + geo.second.indicesBlockData[t + 1];
				indices[beforeIndices + t + 2] = beforeVertexCount + geo.second.indicesBlockData[t + 2];
			}

			geo.second.vertexStart = beforeVertexCount;
			geo.second.vertexEnd = vertices.size();
			geo.second.uvStart = beforeUVCount;
			geo.second.uvEnd = uvs.size();
			geo.second.normalStart = beforeNormalCount;
			geo.second.normalEnd = normals.size();
			geo.second.tangentStart = beforeTangentCount;
			geo.second.tangentEnd = tangents.size();
			geo.second.bitangentStart = beforeBitangentCount;
			geo.second.bitangentEnd = bitangents.size();
			geo.second.indicesStart = beforeIndices;
			geo.second.indicesEnd = indices.size();

			logger::info("{}::{} : get geometry data {} => vertices {} / uvs {} / tris {}", __func__, geo.second.info.name.c_str(), geometries.size(),
						 geo.second.vertexCount(), geo.second.uvCount(), geo.second.indicesCount() / 3);
		}
#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + mainInfo.name, true, false);
#endif // GEOMETRY_TEST
	}

	void GeometryData::UpdateMap() {
		if (vertices.empty() || indices.empty())
			return;
		vertexMap.clear();
		positionMap.clear();
		faceNormals.clear();
		faceTangents.clear();

		std::size_t triCount = indices.size() / 3;
		faceNormals.resize(triCount);
		faceTangents.resize(triCount);
		vertexToFaceMap.resize(vertices.size());

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(indices.size()), false, false);
#endif // GEOMETRY_TEST
		concurrency::parallel_for(std::size_t(0), triCount, [&](std::size_t i) {
			std::size_t offset = i * 3;
			std::uint32_t i0 = indices[offset + 0];
			std::uint32_t i1 = indices[offset + 1];
			std::uint32_t i2 = indices[offset + 2];

			const DirectX::XMFLOAT3& p0 = vertices[i0];
			const DirectX::XMFLOAT3& p1 = vertices[i1];
			const DirectX::XMFLOAT3& p2 = vertices[i2];

			DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&p0);
			DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&p1);
			DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&p2);

			const DirectX::XMFLOAT2& uv0 = uvs[i0];
			const DirectX::XMFLOAT2& uv1 = uvs[i1];
			const DirectX::XMFLOAT2& uv2 = uvs[i2];

			// Normal
			DirectX::XMVECTOR normalVec = DirectX::XMVector3Normalize(
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
			DirectX::XMVECTOR dp1 = DirectX::XMVectorSubtract(v1, v0);
			DirectX::XMVECTOR dp2 = DirectX::XMVectorSubtract(v2, v0);

			DirectX::XMFLOAT2 duv1 = { uv1.x - uv0.x, uv1.y - uv0.y };
			DirectX::XMFLOAT2 duv2 = { uv2.x - uv0.x, uv2.y - uv0.y };

			float r = duv1.x * duv2.y - duv2.x * duv1.y;
			r = (fabs(r) < floatPrecision) ? 1.0f : 1.0f / r;

			DirectX::XMVECTOR tangentVec = DirectX::XMVectorScale(
				DirectX::XMVectorSubtract(
					DirectX::XMVectorScale(dp1, duv2.y),
					DirectX::XMVectorScale(dp2, duv1.y)
				), r);

			DirectX::XMVECTOR bitangentVec = DirectX::XMVectorScale(
				DirectX::XMVectorSubtract(
					DirectX::XMVectorScale(dp2, duv1.x),
					DirectX::XMVectorScale(dp1, duv2.x)
				), r);

			DirectX::XMFLOAT3 tangent, bitangent;
			DirectX::XMStoreFloat3(&tangent, tangentVec);
			DirectX::XMStoreFloat3(&bitangent, bitangentVec);
			faceTangents[i] = { i0, i1, i2, tangent, bitangent };

			vertexMap[{ p0, uv0 }].push_back(i);
			vertexMap[{ p1, uv1 }].push_back(i);
			vertexMap[{ p2, uv2 }].push_back(i);

			positionMap[{ p0 }].push_back(i0);
			positionMap[{ p1 }].push_back(i1);
			positionMap[{ p2 }].push_back(i2);
		});
		logger::debug("{}::{} : vertex map updated, vertices {} / uvs {} / tris {}", __func__,
					 mainInfo.name, vertices.size(), uvs.size(), indices.size() / 3);
#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(indices.size()), true, false);
#endif // GEOMETRY_TEST
	}

	void GeometryData::RecalculateNormals(float a_smooth)
	{
		if (!mainInfo.hasVertices || vertices.empty() || indices.empty())
			return;

		mainInfo.hasNormals = true;
		for (auto& geo : geometries) {
			geo.second.info.hasNormals = true;
			geo.second.normalStart = geo.second.vertexStart;
			geo.second.normalEnd = geo.second.vertexEnd;
		}
		normals.resize(vertices.size());

		mainInfo.hasTangents = true;
		for (auto& geo : geometries) {
			geo.second.info.hasTangents = true;
			geo.second.tangentStart = geo.second.vertexStart;
			geo.second.tangentEnd = geo.second.vertexEnd;
		}
		tangents.resize(vertices.size());

		mainInfo.hasBitangents = true;
		for (auto& geo : geometries) {
			geo.second.info.hasBitangents = true;
			geo.second.bitangentStart = geo.second.vertexStart;
			geo.second.bitangentEnd = geo.second.vertexEnd;
		}
		bitangents.resize(vertices.size());

		logger::debug("{} : normals {} re-calculate...", __func__, normals.size());
#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(normals.size()), false, false);
#endif // GEOMETRY_TEST
		const float smoothCos = std::cosf(DirectX::XMConvertToRadians(a_smooth));

		concurrency::parallel_for(std::size_t(0), vertices.size(), [&](std::size_t i) {
			const DirectX::XMFLOAT3& pos = vertices[i];
			const DirectX::XMFLOAT2& uv = uvs[i];

			VertexKey vkey = { pos, uv };
			auto it = vertexMap.find(vkey);
			if (it == vertexMap.end())
				return;

			const auto& exactFaceIndices = it->second;

			DirectX::XMVECTOR nSelf = DirectX::XMVectorZero();
			bool foundSelf = false;
			for (auto fi : exactFaceIndices) {
				const auto& fn = faceNormals[fi];
				if (fn.v0 == i || fn.v1 == i || fn.v2 == i) {
					nSelf = DirectX::XMLoadFloat3(&fn.normal);
					foundSelf = true;
					break;
				}
			}
			if (!foundSelf)
				return;

			PositionKey pkey = { pos };
			auto posIt = positionMap.find(pkey);
			if (posIt == positionMap.end())
				return;

			DirectX::XMVECTOR nSum = DirectX::XMVectorZero();
			DirectX::XMVECTOR tSum = DirectX::XMVectorZero();
			DirectX::XMVECTOR bSum = DirectX::XMVectorZero();

			for (auto vi : posIt->second) {
				for (auto fi : vertexToFaceMap[vi]) {
					const auto& fn = faceNormals[fi];
					if (fn.v0 == vi || fn.v1 == vi || fn.v2 == vi) {
						DirectX::XMVECTOR fnVec = DirectX::XMLoadFloat3(&fn.normal);
						float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(fnVec, nSelf));
						if (dot < smoothCos)
							continue;
						const auto& ft = faceTangents[fi];
						nSum = DirectX::XMVectorAdd(nSum, fnVec);
						tSum = DirectX::XMVectorAdd(tSum, DirectX::XMLoadFloat3(&ft.tangent));
						bSum = DirectX::XMVectorAdd(bSum, DirectX::XMLoadFloat3(&ft.bitangent));
					}
				}
			}

			if (DirectX::XMVector3Equal(nSum, DirectX::XMVectorZero()))
				return;

			DirectX::XMVECTOR t = DirectX::XMVectorGetX(DirectX::XMVector3Length(tSum)) > floatPrecision ? DirectX::XMVector3Normalize(tSum) : DirectX::XMVectorZero();
			DirectX::XMVECTOR b = DirectX::XMVectorGetX(DirectX::XMVector3Length(bSum)) > floatPrecision ? DirectX::XMVector3Normalize(bSum) : DirectX::XMVectorZero();
			const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(nSum);

			if (!DirectX::XMVector3Equal(t, DirectX::XMVectorZero())) {
				t = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t)))));
				const DirectX::XMVECTOR cross = DirectX::XMVector3Cross(n, t);
				const float handedness = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(cross, b)) < 0.0f) ? -1.0f : 1.0f;
				b = DirectX::XMVector3Normalize(DirectX::XMVectorScale(cross, handedness));
			}

			if (!DirectX::XMVector3Equal(t, DirectX::XMVectorZero()))
				DirectX::XMStoreFloat3(&tangents[i], t);
			if (!DirectX::XMVector3Equal(b, DirectX::XMVectorZero()))
				DirectX::XMStoreFloat3(&bitangents[i], b);
			DirectX::XMStoreFloat3(&normals[i], n);
		});

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(normals.size()), true, false);
#endif // GEOMETRY_TEST
		logger::debug("{}::{} : normals {} re-calculated", __func__, mainInfo.name, normals.size());
		return;
	}

	void GeometryData::Subdivision(std::uint32_t a_subCount)
	{
		if (a_subCount == 0)
			return;

		std::string subID = std::to_string(vertices.size());
#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + subID, false, false);
#endif // GEOMETRY_TEST

		logger::debug("{} : {} subdivition({})...", __func__, vertices.size(), a_subCount);

		struct LocalDate {
			std::string geoName;
			GeometryInfo geoInfo;
			std::vector<DirectX::XMFLOAT3> vertices;
			std::vector<DirectX::XMFLOAT2> uvs;
			std::vector<std::uint32_t> indices;
			ObjectInfo objInfo;
		};

		std::vector<LocalDate> subdividedDatas;
		subdividedDatas.reserve(geometries.size());
		for (auto& geometry : geometries)
		{
			LocalDate data(
				geometry.first,
				geometry.second.info
			);

			data.vertices.reserve(geometry.second.vertexCount());
			data.uvs.reserve(geometry.second.uvCount());
			data.indices.reserve(geometry.second.indicesCount());
			data.vertices.assign(vertices.begin() + geometry.second.vertexStart, vertices.begin() + geometry.second.vertexEnd);
			data.uvs.assign(uvs.begin() + geometry.second.uvStart, uvs.begin() + geometry.second.uvEnd);
			data.indices.assign(indices.begin() + geometry.second.indicesStart, indices.begin() + geometry.second.indicesEnd);
			data.objInfo = geometry.second;

			subdividedDatas.push_back(data);
		}

		for (auto& data : subdividedDatas)
		{
			for (std::uint32_t doSubdivision = 0; doSubdivision < a_subCount; doSubdivision++)
			{
				std::unordered_map<std::uint64_t, std::uint32_t> midpointMap;
				auto getMidpointIndex = [&](std::uint32_t i0, std::uint32_t i1) -> std::uint32_t {
					auto createKey = [](std::uint32_t i0, std::uint32_t i1) -> std::uint64_t {
						std::uint32_t a = (std::min)(i0, i1);
						std::uint32_t b = std::max(i0, i1);
						return (static_cast<uint64_t>(a) << 32) | b;
						};

					auto key = createKey(i0, i1);
					if (auto it = midpointMap.find(key); it != midpointMap.end())
						return it->second;

					auto normalize = [&](const DirectX::XMFLOAT3& v) -> DirectX::XMFLOAT3 {
						DirectX::XMVECTOR vec = DirectX::XMLoadFloat3(&v);
						vec = DirectX::XMVector3Normalize(vec);
						DirectX::XMFLOAT3 result;
						DirectX::XMStoreFloat3(&result, vec);
						return result;
						};

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
					std::uint32_t v0 = oldIndices[offset + 0] - data.objInfo.vertexStart;
					std::uint32_t v1 = oldIndices[offset + 1] - data.objInfo.vertexStart;
					std::uint32_t v2 = oldIndices[offset + 2] - data.objInfo.vertexStart;

					std::uint32_t m01 = getMidpointIndex(v0, v1);
					std::uint32_t m12 = getMidpointIndex(v1, v2);
					std::uint32_t m20 = getMidpointIndex(v2, v0);

					std::size_t triOffset = offset * 4;
					data.indices[triOffset + 0] = v0 + data.objInfo.vertexStart;
					data.indices[triOffset + 1] = m01 + data.objInfo.vertexStart;
					data.indices[triOffset + 2] = m20 + data.objInfo.vertexStart;

					data.indices[triOffset + 3] = v1 + data.objInfo.vertexStart;
					data.indices[triOffset + 4] = m12 + data.objInfo.vertexStart;
					data.indices[triOffset + 5] = m01 + data.objInfo.vertexStart;

					data.indices[triOffset + 6] = v2 + data.objInfo.vertexStart;
					data.indices[triOffset + 7] = m20 + data.objInfo.vertexStart;
					data.indices[triOffset + 8] = m12 + data.objInfo.vertexStart;

					data.indices[triOffset + 9] = m01 + data.objInfo.vertexStart;
					data.indices[triOffset + 10] = m12 + data.objInfo.vertexStart;
					data.indices[triOffset + 11] = m20 + data.objInfo.vertexStart;
				}
				subdividedDatas.push_back(data);
			}
		}

		LocalDate totalData;
		geometries.clear();
		for (auto& subdividedData : subdividedDatas) {
			ObjectInfo newObjInfo;
			newObjInfo.info = subdividedData.geoInfo;
			newObjInfo.vertexStart = totalData.vertices.size();
			newObjInfo.uvStart = totalData.uvs.size();
			newObjInfo.indicesStart = totalData.indices.size();

			totalData.vertices.insert(totalData.vertices.end(), subdividedData.vertices.begin(), subdividedData.vertices.end());
			totalData.uvs.insert(totalData.uvs.end(), subdividedData.uvs.begin(), subdividedData.uvs.end());
			totalData.indices.insert(totalData.indices.end(), subdividedData.indices.begin(), subdividedData.indices.end());

			newObjInfo.vertexEnd = totalData.vertices.size();
			newObjInfo.uvEnd = totalData.uvs.size();
			newObjInfo.indicesEnd = totalData.indices.size();

			geometries.push_back(std::make_pair(subdividedData.geoName, newObjInfo));
		}
		vertices = concurrency::concurrent_vector<DirectX::XMFLOAT3>(totalData.vertices.begin(), totalData.vertices.end());
		uvs = concurrency::concurrent_vector<DirectX::XMFLOAT2>(totalData.uvs.begin(), totalData.uvs.end());
		indices = concurrency::concurrent_vector<std::uint32_t>(totalData.indices.begin(), totalData.indices.end());

		logger::debug("{} : {} subdivition done", __func__, vertices.size());

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + subID, true, false);
#endif // GEOMETRY_TEST
		return;
	}

	void GeometryData::VertexSmooth(float a_strength, std::uint32_t a_smoothCount)
	{
		if (!mainInfo.hasVertices || vertices.empty() || a_smoothCount == 0)
			return;

		logger::debug("{} : {} vertex smooth({})...", __func__, vertices.size(), a_smoothCount);

		for (std::uint32_t doSmooth = 0; doSmooth < a_smoothCount; doSmooth++)
		{
#ifdef GEOMETRY_TEST
			PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), false, false);
#endif // GEOMETRY_TEST
			auto tempVertices = vertices;

			concurrency::parallel_for(std::size_t(0), vertices.size(), [&](std::size_t i) {
				const DirectX::XMFLOAT3& pos = vertices[i];
				const DirectX::XMFLOAT2& uv = uvs[i];

				PositionKey pkey = { pos };
				auto posIt = positionMap.find(pkey);
				if (posIt == positionMap.end())
					return;

				DirectX::XMVECTOR avgPos = DirectX::XMVectorZero();
				int totalCount = 0;

				std::unordered_set<std::uint32_t> connectedVertices;
				for (auto vi : posIt->second) {
					for (auto fi : vertexToFaceMap[vi]) {
						const auto& fn = faceNormals[fi];
						if (fn.v0 == vi || fn.v1 == vi || fn.v2 == vi) {
							connectedVertices.insert(fn.v0);
							connectedVertices.insert(fn.v1);
							connectedVertices.insert(fn.v2);
						}
					}
				}

				for (auto vi : connectedVertices) {
					if (std::abs(uvs[vi].x - uv.x) <= weldDistance &&
						std::abs(uvs[vi].y - uv.y) <= weldDistance) {
						avgPos = DirectX::XMVectorAdd(avgPos, DirectX::XMLoadFloat3(&vertices[vi]));
						totalCount++;
					}
				}

				if (totalCount == 0)
					return;

				avgPos = DirectX::XMVectorScale(avgPos, 1.0f / totalCount);
				DirectX::XMVECTOR original = DirectX::XMLoadFloat3(&pos);
				DirectX::XMVECTOR smoothed = DirectX::XMVectorLerp(original, avgPos, a_strength);
				DirectX::XMStoreFloat3(&tempVertices[i], smoothed);
			});

			vertices = tempVertices;
#ifdef GEOMETRY_TEST
			PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), true, false);
#endif // GEOMETRY_TEST
		}

		logger::debug("{} : {} vertex smooth done", __func__, vertices.size());
		return;
	}
}
