#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
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

		std::sort(geometries.begin(), geometries.end(), [](const GeometriesInfo& a, const GeometriesInfo& b) {
            return a.objInfo.info.name < b.objInfo.info.name;
        });

		for (auto& geo : geometries)
		{
			logger::debug("{}::{} : get geometry data...", __func__, geo.objInfo.info.name);

			const std::size_t beforeVertexCount = vertices.size();
            const std::size_t beforeUVCount = uvs.size();
            const std::size_t beforeIndices = indices.size();

			const std::uint32_t vertexSize = geo.objInfo.info.desc.GetSize();
            const std::uint32_t triCount = geo.objInfo.indicesBlockData.size() / 3;

			vertices.resize(beforeVertexCount + geo.objInfo.info.vertexCount);
            uvs.resize(beforeUVCount + geo.objInfo.info.vertexCount);
            indices.resize(beforeIndices + geo.objInfo.indicesBlockData.size());

			for (std::size_t i = 0; i < geo.objInfo.info.vertexCount; i++) {
				std::uint8_t* block = &geo.objInfo.geometryBlockData[i * vertexSize];
                const std::size_t vi = beforeVertexCount + i;
                const std::size_t ui = beforeUVCount + i;

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
				}
				if (geo.objInfo.info.hasVertices)
				{
					vertices[vi] = *reinterpret_cast<DirectX::XMFLOAT3*>(block);
					block += 12;
					block += 4;
				}
				if (geo.objInfo.info.hasUVs)
				{
					uvs[ui].x = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block));
					uvs[ui].y = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block + 2));
					block += 4;
				}
			}

            for (std::uint32_t i = 0; i < triCount; i++)
            {
                const std::uint32_t offset = i * 3;
                indices[beforeIndices + offset + 0] = beforeVertexCount + geo.objInfo.indicesBlockData[offset + 0];
                indices[beforeIndices + offset + 1] = beforeVertexCount + geo.objInfo.indicesBlockData[offset + 1];
                indices[beforeIndices + offset + 2] = beforeVertexCount + geo.objInfo.indicesBlockData[offset + 2];
			}

			geo.objInfo.vertexStart = beforeVertexCount;
			geo.objInfo.vertexEnd = vertices.size();
			geo.objInfo.uvStart = beforeUVCount;
			geo.objInfo.uvEnd = uvs.size();
			geo.objInfo.indicesStart = beforeIndices;
			geo.objInfo.indicesEnd = indices.size();

			logger::info("{}::{} : get geometry data => vertices {} / uvs {} / tris {}", __func__, geo.objInfo.info.name.c_str(),
						 geo.objInfo.vertexCount(), geo.objInfo.uvCount(), geo.objInfo.indicesCount() / 3);
		}
		if (Config::GetSingleton().GetGeometryDataTime())
			PerformanceLog(std::string(__func__) + "::" + mainInfo.name, true, false);
	}

	void GeometryData::CreateVertexMap()
    {
        if (vertices.empty() || indices.empty())
            return;

        const std::size_t triCount = indices.size() / 3;
        const std::size_t vertCount = vertices.size();

        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + std::to_string(triCount), false, false);

        vertexToFaceMap.clear();
        vertexToFaceMap.resize(vertCount);
        std::vector<Edge> edges(indices.size());
        {
            std::vector<std::future<void>> processes;
            const std::size_t sub = std::max(1ull, std::min(triCount, currentProcessingThreads.load()->GetThreads() * 4));
            const std::size_t unit = (triCount + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, triCount);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, t, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        const std::size_t offset = i * 3;
                        const std::uint32_t i0 = indices[offset + 0];
                        const std::uint32_t i1 = indices[offset + 1];
                        const std::uint32_t i2 = indices[offset + 2];
                        edges[offset + 0] = {i0, i1};
                        edges[offset + 1] = {i1, i2};
                        edges[offset + 2] = {i0, i2};
                        vertexToFaceMap[i0].push_back(i);
                        vertexToFaceMap[i1].push_back(i);
                        vertexToFaceMap[i2].push_back(i);
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }
        }

        parallel_sort(edges, currentProcessingThreads.load());

        std::vector<std::uint8_t> isBoundaryVert(vertCount, 0);
        std::size_t i = 0;
        while (i < edges.size())
        {
            std::size_t count = 1;
            while (i + count < edges.size() && edges[i] == edges[i + count])
            {
                count++;
            }

            if (count == 1)
            {
                isBoundaryVert[edges[i].v0] = 1;
                isBoundaryVert[edges[i].v1] = 1;
            }
            i += count;
        }

        std::vector<PosEntry> pMap(vertCount);
        std::vector<PosEntry> pbMap;
        std::vector<std::vector<PosEntry>> tpbMap(currentProcessingThreads.load()->GetThreads());
        {
            std::vector<std::future<void>> processes;
            const std::size_t sub = std::max(1ull, std::min(vertCount, currentProcessingThreads.load()->GetThreads() * 4));
            const std::size_t unit = (vertCount + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, vertCount);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                    auto ti = currentProcessingThreads.load()->GetThreadIndex(GetCurrentThreadId());
                    for (std::size_t i = begin; i < end; i++)
                    {
                        pMap[i] = PosEntry(MakePositionKey(vertices[i]), i);
                        if (isBoundaryVert[i])
                            tpbMap[ti].emplace_back(MakeBoundaryPositionKey(vertices[i]), i);
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }
        }
        for (const auto& m : tpbMap)
        {
            pbMap.append_range(m);
        }
        parallel_sort(pMap, currentProcessingThreads.load());
        parallel_sort(pbMap, currentProcessingThreads.load());

        weldedVertices.clear();
        weldedVertices.resize(vertCount);
        auto ProcessGroups = [&](const std::vector<PosEntry>& entry, bool checkSameGeo) {
            if (entry.empty())
                return;
            std::size_t begin = 0;
            for (std::size_t end = 1; end <= entry.size(); end++)
            {
                if (end != entry.size() && entry[end].key == entry[begin].key)
                    continue;
                for (std::size_t i = begin; i < end; i++)
                {
                    const std::uint32_t v0 = entry[i].index;
                    for (std::size_t j = begin; j < end; j++)
                    {
                        const std::uint32_t v1 = entry[j].index;
                        if (checkSameGeo && IsSameGeometry(v0, v1))
                            continue;
                        weldedVertices[v0].push_back(v1);
                    }
                }
                begin = end;
            }
        };
        ProcessGroups(pMap, false);
        ProcessGroups(pbMap, true);

        {
            std::vector<std::future<void>> processes;
            const std::size_t size = weldedVertices.size();
            const std::size_t sub = std::max(1ull, std::min(size, currentProcessingThreads.load()->GetThreads() * 4));
            const std::size_t unit = (size + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, size);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        std::sort(weldedVertices[i].begin(), weldedVertices[i].end());
                        auto last = std::unique(weldedVertices[i].begin(), weldedVertices[i].end());
                        weldedVertices[i].erase(last, weldedVertices[i].end());
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }
        }

        logger::debug("{}::{} : map updated, vertices {} / uvs {} / tris {}", __func__,
                      mainInfo.name, vertices.size(), uvs.size(), triCount);

        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + std::to_string(triCount), true, false);
        
        CreateFaceData();
    }

	void GeometryData::CreateFaceData()
    {
		const std::size_t triCount = indices.size() / 3;
        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + std::to_string(triCount), false, false);

        {
            std::vector<std::future<void>> processes;
            const auto vertices_ = vertices;
            const std::size_t size = weldedVertices.size();
            const std::size_t sub = std::max(1ull, std::min(size, currentProcessingThreads.load()->GetThreads() * 4));
            const std::size_t unit = (size + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, size);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        DirectX::XMVECTOR pos = emptyVector;
                        for (const auto& vi : weldedVertices[i])
                        {
                            pos = DirectX::XMVectorAdd(pos, DirectX::XMLoadFloat3(&vertices_[vi]));
                        }
                        pos = DirectX::XMVectorScale(pos, 1.0f / weldedVertices[i].size());
                        DirectX::XMStoreFloat3(&vertices[i], pos);
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }
        }

        faceNormals.resize(triCount);
        faceTangents.resize(triCount);
        {
            std::vector<std::future<void>> processes;
            const std::size_t sub = std::max(1ull, std::min(triCount, currentProcessingThreads.load()->GetThreads() * 4));
            const std::size_t unit = (triCount + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, triCount);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
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
                        const DirectX::XMVECTOR normalVec = DirectX::XMVector4NormalizeEst(
                            DirectX::XMVector3Cross(
                                DirectX::XMVectorSubtract(v1, v0),
                                DirectX::XMVectorSubtract(v2, v0)));
                        DirectX::XMFLOAT3 normal;
                        DirectX::XMStoreFloat3(&normal, normalVec);
                        faceNormals[i] = {i0, i1, i2, normal};

                        // Tangent / Bitangent
                        const DirectX::XMVECTOR dp1 = DirectX::XMVectorSubtract(v1, v0);
                        const DirectX::XMVECTOR dp2 = DirectX::XMVectorSubtract(v2, v0);

                        const DirectX::XMFLOAT2 duv1 = {uv1.x - uv0.x, uv1.y - uv0.y};
                        const DirectX::XMFLOAT2 duv2 = {uv2.x - uv0.x, uv2.y - uv0.y};

                        float r = duv1.x * duv2.y - duv2.x * duv1.y;
                        r = (fabs(r) < floatPrecision) ? 1.0f : 1.0f / r;

                        const DirectX::XMVECTOR tangentVec = DirectX::XMVectorScale(
                            DirectX::XMVectorSubtract(
                                DirectX::XMVectorScale(dp1, duv2.y),
                                DirectX::XMVectorScale(dp2, duv1.y)),
                            r);

                        const DirectX::XMVECTOR bitangentVec = DirectX::XMVectorScale(
                            DirectX::XMVectorSubtract(
                                DirectX::XMVectorScale(dp2, duv1.x),
                                DirectX::XMVectorScale(dp1, duv2.x)),
                            r);

                        DirectX::XMFLOAT3 tangent, bitangent;
                        DirectX::XMStoreFloat3(&tangent, tangentVec);
                        DirectX::XMStoreFloat3(&bitangent, bitangentVec);
                        faceTangents[i] = {i0, i1, i2, tangent, bitangent};
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }
        }

        logger::debug("{}::{} : map data updated, vertices {} / uvs {} / tris {}", __func__,
                      mainInfo.name, vertices.size(), uvs.size(), triCount);

        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + std::to_string(triCount), true, false);
    }

	void GeometryData::RecalculateNormals(float a_smoothDegree)
	{
        if (vertices.empty() || indices.empty() || vertexToFaceMap.empty() || a_smoothDegree < floatPrecision)
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
        const bool allowInvertNormalSmooth = Config::GetSingleton().GetAllowInvertNormalSmooth();

		std::vector<std::future<void>> processes;
        const std::size_t sub = std::max(1ull, std::min(vertices.size(), currentProcessingThreads.load()->GetThreads() * 4));
		const std::size_t unit = (vertices.size() + sub - 1) / sub;
        for (std::size_t t = 0; t < sub; t++) {
			const std::size_t begin = t * unit;
			const std::size_t end = std::min(begin + unit, vertices.size());
            processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
				for (std::size_t i = begin; i < end; i++)
                {
                    DirectX::XMVECTOR nSelf = emptyVector;
                    if (vertexToFaceMap[i].empty())
                        continue;
                    for (const auto& fi : vertexToFaceMap[i])
                    {
                        const auto& fn = faceNormals[fi];
                        nSelf = DirectX::XMVectorAdd(nSelf, DirectX::XMLoadFloat3(&fn.normal));
                    }
                    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(nSelf)) < floatPrecision)
                        continue;
                    nSelf = DirectX::XMVector3Normalize(nSelf);

                    DirectX::XMVECTOR nSum = emptyVector;
                    DirectX::XMVECTOR tSum = emptyVector;
                    DirectX::XMVECTOR bSum = emptyVector;

					for (const auto& link : weldedVertices[i])
                    {
                        for (const auto& fi : vertexToFaceMap[link])
                        {
                            const auto& fn = faceNormals[fi];
                            DirectX::XMVECTOR fnVec = DirectX::XMLoadFloat3(&fn.normal);
                            float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(fnVec, nSelf));
                            if (allowInvertNormalSmooth)
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

					if (DirectX::XMVector3Equal(nSum, emptyVector))
						continue;

					DirectX::XMVECTOR t = DirectX::XMVectorGetX(DirectX::XMVector3LengthEst(tSum)) > floatPrecision ? DirectX::XMVector3NormalizeEst(tSum) : emptyVector;
                    DirectX::XMVECTOR b = DirectX::XMVectorGetX(DirectX::XMVector3LengthEst(bSum)) > floatPrecision ? DirectX::XMVector3NormalizeEst(bSum) : emptyVector;
					const DirectX::XMVECTOR n = DirectX::XMVector3NormalizeEst(nSum);

					if (!DirectX::XMVector3Equal(t, emptyVector)) {
						t = DirectX::XMVector3NormalizeEst(DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t)))));
						b = DirectX::XMVector3NormalizeEst(DirectX::XMVector3Cross(n, t));
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

	void GeometryData::Subdivision(std::uint32_t a_subCount, std::uint32_t a_triThreshold, float a_strength, std::uint32_t a_smoothCount)
    {
        if (a_subCount == 0)
            return;

        logger::debug("{} : {} subdivition({})...", __func__, vertices.size(), a_subCount);

        std::vector<std::uint32_t> orgTriCount(geometries.size());
        for (std::size_t gi = 0; gi < geometries.size(); gi++)
        {
            orgTriCount[gi] = geometries[gi].objInfo.indicesCount() / 3;
        }

        const std::size_t threadsCount = currentProcessingThreads.load()->GetThreads();
        auto doSubdivision = [&]() {
            std::vector<LocalDate> subdividedDatas(geometries.size());
            std::vector<std::uint32_t> beforeVertexStart(geometries.size());

            // divide geo data
            for (std::size_t gi = 0; gi < geometries.size(); gi++)
            {
                const auto& geometry = geometries[gi];
                auto& data = subdividedDatas[gi];

                beforeVertexStart[gi] = geometry.objInfo.vertexStart;

                data.geometry = geometry.geometry;
                data.objInfo.info = geometry.objInfo.info;
                data.vertices.assign(vertices.begin() + geometry.objInfo.vertexStart, vertices.begin() + geometry.objInfo.vertexEnd);
                data.uvs.assign(uvs.begin() + geometry.objInfo.uvStart, uvs.begin() + geometry.objInfo.uvEnd);
                data.indices.assign(indices.begin() + geometry.objInfo.indicesStart, indices.begin() + geometry.objInfo.indicesEnd);
            }

            // fix indices
            {
                std::vector<std::future<void>> processes;
                for (std::size_t gi = 0; gi < geometries.size(); gi++)
                {
                    const auto& data = subdividedDatas[gi];
                    const std::size_t triCount = data.indices.size() / 3;
                    const std::size_t sub = std::max(1ull, std::min(triCount, threadsCount * 4));
                    const std::size_t unit = (triCount + sub - 1) / sub;
                    for (std::size_t t = 0; t < sub; t++)
                    {
                        const std::size_t begin = t * unit;
                        const std::size_t end = std::min(begin + unit, triCount);
                        processes.push_back(currentProcessingThreads.load()->submitAsync([&, gi, begin, end]() {
                            const auto& geometry = geometries[gi];
                            auto& data = subdividedDatas[gi];
                            for (std::size_t i = begin; i < end; i++)
                            {
                                const std::size_t offset = i * 3;
                                data.indices[offset + 0] -= geometry.objInfo.vertexStart;
                                data.indices[offset + 1] -= geometry.objInfo.vertexStart;
                                data.indices[offset + 2] -= geometry.objInfo.vertexStart;
                            }
                        }));
                    }
                }
                for (auto& process : processes)
                {
                    process.get();
                }
            }

            // clear old data
            vertices.clear();
            uvs.clear();
            indices.clear();

            // create tris
            {
                std::vector<std::future<void>> processes;
                for (std::size_t gi = 0; gi < subdividedDatas.size(); gi++)
                {
                    if (orgTriCount[gi] / 3 > a_triThreshold)
                        continue;
                    processes.push_back(currentProcessingThreads.load()->submitAsync([&, gi]() {
                        auto& data = subdividedDatas[gi];
                        data.vertices.reserve(data.vertices.size() + data.indices.size() / 3);
                        data.uvs.reserve(data.uvs.size() + data.indices.size() / 3);
                        data.localCreatedEdges.reserve(data.indices.size());
                        std::unordered_map<std::size_t, std::uint32_t> midpointMap;
                        auto getMidpointIndex = [&](const std::uint32_t i0, const std::uint32_t i1) -> std::uint32_t {
                            const auto key = Edge(i0, i1)();
                            if (auto it = midpointMap.find(key); it != midpointMap.end())
                                return it->second;

                            const std::uint32_t index = data.vertices.size();

                            const auto& v0 = data.vertices[i0];
                            const auto& v1 = data.vertices[i1];
                            const DirectX::XMFLOAT3 midVertex = {
                                (v0.x + v1.x) * 0.5f,
                                (v0.y + v1.y) * 0.5f,
                                (v0.z + v1.z) * 0.5f};
                            data.vertices.push_back(midVertex);

                            const auto& u0 = data.uvs[i0];
                            const auto& u1 = data.uvs[i1];
                            const DirectX::XMFLOAT2 midUV = {
                                (u0.x + u1.x) * 0.5f,
                                (u0.y + u1.y) * 0.5f};
                            data.uvs.push_back(midUV);

                            midpointMap[key] = index;
                            data.localCreatedEdges.push_back({i0, i1, index});
                            return index;
                        };

                        const std::size_t oldIndicesCount = data.indices.size();
                        auto oldIndices = data.indices;
                        data.indices.resize(oldIndicesCount * 4);
                        for (std::size_t i = 0; i < oldIndicesCount / 3; i++)
                        {
                            const std::size_t offset = i * 3;
                            const std::uint32_t v0 = oldIndices[offset + 0];
                            const std::uint32_t v1 = oldIndices[offset + 1];
                            const std::uint32_t v2 = oldIndices[offset + 2];

                            const std::uint32_t m01 = getMidpointIndex(v0, v1);
                            const std::uint32_t m12 = getMidpointIndex(v1, v2);
                            const std::uint32_t m20 = getMidpointIndex(v0, v2);

                            const std::size_t triOffset = offset * 4;
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
                    }));
                }
                for (auto& process : processes)
                {
                    process.get();
                }
            }

            // add vertex datas
            for (std::size_t gi = 0; gi < subdividedDatas.size(); gi++)
            {
                auto& data = subdividedDatas[gi];
                data.objInfo.vertexStart = vertices.size();
                data.objInfo.uvStart = uvs.size();

                vertices.append_range(data.vertices);
                uvs.append_range(data.uvs);

                data.objInfo.vertexEnd = vertices.size();
                data.objInfo.uvEnd = uvs.size();
            }

            // fix new indices
            {
                std::vector<std::future<void>> processes;
                for (std::size_t gi = 0; gi < subdividedDatas.size(); gi++)
                {
                    const auto& data = subdividedDatas[gi];
                    const std::size_t triCount = data.indices.size() / 3;
                    const std::size_t sub = std::max(1ull, std::min(triCount, threadsCount * 4));
                    const std::size_t unit = (triCount + sub - 1) / sub;
                    for (std::size_t t = 0; t < sub; t++)
                    {
                        const std::size_t begin = t * unit;
                        const std::size_t end = std::min(begin + unit, triCount);
                        processes.push_back(currentProcessingThreads.load()->submitAsync([&, gi, begin, end]() {
                            auto& data = subdividedDatas[gi];
                            for (std::size_t i = begin; i < end; i++)
                            {
                                const std::size_t offset = i * 3;
                                data.indices[offset + 0] += data.objInfo.vertexStart;
                                data.indices[offset + 1] += data.objInfo.vertexStart;
                                data.indices[offset + 2] += data.objInfo.vertexStart;
                            }
                        }));
                    }
                }
                for (auto& process : processes)
                {
                    process.get();
                }
            }

            // add indices and done
            for (std::size_t gi = 0; gi < subdividedDatas.size(); gi++)
            {
                auto& data = subdividedDatas[gi];
                data.objInfo.indicesStart = indices.size();
                indices.append_range(data.indices);
                data.objInfo.indicesEnd = indices.size();

                GeometriesInfo newGeoInfo = {
                    .geometry = data.geometry,
                    .objInfo = data.objInfo};
                geometries[gi] = std::move(newGeoInfo);
            }

            // fix original linked vertices data
            auto linkedVertices_ = std::move(weldedVertices);
            weldedVertices.resize(vertices.size());
            {
                std::size_t gi = 0;
                const std::size_t bvsSize = beforeVertexStart.size();
                for (std::size_t i = 0; i < linkedVertices_.size(); i++)
                {
                    while (gi < bvsSize - 1 && i >= beforeVertexStart[gi + 1])
                    {
                        gi++;
                    }
                    for (auto& vi : linkedVertices_[i])
                    {
                        std::size_t giAlt = 0;
                        while (giAlt < bvsSize - 1 && vi >= beforeVertexStart[giAlt + 1])
                        {
                            giAlt++;
                        }
                        vi = vi - beforeVertexStart[giAlt] + geometries[giAlt].objInfo.vertexStart;
                    }
                    const std::uint32_t ni = i - beforeVertexStart[gi] + geometries[gi].objInfo.vertexStart;
                    weldedVertices[ni] = std::move(linkedVertices_[i]);
                }
            }

            // create new edges map
            std::vector<EdgeMid> newEdges;
            newEdges.reserve(indices.size());
            for (std::size_t gi = 0; gi < subdividedDatas.size(); gi++)
            {
                const auto& data = subdividedDatas[gi];
                const std::uint32_t vertexStart = geometries[gi].objInfo.vertexStart;
                for (const auto& edge : data.localCreatedEdges)
                {
                    newEdges.push_back({edge.v0 + vertexStart, edge.v1 + vertexStart, edge.mv + vertexStart});
                }
            }
            parallel_sort(newEdges, currentProcessingThreads.load());

            const std::size_t newEdgesCount = newEdges.size();
            std::vector<PosEntry> pMap(newEdges.size());
            {
                std::vector<std::future<void>> processes;
                const std::size_t sub = std::max(1ull, std::min(newEdgesCount, currentProcessingThreads.load()->GetThreads() * 4));
                const std::size_t unit = (newEdgesCount + sub - 1) / sub;
                for (std::size_t t = 0; t < sub; t++)
                {
                    const std::size_t begin = t * unit;
                    const std::size_t end = std::min(begin + unit, newEdgesCount);
                    processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                        auto ti = currentProcessingThreads.load()->GetThreadIndex(GetCurrentThreadId());
                        for (std::size_t i = begin; i < end; i++)
                        {
                            const auto vi = newEdges[i].mv;
                            pMap[i] = PosEntry(MakePositionKey(vertices[vi]), vi);
                        }
                    }));
                }
                for (auto& process : processes)
                {
                    process.get();
                }
            }
            parallel_sort(pMap, currentProcessingThreads.load());
            {
                std::size_t begin = 0;
                for (std::size_t end = 1; end <= pMap.size(); end++)
                {
                    if (end != pMap.size() && pMap[end].key == pMap[begin].key)
                        continue;
                    for (std::size_t i = begin; i < end; i++)
                    {
                        const std::uint32_t v0 = pMap[i].index;
                        for (std::size_t j = begin; j < end; j++)
                        {
                            const std::uint32_t v1 = pMap[j].index;
                            weldedVertices[v0].push_back(v1);
                        }
                    }
                    begin = end;
                }
            }
            {
                std::vector<std::future<void>> processes;
                const std::size_t sub = std::max(1ull, std::min(newEdgesCount, currentProcessingThreads.load()->GetThreads() * 4));
                const std::size_t unit = (newEdgesCount + sub - 1) / sub;
                for (std::size_t t = 0; t < sub; t++)
                {
                    const std::size_t begin = t * unit;
                    const std::size_t end = std::min(begin + unit, newEdgesCount);
                    processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                        auto ti = currentProcessingThreads.load()->GetThreadIndex(GetCurrentThreadId());
                        for (std::size_t i = begin; i < end; i++)
                        {
                            const auto vi = newEdges[i].mv;
                            std::sort(weldedVertices[vi].begin(), weldedVertices[vi].end());
                            auto last = std::unique(weldedVertices[vi].begin(), weldedVertices[vi].end());
                            weldedVertices[vi].erase(last, weldedVertices[vi].end());
                        }
                    }));
                }
                for (auto& process : processes)
                {
                    process.get();
                }
            }
        };

        auto createVertexToFaceMap = [&](){
            vertexToFaceMap.clear();
            vertexToFaceMap.resize(vertices.size());
            const std::size_t triCount = indices.size() / 3;
            std::vector<std::future<void>> processes;
            {
                const std::size_t sub = std::max(1ull, std::min(triCount, currentProcessingThreads.load()->GetThreads() * 4));
                const std::size_t unit = (triCount + sub - 1) / sub;
                for (std::size_t t = 0; t < sub; t++)
                {
                    const std::size_t begin = t * unit;
                    const std::size_t end = std::min(begin + unit, triCount);
                    processes.push_back(currentProcessingThreads.load()->submitAsync([&, t, begin, end]() {
                        for (std::size_t i = begin; i < end; i++)
                        {
                            const std::size_t offset = i * 3;
                            const std::uint32_t i0 = indices[offset + 0];
                            const std::uint32_t i1 = indices[offset + 1];
                            const std::uint32_t i2 = indices[offset + 2];
                            vertexToFaceMap[i0].push_back(i);
                            vertexToFaceMap[i1].push_back(i);
                            vertexToFaceMap[i2].push_back(i);
                        }
                    }));
                }
            }
            for (auto& process : processes)
            {
                process.get();
            }
        };

        for (std::uint32_t i = 1; i <= a_subCount; i++)
        {
            const std::string subID = std::to_string(vertices.size());
            if (Config::GetSingleton().GetGeometryDataTime())
                PerformanceLog(std::string(__func__) + "::" + subID + "::" + std::to_string(i), false, false);
            doSubdivision();
            createVertexToFaceMap();
            if (Config::GetSingleton().GetGeometryDataTime())
                PerformanceLog(std::string(__func__) + "::" + subID + "::" + std::to_string(i), true, false);
            //CreateVertexMap(1.0f / i);
            CreateFaceData();
            VertexSmooth(a_strength, a_smoothCount);
        }

		logger::debug("{} : {} subdivition done", __func__, vertices.size());
		return;
	}

	void GeometryData::VertexSmooth(float a_strength, std::uint32_t a_smoothCount)
	{
		if (vertices.empty() || a_smoothCount == 0 || a_strength <= floatPrecision)
			return;

		logger::debug("{} : {} vertex smooth({})...", __func__, vertices.size(), a_smoothCount);

        const float deflate = std::clamp(a_strength, 0.0f, 1.0f);
        const float inflate = -(deflate + 0.03f);

        std::vector<std::future<void>> processes;
        auto doSmooth = [&](float weight) {
            auto tempVertices = vertices;
            processes.clear();
            const std::size_t sub = std::max(1ull, std::min(vertices.size(), currentProcessingThreads.load()->GetThreads() * 8));
            const std::size_t unit = (vertices.size() + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, vertices.size());
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end, weight]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        std::unordered_set<std::uint32_t> connectedVertices;
                        for (const auto& link : weldedVertices[i])
                        {
                            for (const auto& fi : vertexToFaceMap[link])
                            {
                                const auto& fn = faceNormals[fi];
                                if (fn.v0 != link)
                                    connectedVertices.insert(fn.v0);
                                if (fn.v1 != link)
                                    connectedVertices.insert(fn.v1);
                                if (fn.v2 != link)
                                    connectedVertices.insert(fn.v2);
                            }
                        }
                        if (connectedVertices.empty())
                            continue;

                        DirectX::XMVECTOR sumPos = emptyVector;
                        for (const auto& cvi : connectedVertices)
                        {
                            sumPos = DirectX::XMVectorAdd(sumPos, DirectX::XMLoadFloat3(&tempVertices[cvi]));
                        }

                        const DirectX::XMVECTOR avgPos = DirectX::XMVectorScale(sumPos, 1.0f / connectedVertices.size());
                        const DirectX::XMVECTOR original = DirectX::XMLoadFloat3(&tempVertices[i]);
                        const DirectX::XMVECTOR delta = DirectX::XMVectorSubtract(avgPos, original);
                        const DirectX::XMVECTOR smoothed = DirectX::XMVectorAdd(original, DirectX::XMVectorScale(delta, weight));

                        DirectX::XMStoreFloat3(&vertices[i], smoothed);
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }
        };

        for (std::uint32_t i = 1; i <= a_smoothCount; i++)
        {
            if (Config::GetSingleton().GetGeometryDataTime())
                PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()) + "::" + std::to_string(i), false, false);
            doSmooth(deflate);
            doSmooth(inflate);
            if (Config::GetSingleton().GetGeometryDataTime())
                PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()) + "::" + std::to_string(i), true, false);
            CreateFaceData();
        }

		logger::debug("{} : {} vertex smooth done", __func__, vertices.size());
		return;
	}

	void GeometryData::VertexSmoothByAngle(float a_smoothThreshold1, float a_smoothThreshold2, std::uint32_t a_smoothCount)
	{
		if (vertices.empty() || a_smoothCount == 0)
			return;

		logger::debug("{} : {} vertex smooth by angle({})...", __func__, vertices.size(), a_smoothCount);
        if (a_smoothThreshold1 > a_smoothThreshold2)
            std::swap(a_smoothThreshold1, a_smoothThreshold2);
        const float maxCos = std::cosf(a_smoothThreshold1 * toRadian);
        const float minCos = std::cosf(a_smoothThreshold2 * toRadian);

        auto doSmooth = [&]() {
            const auto tempVertices = vertices;
            std::vector<std::future<void>> processes;
            const std::size_t sub = std::max(1ull, std::min(vertices.size(), currentProcessingThreads.load()->GetThreads() * 8));
            const std::size_t unit = (vertices.size() + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, vertices.size());
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        DirectX::XMVECTOR nSelf = emptyVector;
                        for (const auto& link : weldedVertices[i])
                        {
                            for (const auto& fi : vertexToFaceMap[link])
                            {
                                nSelf = DirectX::XMVectorAdd(nSelf, DirectX::XMLoadFloat3(&faceNormals[fi].normal));
                            }
                        }
                        if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(nSelf)) < floatPrecision)
                            continue;
                        nSelf = DirectX::XMVector3NormalizeEst(nSelf);

                        std::unordered_set<std::uint32_t> connectedVertices;
                        float dotTotal = 0.0f;
                        std::uint32_t dotCount = 0;
                        for (const auto& link : weldedVertices[i])
                        {
                            for (const auto& fi : vertexToFaceMap[link])
                            {
                                const auto& fn = faceNormals[fi];
                                const float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(nSelf, DirectX::XMLoadFloat3(&fn.normal)));
                                if (dot > maxCos)
                                    continue;
                                if (fn.v0 != link)
                                    connectedVertices.insert(fn.v0);
                                if (fn.v1 != link)
                                    connectedVertices.insert(fn.v1);
                                if (fn.v2 != link)
                                    connectedVertices.insert(fn.v2);
                                dotTotal += dot;
                                dotCount++;
                            }
                        }
                        if (connectedVertices.empty())
                            continue;

                        DirectX::XMVECTOR avgPos = emptyVector;
                        const float avgDot = dotTotal / dotCount;
                        const float strength = SmoothStepRange(avgDot, maxCos, minCos);
                        for (const auto& cvi : connectedVertices)
                        {
                            avgPos = DirectX::XMVectorAdd(avgPos, DirectX::XMLoadFloat3(&tempVertices[cvi]));
                        }
                        avgPos = DirectX::XMVectorScale(avgPos, 1.0f / connectedVertices.size());
                        const DirectX::XMVECTOR original = DirectX::XMLoadFloat3(&tempVertices[i]);
                        const DirectX::XMVECTOR smoothed = DirectX::XMVectorLerp(original, avgPos, strength);
                        DirectX::XMStoreFloat3(&vertices[i], smoothed);
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }
        };
		for (std::uint32_t i = 1; i <= a_smoothCount; i++)
		{
			if (Config::GetSingleton().GetGeometryDataTime())
                PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()) + "::" + std::to_string(i), false, false);
            doSmooth();
			if (Config::GetSingleton().GetGeometryDataTime())
                PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()) + "::" + std::to_string(i), true, false);
            CreateFaceData();
        }
	}

	void GeometryData::CreateGeometryHash()
    {
        for (auto& geo : geometries)
        {
            geo.hash = XXH3_64bits(normals.data() + geo.objInfo.normalStart, geo.objInfo.normalCount() * sizeof(DirectX::XMFLOAT3));
        }
    }

    void GeometryData::GeometryProcessing()
    {
        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + mainInfo.name, false, false);
        CreateVertexMap();
        Subdivision(Config::GetSingleton().GetSubdivision(), Config::GetSingleton().GetSubdivisionTriThreshold(),
                            Config::GetSingleton().GetSubdivisionVertexSmoothStrength(), Config::GetSingleton().GetSubdivisionVertexSmooth());
        VertexSmoothByAngle(Config::GetSingleton().GetVertexSmoothByAngleThreshold1(), Config::GetSingleton().GetVertexSmoothByAngleThreshold2(), Config::GetSingleton().GetVertexSmoothByAngle());
        VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
        RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());
        CreateGeometryHash();
        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + mainInfo.name, true, false);
    }

    void GeometryData::ApplyNormals()
    {
        for (auto& geo : geometries)
        {
            if (!geo.geometry)
                continue;

            const RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(geo.geometry);
            if (!skinPartition)
                continue;
            RE::NiPointer<RE::NiSkinPartition> newSkinPartition = nullptr;
            {
                RE::NiPointer<RE::NiObject> niObj;
                skinPartition->CreateDeepCopy(niObj);
                newSkinPartition = RE::NiPointer(netimmerse_cast<RE::NiSkinPartition*>(niObj.get()));
            }
            if (!newSkinPartition)
                continue;

            auto dynamicTriShape = geo.geometry->AsDynamicTriShape();
            if (dynamicTriShape)
                dynamicTriShape->GetDynamicTrishapeRuntimeData().lock.Lock();

            auto oldVertexDesc = skinPartition->partitions[0].buffData->vertexDesc;
            auto oldVertexSize = oldVertexDesc.GetSize();
            bool hasVertices = oldVertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_VERTEX);
            bool hasUVs = oldVertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_UV);
            bool hasNormals = oldVertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_NORMAL);
            bool hasTangents = oldVertexDesc.HasFlag(RE::BSGraphics::Vertex::Flags::VF_TANGENT);
            auto newVertexDesc = oldVertexDesc;
            newVertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_NORMAL);
            newVertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_TANGENT);

            std::vector<std::uint8_t> newVertexBlocks(geo.objInfo.vertexCount() * newVertexDesc.GetSize());
            auto round_v = [](float num) {
                return (num > 0.0) ? floor(num + 0.5) : ceil(num - 0.5);
            };
            for (std::size_t i = 0; i < geo.objInfo.vertexCount(); i++)
            {
                std::uint8_t* srcBlock = &skinPartition->partitions[0].buffData->rawVertexData[i * oldVertexSize];
                std::uint8_t* dstBlock = &newVertexBlocks[i * newVertexDesc.GetSize()];
                std::uint32_t currentOffset = 0;
                const std::uint32_t iOffset = i + geo.objInfo.vertexStart;
                if (hasVertices)
                {
                    std::memcpy(dstBlock, srcBlock, 12); // X,Y,Z

                    srcBlock += 12;
                    dstBlock += 12;
                    currentOffset += 12;

                    std::memcpy(dstBlock, reinterpret_cast<std::uint8_t*>(&bitangents[iOffset].x), 4);
                    srcBlock += 4;
                    dstBlock += 4;
                    currentOffset += 4;
                }
                if (hasUVs)
                {
                    std::memcpy(dstBlock, srcBlock, 4); // X,Y
                    srcBlock += 4;
                    dstBlock += 4;
                    currentOffset += 4;
                }

                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((normals[iOffset].x + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;
                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((normals[iOffset].y + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;
                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((normals[iOffset].z + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;
                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((bitangents[iOffset].y + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;

                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((tangents[iOffset].x + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;
                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((tangents[iOffset].y + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;
                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((tangents[iOffset].z + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;
                *reinterpret_cast<std::int8_t*>(dstBlock) = static_cast<std::uint8_t>(round_v(((bitangents[iOffset].z + 1.0f) / 2.0f) * 255.0f));
                dstBlock += 1;

                if (hasNormals)
                {
                    srcBlock += 4;
                    currentOffset += 4;
                    if (hasTangents)
                    {
                        srcBlock += 4;
                        currentOffset += 4;
                    }
                }

                if (currentOffset < oldVertexSize)
                    std::memcpy(dstBlock, srcBlock, oldVertexSize - currentOffset);
            }
            for (auto& partition : newSkinPartition->partitions)
            {
                if (!partition.buffData)
                    continue;
                partition.vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_NORMAL);
                partition.vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_TANGENT);
                partition.buffData->vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_NORMAL);
                partition.buffData->vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_TANGENT);
            }
            geo.geometry->GetGeometryRuntimeData().vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_NORMAL);
            geo.geometry->GetGeometryRuntimeData().vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_TANGENT);
            if (dynamicTriShape)
                dynamicTriShape->GetDynamicTrishapeRuntimeData().lock.Unlock();

            for (auto& partition : newSkinPartition->partitions)
            {
                if (oldVertexSize != newVertexDesc.GetSize())
                {
                    partition.buffData->rawVertexData = reinterpret_cast<std::uint8_t*>(RE::malloc(newVertexBlocks.size()));
                }
                memcpy(partition.buffData->rawVertexData, newVertexBlocks.data(), newVertexBlocks.size());
            }

            auto context = Shader::ShaderManager::GetSingleton().GetContext();
            Shader::ShaderLocker sl(context);
            {
                Shader::ShaderLockGuard slg(sl);
                auto skinInstance = geo.geometry->GetGeometryRuntimeData().skinInstance;
#ifdef SKYRIM_CROSS_VR
                EnterCriticalSection(&skinInstance->lock);
#endif

                auto rendererData = geo.geometry->GetGeometryRuntimeData().rendererData;
                if (rendererData)
                {
                    rendererData->vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_NORMAL);
                    rendererData->vertexDesc.SetFlag(RE::BSGraphics::Vertex::Flags::VF_TANGENT);
                }

                D3D11_BUFFER_DESC desc;
                reinterpret_cast<ID3D11Buffer*>(newSkinPartition->partitions[0].buffData->vertexBuffer)->GetDesc(&desc);
                if (desc.Usage != D3D11_USAGE_DYNAMIC)
                {
                    D3D11_BUFFER_DESC newDesc = desc;
                    newDesc.Usage = D3D11_USAGE_DYNAMIC;
                    newDesc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
                    newDesc.ByteWidth = newVertexBlocks.size();

                    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer = nullptr;
                    D3D11_SUBRESOURCE_DATA data;
                    data.pSysMem = newSkinPartition->partitions[0].buffData->rawVertexData;
                    data.SysMemPitch = newVertexBlocks.size();
                    data.SysMemSlicePitch = 0;

                    if (SUCCEEDED(Shader::ShaderManager::GetSingleton().GetDevice()->CreateBuffer(&newDesc, &data, &buffer)))
                    {
                        for (auto& partition : newSkinPartition->partitions)
                        {
                            auto oldVertexBuffer = reinterpret_cast<ID3D11Buffer*>(partition.buffData->vertexBuffer);
                            partition.buffData->vertexBuffer = reinterpret_cast<RE::ID3D11Buffer*>(buffer.Get());
                            buffer->AddRef();
                            oldVertexBuffer->Release();
                        }
                        if (rendererData)
                        {
                            rendererData->vertexBuffer = newSkinPartition->partitions[0].buffData->vertexBuffer;
                            buffer->AddRef();
                        }
                        desc = newDesc;
                    }
                }
                if (desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
                {
                    D3D11_MAPPED_SUBRESOURCE mappedResource;
                    auto vertexBuffer = reinterpret_cast<ID3D11Buffer*>(newSkinPartition->partitions[0].buffData->vertexBuffer);
                    if (context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) == S_OK)
                    {
                        memcpy(mappedResource.pData, newSkinPartition->partitions[0].buffData->rawVertexData, newVertexBlocks.size());
                        context->Unmap(vertexBuffer, 0);
                    }

                    for (std::uint32_t p = 1; p < newSkinPartition->partitions.size(); p++)
                    {
                        if (newSkinPartition->partitions[p].buffData->vertexBuffer != newSkinPartition->partitions[0].buffData->vertexBuffer)
                        {
                            context->CopyResource(
                                reinterpret_cast<ID3D11Buffer*>(newSkinPartition->partitions[p].buffData->vertexBuffer),
                                vertexBuffer);
                        }
                    }
                    /*for (std::uint32_t p = 1; p < newSkinPartition->partitions.size(); p++)
                    {
                        if (newSkinPartition->partitions[p].buffData->vertexBuffer != newSkinPartition->partitions[0].buffData->vertexBuffer)
                        {
                            if (context->Map(reinterpret_cast<ID3D11Buffer*>(newSkinPartition->partitions[p].buffData->vertexBuffer), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) == S_OK)
                            {
                                memcpy(mappedResource.pData, newSkinPartition->partitions[p].buffData->rawVertexData, newVertexBlocks.size());
                                context->Unmap(reinterpret_cast<ID3D11Buffer*>(newSkinPartition->partitions[p].buffData->vertexBuffer), 0);
                            }
                        }
                    }*/
                }
                if (rendererData)
                {
                    if (oldVertexSize != newVertexDesc.GetSize())
                    {
                        rendererData->rawVertexData = reinterpret_cast<std::uint8_t*>(RE::malloc(newVertexBlocks.size()));
                    }
                    memcpy(rendererData->rawVertexData, newVertexBlocks.data(), newVertexBlocks.size());
                }
#ifdef SKYRIM_CROSS_VR
                LeaveCriticalSection(&skinInstance->lock);
#endif
                skinInstance->skinPartition = newSkinPartition;
            }
        }
    }

    bool GeometryData::PrintGeometry(const lString& filePath)
    {
        tinygltf::Model model;
        tinygltf::Asset asset;
        asset.generator = "DirectXExporter";
        asset.version = "2.0";
        model.asset = asset;
        tinygltf::Buffer buffer;

        std::size_t bufferOffset = 0;
        std::size_t indicesSize = indices.size() * sizeof(std::uint32_t);
        std::size_t indicesOffset = bufferOffset;
        bufferOffset += indicesSize;

        std::size_t verticesSize = vertices.size() * sizeof(DirectX::XMFLOAT3);
        std::size_t verticesOffset = bufferOffset;
        bufferOffset += verticesSize;

        std::size_t normalsSize = normals.size() * sizeof(DirectX::XMFLOAT3);
        std::size_t normalsOffset = bufferOffset;
        bufferOffset += normalsSize;

        std::size_t uvsSize = uvs.size() * sizeof(DirectX::XMFLOAT2);
        std::size_t uvsOffset = bufferOffset;
        bufferOffset += uvsSize;

        struct TangentV4
        {
            float x, y, z, w;
        };
        std::vector<TangentV4> tangentV4s;
        tangentV4s.reserve(tangents.size());

        for (size_t i = 0; i < tangents.size(); ++i)
        {
            DirectX::XMVECTOR N = XMLoadFloat3(&normals[i]);
            DirectX::XMVECTOR T = XMLoadFloat3(&tangents[i]);
            DirectX::XMVECTOR B = XMLoadFloat3(&bitangents[i]);

            DirectX::XMVECTOR cross = DirectX::XMVector3Cross(N, T);
            float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(cross, B));
            float w = (dot < 0.0f) ? -1.0f : 1.0f;
            tangentV4s.push_back({tangents[i].x, tangents[i].y, tangents[i].z, w});
        }

        std::size_t tangentsSize = tangentV4s.size() * sizeof(TangentV4);
        std::size_t tangentsOffset = bufferOffset;
        bufferOffset += tangentsSize;

        buffer.data.resize(bufferOffset);

        memcpy(buffer.data.data() + indicesOffset, indices.data(), indicesSize);
        memcpy(buffer.data.data() + verticesOffset, vertices.data(), verticesSize);
        memcpy(buffer.data.data() + normalsOffset, normals.data(), normalsSize);
        memcpy(buffer.data.data() + uvsOffset, uvs.data(), uvsSize);
        memcpy(buffer.data.data() + tangentsOffset, tangentV4s.data(), tangentsSize);

        model.buffers.push_back(buffer);

        auto addBufferView = [&](std::size_t offset, std::size_t length, int target) {
            tinygltf::BufferView bv;
            bv.buffer = 0;
            bv.byteOffset = offset;
            bv.byteLength = length;
            bv.target = target;
            model.bufferViews.push_back(bv);
            return (int)model.bufferViews.size() - 1;
        };

        int bvIndices = addBufferView(indicesOffset, indicesSize, TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);
        int bvPos = addBufferView(verticesOffset, verticesSize, TINYGLTF_TARGET_ARRAY_BUFFER);
        int bvNormal = addBufferView(normalsOffset, normalsSize, TINYGLTF_TARGET_ARRAY_BUFFER);
        int bvUV = addBufferView(uvsOffset, uvsSize, TINYGLTF_TARGET_ARRAY_BUFFER);
        int bvTangent = addBufferView(tangentsOffset, tangentsSize, TINYGLTF_TARGET_ARRAY_BUFFER);

        DirectX::XMFLOAT3 minPos = {FLT_MAX, FLT_MAX, FLT_MAX};
        DirectX::XMFLOAT3 maxPos = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (const auto& v : vertices)
        {
            minPos.x = std::min(minPos.x, v.x);
            minPos.y = std::min(minPos.y, v.y);
            minPos.z = std::min(minPos.z, v.z);
            maxPos.x = std::max(maxPos.x, v.x);
            maxPos.y = std::max(maxPos.y, v.y);
            maxPos.z = std::max(maxPos.z, v.z);
        }

        auto addAccessor = [&](int bufferView, int componentType, int count, int type,
                               const std::vector<double>& minVal = {}, const std::vector<double>& maxVal = {}) {
            tinygltf::Accessor acc;
            acc.bufferView = bufferView;
            acc.byteOffset = 0;
            acc.componentType = componentType;
            acc.count = count;
            acc.type = type;
            if (!minVal.empty())
                acc.minValues = minVal;
            if (!maxVal.empty())
                acc.maxValues = maxVal;
            model.accessors.push_back(acc);
            return (int)model.accessors.size() - 1;
        };

        int accIndices = addAccessor(bvIndices, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, (int)indices.size(), TINYGLTF_TYPE_SCALAR);
        int accPos = addAccessor(bvPos, TINYGLTF_COMPONENT_TYPE_FLOAT, (int)vertices.size(), TINYGLTF_TYPE_VEC3,
                                 {minPos.x, minPos.y, minPos.z}, {maxPos.x, maxPos.y, maxPos.z});
        int accNormal = addAccessor(bvNormal, TINYGLTF_COMPONENT_TYPE_FLOAT, (int)normals.size(), TINYGLTF_TYPE_VEC3);
        int accUV = addAccessor(bvUV, TINYGLTF_COMPONENT_TYPE_FLOAT, (int)uvs.size(), TINYGLTF_TYPE_VEC2);
        int accTangent = addAccessor(bvTangent, TINYGLTF_COMPONENT_TYPE_FLOAT, (int)tangentV4s.size(), TINYGLTF_TYPE_VEC4);

        tinygltf::Primitive primitive;
        primitive.attributes["POSITION"] = accPos;
        primitive.attributes["NORMAL"] = accNormal;
        primitive.attributes["TEXCOORD_0"] = accUV;
        primitive.attributes["TANGENT"] = accTangent;
        primitive.indices = accIndices;
        primitive.mode = TINYGLTF_MODE_TRIANGLES;

        tinygltf::Mesh mesh;
        mesh.primitives.push_back(primitive);
        model.meshes.push_back(mesh);

        tinygltf::Node node;
        node.mesh = 0;
        model.nodes.push_back(node);

        tinygltf::Scene scene;
        scene.nodes.push_back(0);
        model.scenes.push_back(scene);
        model.defaultScene = 0;

        tinygltf::TinyGLTF gltf;
        bool binary = filePath.ends_with(".glb");
        return gltf.WriteGltfSceneToFile(&model, filePath, false, false, true, binary);
    }
}
