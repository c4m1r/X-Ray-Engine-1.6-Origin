#pragma once
// Spatial acceleration for EScene::Render().
// Header-only (all methods inline) — no separate .cpp needed.
// Include after stdafx.h; requires Fvector/xr_vector/CCustomObject from precompiled chain.

#include "CustomObject.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

//=============================================================================
// Abstract interface — swap implementations without touching scene code
//=============================================================================
class ISpatialIndex
{
public:
    virtual ~ISpatialIndex() {}

    virtual void Clear()                                                = 0;
    virtual void Insert(CCustomObject* obj)                             = 0;
    virtual void Remove(CCustomObject* obj)                             = 0;

    // Returns candidate objects near cam within radius_sq.
    // Applies cell-level frustum culling via ::Render->ViewBase.
    // Does NOT call IsRender() — caller is responsible for final culling.
    virtual void Query(const Fvector& cam, float radius_sq,
                       xr_vector<CCustomObject*>& out)                  = 0;

    virtual const char* DebugName() const                               = 0;
};

//=============================================================================
// Uniform 2D grid — cells partitioned by X/Z world coordinates
//=============================================================================
class CObjectGridUniform final : public ISpatialIndex
{
public:
    explicit CObjectGridUniform(float cell_size = 64.f)
        : m_CellSize(cell_size)
        , m_InvCellSize(1.f / cell_size)
    {}

    const char* DebugName() const override { return "UniformGrid"; }

    inline void Clear() override
    {
        m_Grid.clear();
        m_ObjCells.clear();
    }

    inline void Insert(CCustomObject* obj) override
    {
        VERIFY(obj);
        if (m_ObjCells.count(obj)) return;
        xr_vector<SKey> keys;
        GetObjectCells(obj, keys);
        for (const SKey& k : keys)
            m_Grid[k].push_back(obj);
        m_ObjCells[obj] = std::move(keys);
    }

    inline void Remove(CCustomObject* obj) override
    {
        VERIFY(obj);
        auto oit = m_ObjCells.find(obj);
        if (oit == m_ObjCells.end()) return;
        for (const SKey& k : oit->second) {
            auto git = m_Grid.find(k);
            if (git == m_Grid.end()) continue;
            auto& cell = git->second;
            cell.erase(std::remove(cell.begin(), cell.end(), obj), cell.end());
        }
        m_ObjCells.erase(oit);
    }

    inline void Query(const Fvector& cam, float radius_sq,
                      xr_vector<CCustomObject*>& out) override
    {
        const float radius = sqrtf(radius_sq);
        const SKey  k0     = CellOf(cam.x - radius, cam.z - radius);
        const SKey  k1     = CellOf(cam.x + radius, cam.z + radius);

        // When the query box covers far more grid cells than there are objects,
        // iterating empty cells dominates cost (9M+ hash lookups for radius=100k).
        // Fall back to a direct object-list scan instead — O(N) with no false empties.
        const int64_t cell_span = (int64_t)(k1.x - k0.x + 1) * (k1.z - k0.z + 1);
        if (cell_span > (int64_t)m_ObjCells.size() * 4) {
            for (auto& kv : m_ObjCells) {
                CCustomObject* obj = kv.first;
                const Fvector& p = obj->FPosition;
                float dx = p.x - cam.x, dy = p.y - cam.y, dz = p.z - cam.z;
                if (dx*dx + dy*dy + dz*dz <= radius_sq)
                    out.push_back(obj);
            }
            return;
        }

        std::unordered_set<CCustomObject*> seen;
        seen.reserve(std::min(m_ObjCells.size(), size_t(2048)));

        for (int cx = k0.x; cx <= k1.x; ++cx) {
            for (int cz = k0.z; cz <= k1.z; ++cz) {
                // Skip empty cells first (cheap hash lookup)
                auto it = m_Grid.find({cx, cz});
                if (it == m_Grid.end()) continue;

                // Cell-level frustum cull
                Fbox cell_box;
                cell_box.min.set(float(cx)   * m_CellSize, -500.f, float(cz)   * m_CellSize);
                cell_box.max.set(float(cx+1) * m_CellSize, +500.f, float(cz+1) * m_CellSize);
                u32 mask = 0xff;
                if (!::Render->ViewBase.testAABB(cell_box.data(), mask))
                    continue;

                for (CCustomObject* obj : it->second)
                    if (seen.insert(obj).second)
                        out.push_back(obj);
            }
        }
    }

private:
    struct SKey {
        int x, z;
        bool operator==(const SKey& o) const noexcept { return x==o.x && z==o.z; }
    };
    struct SKeyHash {
        size_t operator()(const SKey& k) const noexcept {
            return size_t(k.x * 73856093) ^ size_t(k.z * 19349663);
        }
    };

    float m_CellSize;
    float m_InvCellSize;

    std::unordered_map<SKey, xr_vector<CCustomObject*>, SKeyHash> m_Grid;
    std::unordered_map<CCustomObject*, xr_vector<SKey>>           m_ObjCells;

    inline SKey CellOf(float x, float z) const
    {
        return { int(floorf(x * m_InvCellSize)), int(floorf(z * m_InvCellSize)) };
    }

    inline void GetObjectCells(CCustomObject* obj, xr_vector<SKey>& out) const
    {
        Fbox box;
        if (obj->GetBox(box) && box.min.x <= box.max.x && box.min.z <= box.max.z) {
            // Object has valid bbox — cover every cell it overlaps in XZ
            SKey k0 = CellOf(box.min.x, box.min.z);
            SKey k1 = CellOf(box.max.x, box.max.z);
            for (int cx = k0.x; cx <= k1.x; ++cx)
                for (int cz = k0.z; cz <= k1.z; ++cz)
                    out.push_back({cx, cz});
        } else {
            // No valid bbox: single cell by pivot
            out.push_back(CellOf(obj->FPosition.x, obj->FPosition.z));
        }
    }
};
