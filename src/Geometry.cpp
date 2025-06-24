#include "Geometry.h"

namespace Mus {
#define GEOMETRY_TEST

	GeometryData::GeometryData(RE::BSGeometry* a_geo)
	{
		GetGeometryData(a_geo);
	}

	bool GeometryData::GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info)
	{
		if (!a_geo || a_geo->name.empty())
			return false;

		RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape();
		RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return false;

		RE::BSGraphics::VertexDesc desc = a_geo->GetGeometryRuntimeData().vertexDesc;
		info.hasVertices = dynamicTriShape ? true : desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
		info.hasUVs = desc.HasFlag(RE::BSGraphics::Vertex::VF_UV);
		info.hasNormals = desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL);
		info.hasTangents = desc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT);
		info.hasBitangents = info.hasVertices && info.hasNormals && info.hasTangents;
		return true;
	}
	bool GeometryData::GetGeometryData(RE::BSGeometry* a_geo)
	{
		if (!a_geo || a_geo->name.empty())
			return false;

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + a_geo->name.c_str(), false, false);
#endif // GEOMETRY_TEST

		GeometryInfo info;
		if (!GetGeometryInfo(a_geo, info))
			return false;

		RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape();
		RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return false;

		if (!info.hasVertices || !info.hasUVs)
			return false;

		std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(a_geo);
		if (!skinPartition)
			return false;

		RE::BSGraphics::VertexDesc desc = a_geo->GetGeometryRuntimeData().vertexDesc;

		logger::debug("{} {} : get geometry data...", __func__, a_geo->name.c_str());

		vertexCount = vertexCount ? vertexCount : skinPartition->vertexCount;
		std::size_t beforeVertexCount = vertices.size();
		std::size_t beforeUVCount = uvs.size();
		std::size_t beforeNormalCount = normals.size();
		std::size_t beforeTangentCount = tangents.size();
		std::size_t beforeBitangentCount = bitangents.size();
		vertices.resize(beforeVertexCount + vertexCount);
		uvs.resize(beforeUVCount + vertexCount);
		if (info.hasNormals)
			normals.resize(beforeNormalCount + vertexCount);
		if (info.hasTangents)
			tangents.resize(beforeTangentCount + vertexCount);
		if (info.hasBitangents)
			bitangents.resize(beforeBitangentCount + vertexCount);

		std::uint32_t vertexSize = desc.GetSize();
		std::uint8_t* vertexBlock = skinPartition->partitions[0].buffData->rawVertexData;
		DirectX::XMVECTOR* dynamicVertex = dynamicTriShape ? reinterpret_cast<DirectX::XMVECTOR*>(dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData) : nullptr;
		for (std::uint32_t i = 0; i < vertexCount; i++) {
			std::uint8_t* block = &vertexBlock[i * vertexSize];
			std::uint32_t vi = beforeVertexCount + i;
			std::uint32_t ui = beforeUVCount + i;
			std::uint32_t ni = beforeNormalCount + i;
			std::uint32_t ti = beforeTangentCount + i;
			std::uint32_t bi = beforeBitangentCount + i;
			if (info.hasVertices)
			{
				if (dynamicTriShape)
					DirectX::XMStoreFloat3(&vertices[vi], dynamicVertex[i]);
				else
					vertices[vi] = *reinterpret_cast<DirectX::XMFLOAT3*>(block);
				block += 12;

				if (info.hasBitangents)
					bitangents[bi].x = *reinterpret_cast<float*>(block);
				block += 4;
			}

			if (info.hasUVs)
			{
				uvs[ui].x = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block));
				uvs[ui].y = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block + 2));
				block += 4;
			}

			if (info.hasNormals)
			{
				normals[ni].x = static_cast<float>(*block) / 255.0f;
				normals[ni].y = static_cast<float>(*(block + 1)) / 255.0f;
				normals[ni].z = static_cast<float>(*(block + 2)) / 255.0f;
				block += 3;

				if (info.hasBitangents)
					bitangents[bi].y = static_cast<float>(*block);
				block += 1;

				if (info.hasTangents)
				{
					tangents[ti].x = static_cast<float>(*block) / 255.0f;
					tangents[ti].y = static_cast<float>(*(block + 1)) / 255.0f;
					tangents[ti].z = static_cast<float>(*(block + 2)) / 255.0f;
					block += 3;

					if (info.hasBitangents)
						bitangents[bi].z = static_cast<float>(*block);
				}
			}
		}

		std::size_t indexOffset = 0;
		for (auto& partition : skinPartition->partitions)
		{
			indexOffset += partition.triangles * 3;
		}
		std::size_t beforeIndices = indices.size();
		indices.resize(beforeIndices + indexOffset);
		indexOffset = 0;
		for (auto& partition : skinPartition->partitions)
		{
			for (std::uint32_t t = 0; t < partition.triangles * 3; t++)
			{
				indices[beforeIndices + indexOffset + t] = beforeVertexCount + partition.triList[t];
			}
			indexOffset += partition.triangles * 3;
		}

		ObjectInfo newObjInfo;
		newObjInfo.info = info;
		newObjInfo.vertexStart = beforeVertexCount;
		newObjInfo.vertexEnd = vertices.size();
		newObjInfo.uvStart = beforeUVCount;
		newObjInfo.uvEnd = uvs.size();
		newObjInfo.normalStart = beforeNormalCount;
		newObjInfo.normalEnd = normals.size();
		newObjInfo.tangentStart = beforeTangentCount;
		newObjInfo.tangentEnd = tangents.size();
		newObjInfo.bitangentStart = beforeBitangentCount;
		newObjInfo.bitangentEnd = bitangents.size();
		newObjInfo.indicesStart = beforeIndices;
		newObjInfo.indicesEnd = indices.size();
		geometries.push_back(std::make_pair(std::string(a_geo->name.c_str()), newObjInfo));

		mainInfo.hasVertices |= info.hasVertices;
		mainInfo.hasUVs |= info.hasUVs;
		mainInfo.hasNormals |= info.hasNormals;
		mainInfo.hasTangents |= info.hasTangents;
		mainInfo.hasBitangents |= info.hasBitangents;

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + a_geo->name.c_str(), true, false);
#endif // GEOMETRY_TEST

		logger::info("{} {} : get geometry data {} => vertices {} / uvs {} / normals {} / tangents {} / tris {}", __func__, a_geo->name.c_str(), geometries.size(),
					 newObjInfo.vertexCount(), newObjInfo.uvCount(), newObjInfo.normalCount(), newObjInfo.tangentCount(), newObjInfo.indicesCount() / 3);
		return true;
	}

	void GeometryData::UpdateMap() {
		if (vertices.empty() || indices.empty())
			return;
		vertexMap.clear();
		positionMap.clear();
		faceNormals.clear();
		faceTangents.clear();

		size_t triCount = indices.size() / 3;
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
			DirectX::XMFLOAT3 dp1, dp2;
			DirectX::XMStoreFloat3(&dp1, DirectX::XMVectorSubtract(v1, v0));
			DirectX::XMStoreFloat3(&dp2, DirectX::XMVectorSubtract(v2, v0));

			DirectX::XMFLOAT2 duv1 = { uv1.x - uv0.x, uv1.y - uv0.y };
			DirectX::XMFLOAT2 duv2 = { uv2.x - uv0.x, uv2.y - uv0.y };

			float r = duv1.x * duv2.y - duv2.x * duv1.y;
			r = (fabs(r) < floatPrecision) ? 1.0f : 1.0f / r;

			DirectX::XMVECTOR tangentVec = DirectX::XMVectorScale(
				DirectX::XMVectorSubtract(
					DirectX::XMVectorScale(DirectX::XMLoadFloat3(&dp1), duv2.y),
					DirectX::XMVectorScale(DirectX::XMLoadFloat3(&dp2), duv1.y)
				), r);

			DirectX::XMVECTOR bitangentVec = DirectX::XMVectorScale(
				DirectX::XMVectorSubtract(
					DirectX::XMVectorScale(DirectX::XMLoadFloat3(&dp2), duv1.x),
					DirectX::XMVectorScale(DirectX::XMLoadFloat3(&dp1), duv2.x)
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
#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(indices.size()), true, false);
#endif // GEOMETRY_TEST
	}

	void GeometryData::RecalculateNormals(float a_smooth)
	{
		if (!mainInfo.hasVertices || vertices.empty() || indices.empty())
			return;

		if (!mainInfo.hasNormals)
		{
			mainInfo.hasNormals = true;
			for (auto& geo : geometries) {
				geo.second.info.hasNormals = true;
				geo.second.normalStart = geo.second.vertexStart;
				geo.second.normalEnd = geo.second.vertexEnd;
			}
			normals.resize(vertices.size());
		}
		else if (vertices.size() != normals.size())
			return;

		if (!mainInfo.hasTangents)
		{
			mainInfo.hasTangents = true;
			for (auto& geo : geometries) {
				geo.second.info.hasTangents = true;
				geo.second.tangentStart = geo.second.vertexStart;
				geo.second.tangentEnd = geo.second.vertexEnd;
			}
			tangents.resize(vertices.size());
		}
		else if (vertices.size() != tangents.size())
			return;

		if (!mainInfo.hasBitangents)
		{
			mainInfo.hasBitangents = true;
			for (auto& geo : geometries) {
				geo.second.info.hasBitangents = true;
				geo.second.bitangentStart = geo.second.vertexStart;
				geo.second.bitangentEnd = geo.second.vertexEnd;
			}
			bitangents.resize(vertices.size());
		}
		else if (vertices.size() != bitangents.size())
			return;

		logger::debug("{} : normals {} re-calculate...", __func__, normals.size());
#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(normals.size()), false, false);
#endif // GEOMETRY_TEST
		const float smoothCos = cosf(DirectX::XMConvertToRadians(a_smooth));

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
			for (size_t fi : exactFaceIndices) {
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

			for (size_t vi : posIt->second) {
				for (size_t fi : vertexToFaceMap[vi]) {
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

			DirectX::XMVECTOR n = DirectX::XMVector3Normalize(nSum);
			DirectX::XMVECTOR t = DirectX::XMVector3Length(tSum).m128_f32[0] > floatPrecision ? DirectX::XMVector3Normalize(tSum) : DirectX::XMVectorZero();
			DirectX::XMVECTOR b = DirectX::XMVector3Length(bSum).m128_f32[0] > floatPrecision ? DirectX::XMVector3Normalize(bSum) : DirectX::XMVectorZero();

			if (!DirectX::XMVector3Equal(t, DirectX::XMVectorZero())) {
				t = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t)))));
				DirectX::XMVECTOR cross = DirectX::XMVector3Cross(n, t);
				float handedness = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(cross, b)) < 0.0f) ? -1.0f : 1.0f;
				b = DirectX::XMVectorScale(cross, handedness);
			}

			DirectX::XMStoreFloat3(&normals[i], n);
			if (!DirectX::XMVector3Equal(t, DirectX::XMVectorZero()))
				DirectX::XMStoreFloat3(&tangents[i], t);
			if (!DirectX::XMVector3Equal(b, DirectX::XMVectorZero()))
				DirectX::XMStoreFloat3(&bitangents[i], b);
		});

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(normals.size()), true, false);
#endif // GEOMETRY_TEST
		logger::debug("{} : normals {} re-calculated", __func__, normals.size());
		return;
	}

	void GeometryData::Subdivision(std::uint32_t a_subCount)
	{
		if (a_subCount == 0)
			return;

		logger::debug("{} : {} subdivition({})...", __func__, vertices.size(), a_subCount);

		for (std::uint32_t doSubdivision = 0; doSubdivision < a_subCount; doSubdivision++)
		{
#ifdef GEOMETRY_TEST
			PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), false, false);
#endif // GEOMETRY_TEST

			struct LocalDate {
				std::string geoName;
				GeometryInfo geoInfo;
				std::vector<DirectX::XMFLOAT3> vertices;
				std::vector<DirectX::XMFLOAT2> uvs;
				std::vector<DirectX::XMFLOAT3> normals;
				std::vector<DirectX::XMFLOAT3> tangents;
				std::vector<DirectX::XMFLOAT3> bitangents;
				std::vector<std::uint32_t> indices;
			};
			std::vector<LocalDate> subdividedDatas;
			for (auto& geometry : geometries)
			{
				LocalDate data(
					geometry.first,
					geometry.second.info,

					std::vector<DirectX::XMFLOAT3>(vertices.begin() + geometry.second.vertexStart, vertices.begin() + geometry.second.vertexEnd),
					std::vector<DirectX::XMFLOAT2>(uvs.begin() + geometry.second.uvStart, uvs.begin() + geometry.second.uvEnd),
					std::vector<DirectX::XMFLOAT3>(normals.begin() + geometry.second.normalStart, normals.begin() + geometry.second.normalEnd),
					std::vector<DirectX::XMFLOAT3>(tangents.begin() + geometry.second.tangentStart, tangents.begin() + geometry.second.tangentEnd),
					std::vector<DirectX::XMFLOAT3>(bitangents.begin() + geometry.second.bitangentStart, bitangents.begin() + geometry.second.bitangentEnd),
					std::vector<std::uint32_t>(indices.begin() + geometry.second.indicesStart, indices.begin() + geometry.second.indicesEnd)
				);

				std::unordered_map<uint64_t, uint32_t> midpointMap;
				auto getMidpointIndex = [&](uint32_t i0, uint32_t i1) -> uint32_t {
					auto createKey = [](uint32_t i0, uint32_t i1) -> uint64_t {
						uint32_t a = (std::min)(i0, i1);
						uint32_t b = (std::max)(i0, i1);
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

					uint32_t index = data.vertices.size();

					if (geometry.second.info.hasVertices)
					{
						const auto& v0 = data.vertices[i0];
						const auto& v1 = data.vertices[i1];
						DirectX::XMFLOAT3 midVertex = {
							(v0.x + v1.x) * 0.5f,
							(v0.y + v1.y) * 0.5f,
							(v0.z + v1.z) * 0.5f
						};
						data.vertices.push_back(midVertex);
					}

					if (geometry.second.info.hasUVs)
					{
						const auto& u0 = data.uvs[i0];
						const auto& u1 = data.uvs[i1];
						DirectX::XMFLOAT2 midUV = {
							(u0.x + u1.x) * 0.5f,
							(u0.y + u1.y) * 0.5f
						};
						data.uvs.push_back(midUV);
					}

					if (geometry.second.info.hasNormals)
					{
						const auto& n0 = data.normals[i0];
						const auto& n1 = data.normals[i1];
						DirectX::XMFLOAT3 midNormal = {
							(n0.x + n1.x) * 0.5f,
							(n0.y + n1.y) * 0.5f,
							(n0.z + n1.z) * 0.5f
						};
						midNormal = normalize(midNormal);
						data.normals.push_back(midNormal);
					}

					if (geometry.second.info.hasTangents)
					{
						const auto& t0 = data.tangents[i0];
						const auto& t1 = data.tangents[i1];
						DirectX::XMFLOAT3 midTangent = {
							(t0.x + t1.x) * 0.5f,
							(t0.y + t1.y) * 0.5f,
							(t0.z + t1.z) * 0.5f
						};
						midTangent = normalize(midTangent);
						data.tangents.push_back(midTangent);
					}

					if (geometry.second.info.hasBitangents)
					{
						const auto& b0 = data.bitangents[i0];
						const auto& b1 = data.bitangents[i1];
						DirectX::XMFLOAT3 midBitTangent = {
							(b0.x + b1.x) * 0.5f,
							(b0.y + b1.y) * 0.5f,
							(b0.z + b1.z) * 0.5f
						};
						midBitTangent = normalize(midBitTangent);
						data.bitangents.push_back(midBitTangent);
					}

					midpointMap[key] = index;
					return index;
				};

				auto oldIndices = data.indices;
				data.indices.resize(data.indices.size() * 4);
				for (std::size_t i = 0; i < oldIndices.size() / 3; i++)
				{
					std::size_t offset = i * 3;
					uint32_t v0 = oldIndices[offset + 0] - geometry.second.vertexStart;
					uint32_t v1 = oldIndices[offset + 1] - geometry.second.vertexStart;
					uint32_t v2 = oldIndices[offset + 2] - geometry.second.vertexStart;

					uint32_t m01 = getMidpointIndex(v0, v1);
					uint32_t m12 = getMidpointIndex(v1, v2);
					uint32_t m20 = getMidpointIndex(v2, v0);

					std::size_t triOffset = offset * 4;
					data.indices[triOffset + 0] = v0 + geometry.second.vertexStart;
					data.indices[triOffset + 1] = m01 + geometry.second.vertexStart;
					data.indices[triOffset + 2] = m20 + geometry.second.vertexStart;

					data.indices[triOffset + 3] = v1 + geometry.second.vertexStart;
					data.indices[triOffset + 4] = m12 + geometry.second.vertexStart;
					data.indices[triOffset + 5] = m01 + geometry.second.vertexStart;

					data.indices[triOffset + 6] = v2 + geometry.second.vertexStart;
					data.indices[triOffset + 7] = m20 + geometry.second.vertexStart;
					data.indices[triOffset + 8] = m12 + geometry.second.vertexStart;

					data.indices[triOffset + 9] = m01 + geometry.second.vertexStart;
					data.indices[triOffset + 10] = m12 + geometry.second.vertexStart;
					data.indices[triOffset + 11] = m20 + geometry.second.vertexStart;
				}
				subdividedDatas.push_back(data);
			}

			LocalDate totalData;
			geometries.clear();
			for (auto& subdividedData : subdividedDatas) {
				ObjectInfo newObjInfo;
				newObjInfo.info = subdividedData.geoInfo;
				newObjInfo.vertexStart = totalData.vertices.size();
				newObjInfo.uvStart = totalData.uvs.size();
				newObjInfo.normalStart = totalData.normals.size();
				newObjInfo.tangentStart = totalData.tangents.size();
				newObjInfo.bitangentStart = totalData.bitangents.size();
				newObjInfo.indicesStart = totalData.indices.size();

				totalData.vertices.insert(totalData.vertices.end(), subdividedData.vertices.begin(), subdividedData.vertices.end());
				totalData.uvs.insert(totalData.uvs.end(), subdividedData.uvs.begin(), subdividedData.uvs.end());
				totalData.normals.insert(totalData.normals.end(), subdividedData.normals.begin(), subdividedData.normals.end());
				totalData.tangents.insert(totalData.tangents.end(), subdividedData.tangents.begin(), subdividedData.tangents.end());
				totalData.bitangents.insert(totalData.bitangents.end(), subdividedData.bitangents.begin(), subdividedData.bitangents.end());
				totalData.indices.insert(totalData.indices.end(), subdividedData.indices.begin(), subdividedData.indices.end());

				newObjInfo.vertexEnd = totalData.vertices.size();
				newObjInfo.uvEnd = totalData.uvs.size();
				newObjInfo.normalEnd = totalData.normals.size();
				newObjInfo.tangentEnd = totalData.tangents.size();
				newObjInfo.bitangentEnd = totalData.bitangents.size();
				newObjInfo.indicesEnd = totalData.indices.size();

				geometries.push_back(std::make_pair(subdividedData.geoName, newObjInfo));
			}
			vertices = concurrency::concurrent_vector<DirectX::XMFLOAT3>(totalData.vertices.begin(), totalData.vertices.end());
			uvs = concurrency::concurrent_vector<DirectX::XMFLOAT2>(totalData.uvs.begin(), totalData.uvs.end());
			normals = concurrency::concurrent_vector<DirectX::XMFLOAT3>(totalData.normals.begin(), totalData.normals.end());
			tangents = concurrency::concurrent_vector<DirectX::XMFLOAT3>(totalData.tangents.begin(), totalData.tangents.end());
			bitangents = concurrency::concurrent_vector<DirectX::XMFLOAT3>(totalData.bitangents.begin(), totalData.bitangents.end());
			indices = concurrency::concurrent_vector<std::uint32_t>(totalData.indices.begin(), totalData.indices.end());

#ifdef GEOMETRY_TEST
			PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), true, false);
#endif // GEOMETRY_TEST
		}

		UpdateMap();

		logger::debug("{} : {} subdivition done", __func__, vertices.size());
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

				std::unordered_set<size_t> connectedVertices;
				for (size_t vi : posIt->second) {
					for (size_t fi : vertexToFaceMap[vi]) {
						const auto& fn = faceNormals[fi];
						if (fn.v0 == vi || fn.v1 == vi || fn.v2 == vi) {
							connectedVertices.insert(fn.v0);
							connectedVertices.insert(fn.v1);
							connectedVertices.insert(fn.v2);
						}
					}
				}

				for (size_t vi : connectedVertices) {
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

			UpdateMap();
		}
		logger::debug("{} : {} vertex smooth done", __func__, vertices.size());
		return;
	}
}
