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

        vertexToFaceMap.resize(vertCount);
        std::vector<Edge> edges(indices.size());
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
                        edges[offset + 0] = {std::min(i0, i1), std::max(i0, i1)};
                        edges[offset + 1] = {std::min(i1, i2), std::max(i1, i2)};
                        edges[offset + 2] = {std::min(i2, i0), std::max(i2, i0)};
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
        std::vector < std::vector<PosEntry>> tpbMap;
        processes.clear();
        {
            const std::size_t sub = std::max(1ull, std::min(vertCount, currentProcessingThreads.load()->GetThreads() * 4));
            const std::size_t unit = (vertCount + sub - 1) / sub;
            tpbMap.resize(sub);
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, vertCount);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, t, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        pMap[i] = PosEntry(MakePositionKey(vertices[i]), i);
                        if (isBoundaryVert[i])
                            tpbMap[t].emplace_back(MakeBoundaryPositionKey(vertices[i]), i);
                    }
                }));
            }
        }
        for (auto& process : processes)
        {
            process.get();
        }
        for (auto& m : tpbMap)
        {
            pbMap.append_range(m);
        }

        {
            parallel_sort(pMap, currentProcessingThreads.load());
            parallel_sort(pbMap, currentProcessingThreads.load());
        }

        linkedVertices.resize(vertCount);
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
                    const std::uint32_t v1 = entry[i].index;
                    for (std::size_t j = begin; j < end; j++)
                    {
                        const std::uint32_t v2 = entry[j].index;
                        if (checkSameGeo)
                        {
                            if (IsSameGeometry(v1, v2))
                                continue;
                        }
                        linkedVertices[v1].push_back(v2);
                    }
                }
                begin = end;
            }
        };
        ProcessGroups(pMap, false);
        ProcessGroups(pbMap, true);

        processes.clear();
        {
            const std::size_t size = linkedVertices.size();
            const std::size_t sub = std::max(1ull, std::min(size, currentProcessingThreads.load()->GetThreads() * 4));
            const std::size_t unit = (size + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, size);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        std::sort(linkedVertices[i].begin(), linkedVertices[i].end());
                        auto last = std::unique(linkedVertices[i].begin(), linkedVertices[i].end());
                        linkedVertices[i].erase(last, linkedVertices[i].end());
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

        faceNormals.resize(triCount);
        faceTangents.resize(triCount);

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

        logger::debug("{}::{} : map data updated, vertices {} / uvs {} / tris {}", __func__,
                      mainInfo.name, vertices.size(), uvs.size(), triCount);

        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + std::to_string(triCount), true, false);
    }

	void GeometryData::RecalculateNormals(float a_smoothDegree)
	{
		if (vertices.empty() || indices.empty() || a_smoothDegree < floatPrecision)
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

					for (const auto& link : linkedVertices[i])
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

	void GeometryData::Subdivision(std::uint32_t a_subCount, std::uint32_t a_triThreshold)
    {
        if (a_subCount == 0)
            return;

        std::string subID = std::to_string(vertices.size());
        if (Config::GetSingleton().GetGeometryDataTime())
            PerformanceLog(std::string(__func__) + "::" + subID, false, false);

        logger::debug("{} : {} subdivition({})...", __func__, vertices.size(), a_subCount);

        struct LocalDate
        {
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
            LocalDate data = {
                .geometry = geometry.geometry,
                .geoName = geometry.objInfo.info.name,
                .geoInfo = geometry.objInfo.info};

            data.vertices.assign(vertices.begin() + geometry.objInfo.vertexStart, vertices.begin() + geometry.objInfo.vertexEnd);
            data.uvs.assign(uvs.begin() + geometry.objInfo.uvStart, uvs.begin() + geometry.objInfo.uvEnd);
            data.indices.assign(indices.begin() + geometry.objInfo.indicesStart, indices.begin() + geometry.objInfo.indicesEnd);

            std::size_t triCount = data.indices.size() / 3;
            std::vector<std::future<void>> processes;
            const std::size_t sub = std::max(1ull, std::min(triCount, currentProcessingThreads.load()->GetThreads()));
            const std::size_t unit = (triCount + sub - 1) / sub;
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, triCount);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        std::size_t offset = i * 3;
                        data.indices[offset + 0] -= geometry.objInfo.vertexStart;
                        data.indices[offset + 1] -= geometry.objInfo.vertexStart;
                        data.indices[offset + 2] -= geometry.objInfo.vertexStart;
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }

            data.objInfo = geometry.objInfo;

            subdividedDatas.push_back(data);
        }

        vertices.clear();
        uvs.clear();
        indices.clear();
        geometries.clear();

        std::vector<std::future<void>> processes;
        for (std::size_t i = 0; i < subdividedDatas.size(); i++)
        {
            if (subdividedDatas[i].indices.size() / 3 > a_triThreshold)
                continue;
            processes.push_back(currentProcessingThreads.load()->submitAsync([&, i]() {
                auto& data = subdividedDatas[i];
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
                            (v0.z + v1.z) * 0.5f};
                        data.vertices.push_back(midVertex);

                        const auto& u0 = data.uvs[i0];
                        const auto& u1 = data.uvs[i1];
                        DirectX::XMFLOAT2 midUV = {
                            (u0.x + u1.x) * 0.5f,
                            (u0.y + u1.y) * 0.5f};
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
            }));
        }
		for (auto& process : processes)
		{
			process.get();
		}
		for (std::size_t i = 0; i < subdividedDatas.size(); i++)
        {
            auto& data = subdividedDatas[i];
            data.objInfo.info = data.geoInfo;
            data.objInfo.vertexStart = vertices.size();
            data.objInfo.uvStart = uvs.size();

            vertices.append_range(data.vertices);
            uvs.append_range(data.uvs);

            data.objInfo.vertexEnd = vertices.size();
            data.objInfo.uvEnd = uvs.size();

            data.objInfo.indicesStart = indices.size();
            std::size_t triCount = data.indices.size() / 3;
            const std::size_t sub = std::max(1ull, std::min(triCount, currentProcessingThreads.load()->GetThreads()));
            const std::size_t unit = (triCount + sub - 1) / sub;
            processes.clear();
            for (std::size_t t = 0; t < sub; t++)
            {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, triCount);
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
                    for (std::size_t i = begin; i < end; i++)
                    {
                        std::size_t offset = i * 3;
                        data.indices[offset + 0] += data.objInfo.vertexStart;
                        data.indices[offset + 1] += data.objInfo.vertexStart;
                        data.indices[offset + 2] += data.objInfo.vertexStart;
                    }
                }));
            }
            for (auto& process : processes)
            {
                process.get();
            }

            indices.append_range(data.indices);
            data.objInfo.indicesEnd = indices.size();

            GeometriesInfo newGeoInfo = {
                .geometry = data.geometry,
                .objInfo = std::move(data.objInfo)
            };
            geometries.push_back(std::move(newGeoInfo));
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
            const std::size_t sub = std::max(1ull, std::min(vertices.size(), currentProcessingThreads.load()->GetThreads() * 8));
			const std::size_t unit = (vertices.size() + sub - 1) / sub;
			for (std::size_t t = 0; t < sub; t++) {
				const std::size_t begin = t * unit;
				const std::size_t end = std::min(begin + unit, vertices.size());
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++)
                    {
                        std::unordered_set<std::uint32_t> connectedVertices;
                        for (const auto& link : linkedVertices[i])
                        {
                            std::sort(vertexToFaceMap[link].begin(), vertexToFaceMap[link].end());
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

						DirectX::XMVECTOR avgPos = emptyVector;
						for (const auto& cvi : connectedVertices)
						{
							avgPos = DirectX::XMVectorAdd(avgPos, DirectX::XMLoadFloat3(&tempVertices[cvi]));
						}
						avgPos = DirectX::XMVectorScale(avgPos, 1.0f / connectedVertices.size());
                        const DirectX::XMVECTOR original = DirectX::XMLoadFloat3(&tempVertices[i]);
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
            
            CreateFaceData();
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

			const auto tempVertices = vertices;
			std::vector<std::future<void>> processes;
            const std::size_t sub = std::max(1ull, std::min(vertices.size(), currentProcessingThreads.load()->GetThreads() * 8));
			const std::size_t unit = (vertices.size() + sub - 1) / sub;
			for (std::size_t t = 0; t < sub; t++) {
				const std::size_t begin = t * unit;
				const std::size_t end = std::min(begin + unit, vertices.size());
                processes.push_back(currentProcessingThreads.load()->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++)
					{
                        DirectX::XMVECTOR nSelf = emptyVector;
                        for (const auto& link : linkedVertices[i])
                        {
                            for (const auto& fi : vertexToFaceMap[link])
                            {
                                const auto& fn = faceNormals[fi];
                                nSelf = DirectX::XMVectorAdd(nSelf, DirectX::XMLoadFloat3(&fn.normal));
                            }
                        }
                        if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(nSelf)) < floatPrecision)
                            continue;
                        nSelf = DirectX::XMVector3NormalizeEst(nSelf);

						std::unordered_set<std::uint32_t> connectedVertices;
                        float dotTotal = 0.0f;
                        std::uint32_t dotCount = 0;
                        for (const auto& link : linkedVertices[i])
                        {
                            for (const auto& fi : vertexToFaceMap[link])
                            {
                                const auto& fn = faceNormals[fi];
                                const float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(nSelf, DirectX::XMLoadFloat3(&fn.normal)));
                                if (dot > smoothCos1)
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
						const float strength = SmoothStepRange(avgDot, smoothCos1, smoothCos2);
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
			for (auto& process : processes) {
				process.get();
			}

			if (Config::GetSingleton().GetGeometryDataTime())
				PerformanceLog(std::string(__func__) + "::" + std::to_string(vertices.size()), true, false);
            
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
}
