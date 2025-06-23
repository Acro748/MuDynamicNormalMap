#include "Common.h"

namespace Mus {
#define GEOMETRY_TEST

	GeometryData::GeometryData(RE::BSGeometry* a_geo)
	{
		GetGeometryData(a_geo);
	}

	void GeometryData::GetGeometryData(RE::BSGeometry* a_geo)
	{
		if (!a_geo || a_geo->name.empty())
			return;

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + a_geo->name.c_str(), false, false);
#endif // GEOMETRY_TEST

		RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape();
		RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return;

		std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		desc = a_geo->GetGeometryRuntimeData().vertexDesc;

		hasVertices = desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
		hasUV = desc.HasFlag(RE::BSGraphics::Vertex::VF_UV);
		hasNormals = desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL);
		hasTangents = desc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT);
		hasBitangents = hasVertices && hasNormals && hasTangents;

		RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(a_geo);
		if (!skinPartition)
			return;

		logger::debug("{} {} : get geometry data...", __func__, a_geo->name.c_str());

		vertexCount = vertexCount ? vertexCount : skinPartition->vertexCount;
		if (hasVertices)
			vertices.resize(vertexCount);
		if (hasUV)
			uvs.resize(vertexCount);
		if (hasNormals)
			normals.resize(vertexCount);
		if (hasTangents)
			tangents.resize(vertexCount);
		if (hasBitangents)
			bitangents.resize(vertexCount);

		std::uint32_t vertexSize = desc.GetSize();
		std::uint8_t* vertexBlock = skinPartition->partitions[0].buffData->rawVertexData;

		for (std::uint32_t i = 0; i < vertexCount; i++) {
			std::uint8_t* block = &vertexBlock[i * vertexSize];
			if (hasVertices)
			{
				vertices[i] = *reinterpret_cast<DirectX::XMFLOAT3*>(block);
				block += 12;

				if (hasBitangents)
					bitangents[i].x = *reinterpret_cast<float*>(block);
				block += 4;
			}

			if (hasUV)
			{
				uvs[i].x = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block));
				uvs[i].y = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block + 2));
				block += 4;
			}

			if (hasNormals)
			{
				normals[i].x = static_cast<float>(*block) / 255.0f;
				normals[i].y = static_cast<float>(*(block + 1)) / 255.0f;
				normals[i].z = static_cast<float>(*(block + 2)) / 255.0f;
				block += 3;

				if (hasBitangents)
					bitangents[i].y = static_cast<float>(*block);
				block += 1;

				if (hasTangents)
				{
					tangents[i].x = static_cast<float>(*block) / 255.0f;
					tangents[i].y = static_cast<float>(*(block + 1)) / 255.0f;
					tangents[i].z = static_cast<float>(*(block + 2)) / 255.0f;
					block += 3;

					if (hasBitangents)
						bitangents[i].z = static_cast<float>(*block);
				}
			}
		}

		if (dynamicTriShape)
		{
			DirectX::XMVECTOR* vertex = reinterpret_cast<DirectX::XMVECTOR*>(dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData);
			hasVertices = true;
			vertices.resize(vertexCount);
			for (std::uint32_t i = 0; i < vertexCount; i++)
			{
				DirectX::XMStoreFloat3(&vertices[i], vertex[i]);
			}
		}

		std::size_t indexOffset = 0;
		for (auto& partition : skinPartition->partitions)
		{
			indexOffset += partition.triangles * 3;
		}
		indices.resize(indexOffset);
		indexOffset = 0;
		for (auto& partition : skinPartition->partitions)
		{
			for (std::uint32_t t = 0; t < partition.triangles * 3; t++)
			{
				indices[indexOffset + t] = partition.triList[t];
			}
			indexOffset += partition.triangles * 3;
		}

#ifdef GEOMETRY_TEST
		PerformanceLog(std::string(__func__) + "::" + a_geo->name.c_str(), true, false);
#endif // GEOMETRY_TEST

		logger::debug("{} {} : get geometry data done => vertices {} / uvs {} / normals {} / tangents {} / tris {}", __func__, a_geo->name.c_str(),
					  vertices.size(), uvs.size(), normals.size(), tangents.size(), indices.size() / 3);
		return;
	}

	void GeometryData::UpdateVertexMap_FaceNormals_FaceTangents() {
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
			r = (fabs(r) < 1e-6f) ? 1.0f : 1.0f / r;

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

			// Update both maps
			// VertexMap: for exact vertex matching (position + UV)
			vertexMap[{ p0, uv0 }].push_back(i);
			vertexMap[{ p1, uv1 }].push_back(i);
			vertexMap[{ p2, uv2 }].push_back(i);

			// PositionMap: for position-only matching (used for smoothing across UV seams)
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
		if (!hasVertices || vertices.empty() || indices.empty())
			return;

		if (!hasNormals)
		{
			hasNormals = true;
			normals.resize(vertices.size());
		}
		else if (vertices.size() != normals.size())
			return;

		if (!hasTangents)
		{
			hasTangents = true;
			tangents.resize(vertices.size());
		}
		else if (vertices.size() != tangents.size())
			return;

		if (!hasBitangents)
		{
			hasBitangents = true;
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

			// First, find faces that contain this exact vertex (position + UV)
			VertexKey vkey = { pos, uv };
			auto it = vertexMap.find(vkey);
			if (it == vertexMap.end())
				return;

			const auto& exactFaceIndices = it->second;

			// Find this vertex's self normal from faces that contain it
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

			// Now find all faces at the same position (including UV seams)
			PositionKey pkey = { pos };
			auto posIt = positionMap.find(pkey);
			if (posIt == positionMap.end())
				return;

			DirectX::XMVECTOR nSum = DirectX::XMVectorZero();
			DirectX::XMVECTOR tSum = DirectX::XMVectorZero();
			DirectX::XMVECTOR bSum = DirectX::XMVectorZero();

			// Collect normals from all vertices at the same position
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
			DirectX::XMVECTOR t = DirectX::XMVector3Length(tSum).m128_f32[0] > 1e-6f ? DirectX::XMVector3Normalize(tSum) : DirectX::XMVectorZero();
			DirectX::XMVECTOR b = DirectX::XMVector3Length(bSum).m128_f32[0] > 1e-6f ? DirectX::XMVector3Normalize(bSum) : DirectX::XMVectorZero();

			// Gram-Schmidt orthogonalization
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

			GeometryData subdividedData;
			if (subdividedData.hasVertices = hasVertices; subdividedData.hasVertices)
				subdividedData.vertices = vertices;
			if (subdividedData.hasUV = hasUV; subdividedData.hasUV)
				subdividedData.uvs = uvs;
			if (subdividedData.hasNormals = hasNormals; subdividedData.hasNormals)
				subdividedData.normals = normals;
			if (subdividedData.hasTangents = hasTangents; subdividedData.hasTangents)
				subdividedData.tangents = tangents;
			if (subdividedData.hasBitangents = hasBitangents; subdividedData.hasBitangents)
				subdividedData.bitangents = bitangents;

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

				uint32_t index = subdividedData.vertices.size();

				if (hasVertices)
				{
					const auto& v0 = subdividedData.vertices[i0];
					const auto& v1 = subdividedData.vertices[i1];
					DirectX::XMFLOAT3 midVertex = {
						(v0.x + v1.x) * 0.5f,
						(v0.y + v1.y) * 0.5f,
						(v0.z + v1.z) * 0.5f
					};
					subdividedData.vertices.push_back(midVertex);
				}

				if (hasUV)
				{
					const auto& u0 = subdividedData.uvs[i0];
					const auto& u1 = subdividedData.uvs[i1];
					DirectX::XMFLOAT2 midUV = {
						(u0.x + u1.x) * 0.5f,
						(u0.y + u1.y) * 0.5f
					};
					subdividedData.uvs.push_back(midUV);
				}

				if (hasNormals)
				{
					const auto& n0 = subdividedData.normals[i0];
					const auto& n1 = subdividedData.normals[i1];
					DirectX::XMFLOAT3 midNormal = {
						(n0.x + n1.x) * 0.5f,
						(n0.y + n1.y) * 0.5f,
						(n0.z + n1.z) * 0.5f
					};
					midNormal = normalize(midNormal);
					subdividedData.normals.push_back(midNormal);
				}

				if (hasTangents)
				{
					const auto& t0 = subdividedData.tangents[i0];
					const auto& t1 = subdividedData.tangents[i1];
					DirectX::XMFLOAT3 midTangent = {
						(t0.x + t1.x) * 0.5f,
						(t0.y + t1.y) * 0.5f,
						(t0.z + t1.z) * 0.5f
					};
					midTangent = normalize(midTangent);
					subdividedData.tangents.push_back(midTangent);
				}

				if (hasBitangents)
				{
					const auto& b0 = subdividedData.bitangents[i0];
					const auto& b1 = subdividedData.bitangents[i1];
					DirectX::XMFLOAT3 midBitTangent = {
						(b0.x + b1.x) * 0.5f,
						(b0.y + b1.y) * 0.5f,
						(b0.z + b1.z) * 0.5f
					};
					midBitTangent = normalize(midBitTangent);
					subdividedData.bitangents.push_back(midBitTangent);
				}

				midpointMap[key] = index;
				return index;
			};

			subdividedData.indices.resize(indices.size() * 4);
			for (std::size_t i = 0; i < indices.size() / 3; i++)
			{
				std::size_t offset = i * 3;
				uint32_t v0 = indices[offset + 0];
				uint32_t v1 = indices[offset + 1];
				uint32_t v2 = indices[offset + 2];

				uint32_t m01 = getMidpointIndex(v0, v1);
				uint32_t m12 = getMidpointIndex(v1, v2);
				uint32_t m20 = getMidpointIndex(v2, v0);

				std::size_t triOffset = offset * 4;
				subdividedData.indices[triOffset + 0] = v0;
				subdividedData.indices[triOffset + 1] = m01;
				subdividedData.indices[triOffset + 2] = m20;

				subdividedData.indices[triOffset + 3] = v1;
				subdividedData.indices[triOffset + 4] = m12;
				subdividedData.indices[triOffset + 5] = m01;

				subdividedData.indices[triOffset + 6] = v2;
				subdividedData.indices[triOffset + 7] = m20;
				subdividedData.indices[triOffset + 8] = m12;

				subdividedData.indices[triOffset + 9] = m01;
				subdividedData.indices[triOffset + 10] = m12;
				subdividedData.indices[triOffset + 11] = m20;
			}
#ifdef GEOMETRY_TEST
			PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), true, false);
#endif // GEOMETRY_TEST

			*this = subdividedData;
		}

		UpdateVertexMap_FaceNormals_FaceTangents();

		logger::debug("{} : {} subdivition done", __func__, vertices.size());
		return;
	}

	void GeometryData::VertexSmooth(float a_strength, std::uint32_t a_smoothCount)
	{
		if (!hasVertices || vertices.empty() || a_smoothCount == 0)
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

				std::set<size_t> connectedVertices;
				for (size_t vi : posIt->second) {
					for (size_t fi : vertexToFaceMap[vi]) {
						const auto& fn = faceNormals[fi];
						if (fn.v0 == vi || fn.v1 == vi || fn.v2 == vi) {
							// Add all vertices from this face
							connectedVertices.insert(fn.v0);
							connectedVertices.insert(fn.v1);
							connectedVertices.insert(fn.v2);
						}
					}
				}

				for (size_t vi : connectedVertices) {
					if (std::abs(uvs[vi].x - uv.x) <= floatPrecision &&
						std::abs(uvs[vi].y - uv.y) <= floatPrecision) {
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

			UpdateVertexMap_FaceNormals_FaceTangents();
		}
		logger::debug("{} : {} vertex smooth done", __func__, vertices.size());
		return;
	}
}