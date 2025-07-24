#pragma once

#include "StringTable.h"

class OverrideVariant
{
public:
	OverrideVariant() : type(kType_None), index(-1) { };
	~OverrideVariant() { };

	bool operator<(const OverrideVariant & rhs) const	{ return key < rhs.key || (key == rhs.key && index < rhs.index); }
	bool operator==(const OverrideVariant & rhs) const	{ return key == rhs.key && index == rhs.index; }

	std::uint16_t key;
	enum
	{
		kKeyMax = 0xFFFF,
		kIndexMax = 0xFF
	};
	enum
	{
		kParam_ShaderEmissiveColor = 0,
		kParam_ShaderEmissiveMultiple,
		kParam_ShaderGlossiness,
		kParam_ShaderSpecularStrength,
		kParam_ShaderLightingEffect1,
		kParam_ShaderLightingEffect2,
		kParam_ShaderTextureSet,
		kParam_ShaderTintColor,
		kParam_ShaderAlpha,
		kParam_ShaderTexture,

		kParam_ControllersStart = 20,
		kParam_ControllerStartStop = kParam_ControllersStart,
		kParam_ControllerStartTime,
		kParam_ControllerStopTime,
		kParam_ControllerFrequency,
		kParam_ControllerPhase,
		kParam_ControllersEnd = kParam_ControllerPhase,

		kParam_NodeTransformStart = 30,
		kParam_NodeTransformScale = kParam_NodeTransformStart,
		kParam_NodeTransformPosition,
		kParam_NodeTransformRotation,
		kParam_NodeTransformScaleMode,
		kParam_NodeTransformEnd = kParam_NodeTransformRotation,

		kParam_NodeDestination = 40
	};
	enum
	{
		kType_None = 0,
		kType_Identifier = 1,
		kType_String = 2,
		kType_Int = 3,
		kType_Float = 4,
		kType_Bool = 5
	};

	std::uint8_t	type;
	std::int8_t	index;
	union
	{
		std::int32_t			i;
		std::uint32_t			u;
		float			f;
		bool			b;
		void			* p;
	} data;
	StringTableItem		str;

	void	SetNone(void)
	{
		type = kType_None;
		index = -1;
		data.u = 0;
		str = nullptr;
	}

	void	SetInt(std::uint16_t paramKey, std::int8_t controllerIndex, std::int32_t i)
	{
		key = paramKey;
		type = kType_Int;
		index = controllerIndex;
		data.i = i;
		str = nullptr;
	}

	void	SetFloat(std::uint16_t paramKey, std::int8_t controllerIndex, float f)
	{
		key = paramKey;
		type = kType_Float;
		index = controllerIndex;
		data.f = f;
		str = nullptr;
	}

	void	SetBool(std::uint16_t paramKey, std::int8_t controllerIndex, bool b)
	{
		key = paramKey;
		type = kType_Bool;
		index = controllerIndex;
		data.b = b;
		str = nullptr;
	}

	void	SetString(std::uint16_t paramKey, std::int8_t controllerIndex, SKEEFixedString string)
	{
		key = paramKey;
		type = kType_String;
		index = controllerIndex;
		data.p = nullptr;
		str = std::shared_ptr<SKEEFixedString>(new SKEEFixedString(string));
	}

	void	SetColor(std::uint16_t paramKey, std::int8_t controllerIndex, RE::NiColor color)
	{
		key = paramKey;
		type = kType_Int;
		index = controllerIndex;
		data.u = (std::uint8_t)(color.red * 255) << 16 | (std::uint8_t)(color.green * 255) << 8 | (std::uint8_t)(color.blue * 255);
		str = nullptr;
	}

	void	SetColorA(std::uint16_t paramKey, std::int8_t controllerIndex, RE::NiColorA color)
	{
		key = paramKey;
		type = kType_Int;
		index = controllerIndex;
		data.u = (std::uint8_t)(color.alpha * 255) << 24 | (std::uint8_t)(color.red * 255) << 16 | (std::uint8_t)(color.green * 255) << 8 | (std::uint8_t)(color.blue * 255);
		str = nullptr;
	}

	void SetIdentifier(std::uint16_t paramKey, std::int8_t controllerIndex, void * ptr)
	{
		key = paramKey;
		type = kType_Identifier;
		index = controllerIndex;
		data.p = ptr;
		str = nullptr;
	}

	static bool IsIndexValid(std::uint16_t key) {
		return (key >= OverrideVariant::kParam_ControllersStart && key <= OverrideVariant::kParam_ControllersEnd) || key == OverrideVariant::kParam_ShaderTexture || (key >= OverrideVariant::kParam_NodeTransformStart && key <= OverrideVariant::kParam_NodeTransformEnd);
	};
};

inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, float* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderEmissiveMultiple:
	case OverrideVariant::kParam_ShaderGlossiness:
	case OverrideVariant::kParam_ShaderSpecularStrength:
	case OverrideVariant::kParam_ShaderLightingEffect1:
	case OverrideVariant::kParam_ShaderLightingEffect2:
	case OverrideVariant::kParam_ShaderAlpha:
	case OverrideVariant::kParam_ControllerStartStop:
	case OverrideVariant::kParam_ControllerStartTime:
	case OverrideVariant::kParam_ControllerStopTime:
	case OverrideVariant::kParam_ControllerFrequency:
	case OverrideVariant::kParam_ControllerPhase:
	case OverrideVariant::kParam_NodeTransformPosition:
	case OverrideVariant::kParam_NodeTransformRotation:
	case OverrideVariant::kParam_NodeTransformScale:
		dst->SetFloat(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, std::uint32_t* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderEmissiveColor:
	case OverrideVariant::kParam_ShaderTintColor:
	case OverrideVariant::kParam_NodeTransformScaleMode:
		dst->SetInt(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, std::int32_t* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderEmissiveColor:
	case OverrideVariant::kParam_ShaderTintColor:
	case OverrideVariant::kParam_NodeTransformScaleMode:
		dst->SetInt(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, bool* src)
{
	dst->SetNone();
	//dst->SetBool(key, index, *src);
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, const char** src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderTexture:
		dst->SetString(key, index, *src);
		break;
	case OverrideVariant::kParam_NodeDestination:
		dst->SetString(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, std::string* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderTexture:
		dst->SetString(key, index, *src);
		break;
	case OverrideVariant::kParam_NodeDestination:
		dst->SetString(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, SKEEFixedString* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderTexture:
		dst->SetString(key, index, *src);
		break;
	case OverrideVariant::kParam_NodeDestination:
		dst->SetString(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, RE::BSFixedString* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderTexture:
		dst->SetString(key, index, *src);
		break;
	case OverrideVariant::kParam_NodeDestination:
		dst->SetString(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, RE::NiColor* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderEmissiveColor:
	case OverrideVariant::kParam_ShaderTintColor:
		dst->SetColor(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, RE::NiColorA* src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderEmissiveColor:
		dst->SetColorA(key, index, *src);
		break;
	default:
		dst->SetNone();
		break;
	}
}
inline void PackValue(OverrideVariant* dst, std::uint16_t key, std::uint8_t index, RE::BGSTextureSet** src)
{
	switch (key)
	{
	case OverrideVariant::kParam_ShaderTextureSet:
		dst->SetIdentifier(key, index, (void*)*src);
		break;
	default:
		dst->SetNone();
		break;
	}
}

inline void UnpackValue(float* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_Int:
		*dst = src->data.i;
		break;

	case OverrideVariant::kType_Float:
		*dst = src->data.f;
		break;

	case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

	default:
		*dst = 0;
		break;
	}
}
inline void UnpackValue(std::uint32_t* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_Int:
		*dst = src->data.u;
		break;

	case OverrideVariant::kType_Float:
		*dst = src->data.f;
		break;

	case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

	default:
		*dst = 0;
		break;
	}
}
inline void UnpackValue(std::int32_t* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_Int:
		*dst = src->data.u;
		break;

	case OverrideVariant::kType_Float:
		*dst = src->data.f;
		break;

	case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

	default:
		*dst = 0;
		break;
	}
}
inline void UnpackValue(bool* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_Int:
		*dst = src->data.u != 0;
		break;

	case OverrideVariant::kType_Float:
		*dst = src->data.f != 0;
		break;

	case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

	default:
		*dst = 0;
		break;
	}
}
inline void UnpackValue(SKEEFixedString* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_String:
	{
		auto str = src->str;
		*dst = str ? str->c_str() : "";
	}
	break;
	default:
		*dst = "";
		break;
	}
}
inline void UnpackValue(RE::BSFixedString* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_String:
	{
		auto str = src->str;
		*dst = str ? str->c_str() : "";
	}
	break;
	default:
		*dst = "";
		break;
	}
}
inline void UnpackValue(RE::NiColor* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_Int:
		dst->red = ((src->data.u >> 16) & 0xFF) / 255.0;
		dst->green = ((src->data.u >> 8) & 0xFF) / 255.0;
		dst->blue = ((src->data.u) & 0xFF) / 255.0;
		break;

	default:
		dst->red = 0;
		dst->green = 0;
		dst->blue = 0;
		break;
	}
}
inline void UnpackValue(RE::NiColorA* dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_Int:
		dst->alpha = ((src->data.u >> 24) & 0xFF) / 255.0;
		dst->red = ((src->data.u >> 16) & 0xFF) / 255.0;
		dst->green = ((src->data.u >> 8) & 0xFF) / 255.0;
		dst->blue = ((src->data.u) & 0xFF) / 255.0;
		break;

	default:
		dst->red = 0;
		dst->green = 0;
		dst->blue = 0;
		dst->alpha = 0;
		break;
	}
}
inline void UnpackValue(RE::BGSTextureSet** dst, OverrideVariant* src)
{
	switch (src->type)
	{
	case OverrideVariant::kType_Identifier:
		*dst = (RE::BGSTextureSet*)src->data.p;
		break;

	default:
		*dst = NULL;
		break;
	}
}
