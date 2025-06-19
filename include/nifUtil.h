#pragma once

namespace Mus::nif {
    namespace debug {
        bool visitObjects(RE::NiAVObject* a_object, std::function<bool(RE::NiAVObject*, int)> a_func, int depth = 0);

        bool printStuff(RE::NiAVObject* a_object, int depth);
        RE::BSVisit::BSVisitControl printGeometry(RE::BSGeometry* a_geometry);
        RE::BSVisit::BSVisitControl printAVObject(RE::NiAVObject* a_object);

        void printObjTree(RE::NiAVObject* obj);

    }

    class TaskupdateNode : public SKSE::detail::TaskDelegate
    {
    public:
        TaskupdateNode(RE::NiAVObject* node) : obj(node) { }

        virtual void Run();
        virtual void Dispose() { delete this; }

        RE::NiAVObject* obj;
    };

    inline RE::BSFixedString* BSFixedStringSet(RE::BSFixedString& ref, const char* a_data)
    {
        using func_t = decltype(&BSFixedStringSet);
        REL::Relocation<func_t> func{ REL::VariantID(67823, 69165, 0x00C6DC90) };
        return func(ref, a_data);
    }

    inline void setBSFixedString(RE::BSFixedString& ref, const char* name)
    {
        BSFixedStringSet(ref, name);
    }

    inline void setNiNodeName(RE::NiNode* node, const char* name)
    {
        setBSFixedString(node->name, name);
    }

    RE::NiNode* addParentToNode(RE::NiNode* node, const char* name);
    RE::NiNode* addChildToNode(RE::NiNode* node, const char* name);

    RE::BSFixedString GetVirtualNodeName(RE::BSFixedString nodeName);

    RE::NiMatrix3 SetMatrixByEntry(float a_entry00, float a_entry01, float a_entry02,
        float a_entry10, float a_entry11, float a_entry12,
        float a_entry20, float a_entry21, float a_entry22);

    RE::NiPoint3 GetEulerAngles(RE::NiMatrix3 a_angles);
    RE::NiMatrix3 SetEulerAngles(float a_point_x, float a_point_y, float a_pointz);
    RE::NiMatrix3 SetEulerAngles(RE::NiPoint3 a_point);
    RE::NiMatrix3 SetMirrorAngles(RE::NiMatrix3 a_angles);

    bool IsSkinned(RE::BSGeometry* a_geometry);
    bool IsSkinned(RE::BSLightingShaderProperty* property);
}