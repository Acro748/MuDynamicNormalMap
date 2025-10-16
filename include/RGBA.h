#pragma once

namespace Mus {
	class RGBA {
	public:
		RGBA() {}
		RGBA(const unsigned long a_red, const unsigned long a_green, const unsigned long a_blue, const unsigned long a_alpha) {
			r = (float)a_red / 255;
			g = (float)a_green / 255;
			b = (float)a_blue / 255;
			a = (float)a_alpha / 255;
		}
		RGBA(const unsigned long a_red, const unsigned long a_green, const unsigned long a_blue) {
			r = (float)a_red / 255;
			g = (float)a_green / 255;
			b = (float)a_blue / 255;
		}
		RGBA(const float a_red, const float a_green, const float a_blue, const float a_alpha) {
			r = a_red;
			g = a_green;
			b = a_blue;
			a = a_alpha;
		}
		RGBA(const float a_red, const float a_green, const float a_blue) {
			r = a_red;
			g = a_green;
			b = a_blue;
			a = 1.0f;
		}
		RGBA(const RGBA& a_rhs) {
			r = a_rhs.r;
			g = a_rhs.g;
			b = a_rhs.b;
			a = a_rhs.a;
		}
		RGBA(const RE::NiColor& a_rhs) {
			r = a_rhs.red;
			g = a_rhs.green;
			b = a_rhs.blue;
			a = 1.0f;
		}
		RGBA(const RE::NiColorA& a_rhs) {
			r = a_rhs.red;
			g = a_rhs.green;
			b = a_rhs.blue;
			a = a_rhs.alpha;
		}
		RGBA(const float a_value) {
			r = a_value;
			g = a_value;
			b = a_value;
			a = a_value;
		}

		static RGBA GetGrayScaleWeight(bool a_correction) {
			RGBA weight;
			weight.r = a_correction ? 0.299f : 0.3333f;
			weight.g = a_correction ? 0.587f : 0.3333f;
			weight.b = a_correction ? 0.114f : 0.3333f;
			return weight;
		}

		static RGBA lerp(const RGBA minValue, const RGBA maxValue, const float value) {
			return minValue * (1.0f - value) + maxValue * value;
		}
		static RGBA clamp(const RGBA value, const RGBA minValue, const RGBA maxValue) {
			RGBA result;
			result.r = std::clamp(value.r, minValue.r, maxValue.r);
			result.g = std::clamp(value.g, minValue.g, maxValue.g);
			result.b = std::clamp(value.b, minValue.b, maxValue.b);
			result.a = std::clamp(value.a, minValue.a, maxValue.a);
			return result;
		}
		static RGBA clamp(const RGBA value) {
			RGBA result;
			result.r = std::clamp(value.r, 0.0f, 1.0f);
			result.g = std::clamp(value.g, 0.0f, 1.0f);
			result.b = std::clamp(value.b, 0.0f, 1.0f);
			result.a = std::clamp(value.a, 0.0f, 1.0f);
			return result;
		}
		static RGBA saturate(const RGBA a_rhs) {
			return clamp(a_rhs, 0.0f, 1.0f);
		}
		static float saturate(const float a_rhs) {
			return std::clamp(a_rhs, 0.0f, 1.0f);
		}

		void Set(const std::uint32_t a_rhs) {
			r = (float)((a_rhs & 0xFF000000) >> 24) / 255;
			g = (float)((a_rhs & 0x00FF0000) >> 16) / 255;
			b = (float)((a_rhs & 0x0000FF00) >> 8) / 255;
			a = (float)((a_rhs & 0x000000FF)) / 255;
		}
		void SetReverse(const std::uint32_t a_rhs) {
			a = (float)((a_rhs & 0xFF000000) >> 24) / 255;
			b = (float)((a_rhs & 0x00FF0000) >> 16) / 255;
			g = (float)((a_rhs & 0x0000FF00) >> 8) / 255;
			r = (float)((a_rhs & 0x000000FF)) / 255;
		}
		void SetGrayscale(const float a_alpha = 1.0f, const bool a_correction = true) {
			RGBA weight = GetGrayScaleWeight(a_correction);
			float grayscale = (r * weight.r) + (g * weight.g) + (b * weight.b);
			float fRed = ((1.0f - a_alpha) * r) + (a_alpha * grayscale);
			float fGreen = ((1.0f - a_alpha) * g) + (a_alpha * grayscale);
			float fBlue = ((1.0f - a_alpha) * b) + (a_alpha * grayscale);
			r = fRed;
			g = fGreen;
			b = fBlue;
		}
		void SetHex(const std::string a_rhs, const bool argb) {
			std::uint32_t value = strtoul(a_rhs.c_str(), 0, 16);
			if (argb)
			{
				a = (float)((value & 0xFF000000) >> 24) / 255;
				r = (float)((value & 0x00FF0000) >> 16) / 255;
				g = (float)((value & 0x0000FF00) >> 8) / 255;
				b = (float)((value & 0x000000FF)) / 255;
			}
			else //rgba
			{
				Set(value);
			}
		}

		std::uint32_t Get() const {
			return ((std::uint32_t)(r * 255) << 24) |
				((std::uint32_t)(g * 255) << 16) |
				((std::uint32_t)(b * 255) << 8) |
				((std::uint32_t)(a * 255));
		}
		std::uint32_t GetReverse() const {
			return ((std::uint32_t)(a * 255) << 24) |
				((std::uint32_t)(b * 255) << 16) |
				((std::uint32_t)(g * 255) << 8) |
				((std::uint32_t)(r * 255));
		}
		RGBA GetGrayscale(const float a_alpha = 1.0f, const bool a_correction = true) const {
			RGBA result;
			RGBA weight = GetGrayScaleWeight(a_correction);
			float grayscale = (r * weight.r) + (g * weight.g) + (b * weight.b);
			result.r = ((1.0f - a_alpha) * r) + (a_alpha * grayscale);
			result.g = ((1.0f - a_alpha) * g) + (a_alpha * grayscale);
			result.b = ((1.0f - a_alpha) * b) + (a_alpha * grayscale);
			result.a = a;
			return result;
		}
		std::string GetHex(const bool argb) const {
			std::string result, sred, sgreen, sblue, salpha;
			std::stringstream ssred, ssgreen, ssblue, ssalpha;
			ssred << std::hex << std::clamp((int)(r * 255), 0, 255);
			ssgreen << std::hex << std::clamp((int)(g * 255), 0, 255);
			ssblue << std::hex << std::clamp((int)(b * 255), 0, 255);
			ssalpha << std::hex << std::clamp((int)(a * 255), 0, 255);
			sred = ssred.str();
			sgreen = ssgreen.str();
			sblue = ssblue.str();
			salpha = ssalpha.str();

			if (sred.size() == 1)
				sred = "0" + sred;
			else if (sred.size() == 0)
				sred = "00";
			if (sgreen.size() == 1)
				sgreen = "0" + sgreen;
			else if (sgreen.size() == 0)
				sgreen = "00";
			if (sblue.size() == 1)
				sblue = "0" + sblue;
			else if (sblue.size() == 0)
				sblue = "00";
			if (salpha.size() == 1)
				salpha = "0" + salpha;
			else if (salpha.size() == 0)
				salpha = "00";
			if (argb)
				result = salpha + sred + sgreen + sblue;
			else
				result = sred + sgreen + sblue + salpha;
			return result;
		}
		std::uint32_t GetARGB() const {
			return ((std::uint32_t)(a * 255) << 24) |
				((std::uint32_t)(r * 255) << 16) |
				((std::uint32_t)(g * 255) << 8) |
				((std::uint32_t)(b * 255));
		}
		DirectX::XMFLOAT4 GetFloat4() const {
			return DirectX::XMFLOAT4(r, g, b, a);
		}
		std::uint8_t A() const {
			return (std::uint8_t)(a * 255);
		}
		std::uint8_t R() const {
			return (std::uint8_t)(r * 255);
		}
		std::uint8_t G() const {
			return (std::uint8_t)(g * 255);
		}
		std::uint8_t B() const {
			return (std::uint8_t)(b * 255);
		}

		RGBA& operator=(const RGBA& a_rhs) {
			r = a_rhs.r;
			g = a_rhs.g;
			b = a_rhs.b;
			a = a_rhs.a;
			return *this;
		}

		RGBA operator*(const RGBA& a_rhs) const {
			return RGBA(r * a_rhs.r, g * a_rhs.g, b * a_rhs.b, a * a_rhs.a);
		}
		RGBA operator*(const float a_scalar) const {
			return RGBA(r * a_scalar, g * a_scalar, b * a_scalar, a * a_scalar);
		}
		RGBA& operator*=(const RGBA& a_rhs) {
			r *= a_rhs.r;
			g *= a_rhs.g;
			b *= a_rhs.b;
			a *= a_rhs.a;
			return *this;
		}
		RGBA& operator*=(const float a_scalar) {
			r *= a_scalar;
			g *= a_scalar;
			b *= a_scalar;
			a *= a_scalar;
			return *this;
		}

		RGBA operator/(const float a_scalar) {
			return RGBA(r / a_scalar, g / a_scalar, b / a_scalar, a / a_scalar);
		}
		RGBA& operator/=(const float a_scalar) {
			r /= a_scalar;
			g /= a_scalar;
			b /= a_scalar;
			a /= a_scalar;
			return *this;
		}

		RGBA operator+(const RGBA& a_rhs) {
			return RGBA(r + a_rhs.r, g + a_rhs.g, b + a_rhs.b, a + a_rhs.a);
		}
		RGBA operator+(const float a_scalar) {
			return RGBA(r + a_scalar, g + a_scalar, b + a_scalar, a + a_scalar);
		}

		RGBA& operator+=(const RGBA& a_rhs) {
			r += a_rhs.r;
			g += a_rhs.g;
			b += a_rhs.b;
			a += a_rhs.a;
			return *this;
		}
		RGBA& operator+=(const float a_scalar) {
			r += a_scalar;
			g += a_scalar;
			b += a_scalar;
			a += a_scalar;
			return *this;
		}

		RGBA operator-(const RGBA& a_rhs) {
			return RGBA(r - a_rhs.r, g - a_rhs.g, b - a_rhs.b, a - a_rhs.a);
		}
		RGBA operator-(const float a_scalar) {
			return RGBA(r - a_scalar, g - a_scalar, b - a_scalar, a - a_scalar);
		}

		RGBA& operator-=(const RGBA& a_rhs) {
			r -= a_rhs.r;
			g -= a_rhs.g;
			b -= a_rhs.b;
			a -= a_rhs.a;
			return *this;
		}
		RGBA& operator-=(const float a_scalar) {
			r += a_scalar;
			g += a_scalar;
			b += a_scalar;
			a += a_scalar;
			return *this;
		}

		bool operator==(const RGBA a_rhs) {
			return r == a_rhs.r && g == a_rhs.g && b == a_rhs.b && a == a_rhs.a;
		}

		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 0.0f;
	};
}