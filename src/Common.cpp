#include "Common.h"

namespace Mus {
	GeometryData::GeometryData(RE::BSGeometry* a_geo)
	{
		GetGeometryData(a_geo);
	}

	void GeometryData::GetGeometryData(RE::BSGeometry* a_geo)
	{
		if (!a_geo || a_geo->name.empty())
			return;

		RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape();
		RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return;

		std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		this->desc = a_geo->GetGeometryRuntimeData().vertexDesc;

		this->hasVertices = this->desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
		this->hasUV = this->desc.HasFlag(RE::BSGraphics::Vertex::VF_UV);
		this->hasNormals = this->desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL);
		this->hasTangents = this->desc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT);
		this->hasBitangents = this->hasVertices && this->hasNormals && this->hasTangents;

		RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(a_geo);
		if (!skinPartition)
			return;

		logger::debug("{} {} : get geometry data...", __func__, a_geo->name.c_str());

		vertexCount = vertexCount ? vertexCount : skinPartition->vertexCount;
		if (this->hasVertices)
			this->vertices.resize(vertexCount);
		if (this->hasUV)
			this->uvs.resize(vertexCount);
		if (this->hasNormals)
			this->normals.resize(vertexCount);
		if (this->hasTangents)
			this->tangents.resize(vertexCount);
		if (this->hasBitangents)
			this->bitangents.resize(vertexCount);

		std::uint32_t vertexSize = this->desc.GetSize();
		std::uint8_t* vertexBlock = skinPartition->partitions[0].buffData->rawVertexData;

		for (std::uint32_t i = 0; i < vertexCount; i++)
		{
			std::uint8_t* block = &vertexBlock[i * vertexSize];
			if (this->hasVertices)
			{
				this->vertices[i] = *reinterpret_cast<DirectX::XMFLOAT3*>(block);
				block += 12;

				if (this->hasBitangents)
					this->bitangents[i].x = *reinterpret_cast<float*>(block);
				block += 4;
			}

			if (this->hasUV)
			{
				this->uvs[i].x = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block));
				this->uvs[i].y = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block + 2));
				block += 4;
			}

			if (this->hasNormals)
			{
				this->normals[i].x = static_cast<float>(*block) / 255.0f;
				this->normals[i].y = static_cast<float>(*(block + 1)) / 255.0f;
				this->normals[i].z = static_cast<float>(*(block + 2)) / 255.0f;
				block += 3;

				if (this->hasBitangents)
					this->bitangents[i].y = static_cast<float>(*block);
				block += 1;

				if (this->hasTangents)
				{
					this->tangents[i].x = static_cast<float>(*block) / 255.0f;
					this->tangents[i].y = static_cast<float>(*(block + 1)) / 255.0f;
					this->tangents[i].z = static_cast<float>(*(block + 2)) / 255.0f;
					block += 3;

					if (this->hasBitangents)
						this->bitangents[i].z = static_cast<float>(*block);
				}
			}
		}

		if (dynamicTriShape)
		{
			DirectX::XMVECTOR* vertex = reinterpret_cast<DirectX::XMVECTOR*>(dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData);
			this->hasVertices = true;
			this->vertices.resize(vertexCount);
			for (std::uint32_t i = 0; i < vertexCount; i++)
			{
				DirectX::XMStoreFloat3(&this->vertices[i], vertex[i]);
			}
		}

		for (std::uint32_t p = 0; p < skinPartition->numPartitions; p++)
		{
			for (std::uint32_t t = 0; t < skinPartition->partitions[p].triangles * 3; t++)
			{
				this->indices.push_back(skinPartition->partitions[p].triList[t]);
			}
		}

		this->UpdateVertexMapAndFaceNormals();

		logger::debug("{} {} : get geometry data done => vertices {} / uvs {} / normals {} / tangents {} / tris {}", __func__, a_geo->name.c_str(),
					  this->vertices.size(), this->uvs.size(), this->normals.size(), this->tangents.size(), this->indices.size() / 3);
		return;
	}

	void GeometryData::UpdateVertexMapAndFaceNormals() {
		if (this->vertices.empty() || this->indices.empty())
			return;
		this->vertexMap.clear();
		this->faceNormals.clear();
		for (std::size_t i = 0; i < this->indices.size(); i += 3)
		{
			std::uint32_t v0 = this->indices[i + 0];
			std::uint32_t v1 = this->indices[i + 1];
			std::uint32_t v2 = this->indices[i + 2];

			DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&this->vertices[v0]);
			DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&this->vertices[v1]);
			DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&this->vertices[v2]);

			DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(p1, p0), DirectX::XMVectorSubtract(p2, p0)));
			this->faceNormals.push_back({ v0, v1, v2, normal });

			this->vertexMap[this->vertices[v0]].push_back(this->faceNormals.size() - 1);
			this->vertexMap[this->vertices[v1]].push_back(this->faceNormals.size() - 1);
			this->vertexMap[this->vertices[v2]].push_back(this->faceNormals.size() - 1);
		}
	}

	void GeometryData::RecalculateNormals(float a_smooth)
	{
		if (!this->hasVertices || this->vertices.empty() || this->indices.empty())
			return;

		if (!this->hasNormals)
		{
			this->hasNormals = true;
			this->normals.resize(this->vertices.size());
		}
		else if (this->vertices.size() != this->normals.size())
			return;

		logger::debug("{} : normals {} re-calculate...", __func__, this->normals.size());
		const float smoothCos = cosf(DirectX::XMConvertToRadians(a_smooth));

		for (std::uint32_t i = 0; i < this->vertices.size(); ++i)
		{
			const DirectX::XMFLOAT3& pos = this->vertices[i];

			DirectX::XMVECTOR nSum = DirectX::XMVectorZero();
			DirectX::XMVECTOR nSelf = DirectX::XMVectorZero();

			std::vector<size_t> faceIndices = this->vertexMap[pos];
			for (size_t fi : faceIndices) {
				const GeometryData::FaceNormal& f = this->faceNormals[fi];
				if (f.v0 == i || f.v1 == i || f.v2 == i) {
					nSelf = f.normal;
					break;
				}
			}

			for (size_t fi : faceIndices) {
				const DirectX::XMVECTOR fn = this->faceNormals[fi].normal;
				float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(fn, nSelf));
				if (dot >= smoothCos) {
					nSum = DirectX::XMVectorAdd(nSum, fn);
				}
			}

			DirectX::XMStoreFloat3(&this->normals[i], DirectX::XMVector3Normalize(nSum));
		}
		logger::debug("{} : normals {} re-calculated", __func__, this->normals.size());
		return;
	}
	void GeometryData::Subdivision(std::uint32_t a_subCount)
	{
		if (a_subCount == 0)
			return;

		logger::debug("{} : {} subdivition({})...", __func__, this->vertices.size(), a_subCount);
		GeometryData subdividedData;
		if (subdividedData.hasVertices = this->hasVertices; subdividedData.hasVertices)
			subdividedData.vertices = this->vertices;
		if (subdividedData.hasUV = this->hasUV; subdividedData.hasUV)
			subdividedData.uvs = this->uvs;
		if (subdividedData.hasNormals = this->hasNormals; subdividedData.hasNormals)
			subdividedData.normals = this->normals;
		if (subdividedData.hasTangents = this->hasTangents; subdividedData.hasTangents)
			subdividedData.tangents = this->tangents;
		if (subdividedData.hasBitangents = this->hasBitangents; subdividedData.hasBitangents)
			subdividedData.bitangents = this->bitangents;

		std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpointMap;
		auto getMidpointIndex = [&](uint32_t i0, uint32_t i1) -> uint32_t {
			auto key = std::minmax(i0, i1);
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

			if (this->hasVertices)
			{
				const auto& v0 = subdividedData.vertices[i0];
				const auto& v1 = subdividedData.vertices[i1];
				DirectX::XMFLOAT3 midVertex;
				midVertex = {
					(v0.x + v1.x) * 0.5f,
					(v0.y + v1.y) * 0.5f,
					(v0.z + v1.z) * 0.5f
				};
				subdividedData.vertices.push_back(midVertex);
			}

			if (this->hasUV)
			{
				const auto& u0 = subdividedData.uvs[i0];
				const auto& u1 = subdividedData.uvs[i1];
				DirectX::XMFLOAT2 midUV;
				midUV = {
					(u0.x + u1.x) * 0.5f,
					(u0.y + u1.y) * 0.5f
				};
				subdividedData.uvs.push_back(midUV);
			}

			if (this->hasNormals)
			{
				const auto& n0 = subdividedData.normals[i0];
				const auto& n1 = subdividedData.normals[i1];
				DirectX::XMFLOAT3 midNormal;
				midNormal = {
					(n0.x + n1.x) * 0.5f,
					(n0.y + n1.y) * 0.5f,
					(n0.z + n1.z) * 0.5f
				};
				normalize(midNormal);
				subdividedData.normals.push_back(midNormal);
			}

			if (this->hasTangents)
			{
				const auto& t0 = subdividedData.tangents[i0];
				const auto& t1 = subdividedData.tangents[i1];
				DirectX::XMFLOAT3 midTangent;
				midTangent = {
					(t0.x + t1.x) * 0.5f,
					(t0.y + t1.y) * 0.5f,
					(t0.z + t1.z) * 0.5f
				};
				normalize(midTangent);
				subdividedData.tangents.push_back(midTangent);
			}

			if (this->hasBitangents)
			{
				const auto& b0 = subdividedData.bitangents[i0];
				const auto& b1 = subdividedData.bitangents[i1];
				DirectX::XMFLOAT3 midBitTangent;
				midBitTangent = {
					(b0.x + b1.x) * 0.5f,
					(b0.y + b1.y) * 0.5f,
					(b0.z + b1.z) * 0.5f
				};
				normalize(midBitTangent);
				subdividedData.bitangents.push_back(midBitTangent);
			}

			midpointMap[key] = index;
			return index;
			};

		for (std::uint32_t i = 0; i < this->indices.size(); i += 3)
		{
			uint32_t v0 = this->indices[i + 0];
			uint32_t v1 = this->indices[i + 1];
			uint32_t v2 = this->indices[i + 2];

			uint32_t m01 = getMidpointIndex(v0, v1);
			uint32_t m12 = getMidpointIndex(v1, v2);
			uint32_t m20 = getMidpointIndex(v2, v0);

			subdividedData.indices.push_back(v0);
			subdividedData.indices.push_back(m01);
			subdividedData.indices.push_back(m20);

			subdividedData.indices.push_back(v1);
			subdividedData.indices.push_back(m12);
			subdividedData.indices.push_back(m01);

			subdividedData.indices.push_back(v2);
			subdividedData.indices.push_back(m20);
			subdividedData.indices.push_back(m12);

			subdividedData.indices.push_back(m01);
			subdividedData.indices.push_back(m12);
			subdividedData.indices.push_back(m20);
		}

		*this = subdividedData;
		Subdivision(--a_subCount);

		this->UpdateVertexMapAndFaceNormals();

		logger::debug("{} : {} subdivition done", __func__, this->vertices.size());
		return;
	}
	void GeometryData::VertexSmooth(float a_strength, std::uint32_t a_smoothCount)
	{
		if (!this->hasVertices || this->vertices.empty() || a_smoothCount == 0)
			return;
		logger::debug("{} : {} vertex smooth({})...", __func__, this->vertices.size(), a_smoothCount);

		for (size_t i = 0; i < this->vertices.size(); ++i)
		{
			const DirectX::XMFLOAT3& v = this->vertices[i];
			auto it = this->vertexMap.find(v);
			if (it == this->vertexMap.end())
				continue;

			DirectX::XMVECTOR avgPos = DirectX::XMVectorZero();
			int totalCount = 0;

			for (size_t faceIdx : it->second)
			{
				const GeometryData::FaceNormal& face = this->faceNormals[faceIdx];
				for (auto vi : { face.v0, face.v1, face.v2 })
				{
					DirectX::XMVECTOR vp = DirectX::XMLoadFloat3(&this->vertices[vi]);
					avgPos = DirectX::XMVectorAdd(avgPos, vp);
					totalCount++;
				}
			}

			if (totalCount == 0)
				continue;

			avgPos = DirectX::XMVectorScale(avgPos, 1.0f / totalCount);
			DirectX::XMVECTOR original = DirectX::XMLoadFloat3(&v);
			DirectX::XMVECTOR smoothed = DirectX::XMVectorLerp(original, avgPos, a_strength);

			XMStoreFloat3(&this->vertices[i], smoothed);
		}

		this->UpdateVertexMapAndFaceNormals();
		VertexSmooth(a_strength, --a_smoothCount);
		logger::debug("{} : {} vertex smooth done", __func__, this->vertices.size());
		return;
	}
}