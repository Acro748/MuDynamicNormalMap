#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

class SKEEFixedString
{
public:
	SKEEFixedString() : m_internal() { m_hash = hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const char * str) : m_internal(str) { m_hash = hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const std::string & str) : m_internal(str) { m_hash = hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const RE::BSFixedString & str) : m_internal(str.c_str()) { m_hash = hash_lower(m_internal.c_str(), m_internal.size()); }

	bool operator<(const SKEEFixedString& x) const
	{
		return _stricmp(m_internal.c_str(), x.m_internal.c_str()) < 0;
	}

	bool operator==(const SKEEFixedString& x) const
	{
		if (m_internal.size() != x.m_internal.size())
			return false;

		if (_stricmp(m_internal.c_str(), x.m_internal.c_str()) == 0)
			return true;

		return false;
	}
	
	size_t length() const { return m_internal.size(); }

	operator RE::BSFixedString() const { return RE::BSFixedString(m_internal.c_str()); }
	RE::BSFixedString AsBSFixedString() const { return operator RE::BSFixedString(); }

	const char * c_str() const { return operator const char *(); }
	operator const char *() const { return m_internal.c_str(); }

	size_t hash_lower(const char * str, size_t count)
	{
		const size_t _FNV_offset_basis = 14695981039346656037ULL;
		const size_t _FNV_prime = 1099511628211ULL;

		size_t _Val = _FNV_offset_basis;
		for (size_t _Next = 0; _Next < count; ++_Next)
		{	// fold in another byte
			_Val ^= (size_t)tolower(str[_Next]);
			_Val *= _FNV_prime;
		}
		return _Val;
	}

	size_t GetHash() const
	{
		return m_hash;
	}

protected:
	std::string		m_internal;
	size_t			m_hash;
};

typedef std::shared_ptr<SKEEFixedString> StringTableItem;
typedef std::weak_ptr<SKEEFixedString> WeakTableItem;

namespace std {
	template <> struct hash<SKEEFixedString>
	{
		size_t operator()(const SKEEFixedString& x) const
		{
			return x.GetHash();
		}
	};
	template <> struct hash<StringTableItem>
	{
		size_t operator()(const StringTableItem& x) const
		{
			return x->GetHash();
		}
	};
}

class ICriticalSection
{
public:
	ICriticalSection() { InitializeCriticalSection(&critSection); }
	~ICriticalSection() { DeleteCriticalSection(&critSection); }

	void	Enter(void) { EnterCriticalSection(&critSection); }
	void	Leave(void) { LeaveCriticalSection(&critSection); }
	bool	TryEnter(void) { return TryEnterCriticalSection(&critSection) != 0; }

private:
	CRITICAL_SECTION	critSection;
};

typedef std::unordered_map<std::uint32_t, StringTableItem> StringIdMap;
class StringTable
{
public:

private:
	std::unordered_map<SKEEFixedString, WeakTableItem>	m_table;
	std::vector<WeakTableItem>							m_tableVector;
	mutable ICriticalSection							m_lock;
};
