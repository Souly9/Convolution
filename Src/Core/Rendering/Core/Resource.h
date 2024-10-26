#pragma once
#include "Core/Global/GlobalDefines.h"

#define TRACKED_DESC_IMPL DecRef(); if(ShouldDestroy()) { CleanUp(); }

// Generic render resource
class Resource
{
public:
	Resource() = default;
	virtual ~Resource() = default;
};

class TrackedResource : public Resource
{
public:
	TrackedResource() = default;
	virtual void CleanUp() {}

	virtual ~TrackedResource() {}

	void DecRef()
	{
		if (m_refCounter != 0)
			--m_refCounter;
	}
	bool ShouldDestroy() const
	{
		return m_refCounter == 0;
	}

	// Artifically increases refcounter to prevent deletion, DecRef needs to be called to properly track it again
	void Grab() const
	{
		++m_refCounter;
	}
	TrackedResource(const TrackedResource& other)
	{
		m_refCounter = other.m_refCounter;
#ifdef CONV_DEBUG
		m_debugName = other.m_debugName;
#endif
		other.Grab();
	}
	TrackedResource& operator=(const TrackedResource& other)
	{
		TrackedResource res;
		res.m_refCounter = other.m_refCounter;
#ifdef CONV_DEBUG
		res.m_debugName = other.m_debugName;
#endif
		other.Grab();
		return res;
	}
	TrackedResource(TrackedResource&& other) noexcept
	{
		m_refCounter = other.m_refCounter;
#ifdef CONV_DEBUG
		m_debugName = other.m_debugName;
#endif
		other.Grab();
	}

	TrackedResource& operator=(TrackedResource&& other) noexcept
	{
		TrackedResource res;
		res.m_refCounter = other.m_refCounter;
#ifdef CONV_DEBUG
		res.m_debugName = other.m_debugName;
#endif
		other.Grab();
		return res;
	}

	void SetDebugName(stltype::string&& name)
	{
#ifdef CONV_DEBUG
		m_debugName = name;
#endif
	}
	void SetDebugName(const stltype::string& name)
	{
#ifdef CONV_DEBUG
		m_debugName = name;
#endif
	}
	stltype::string GetDebugName() const
	{
#ifdef CONV_DEBUG
		return m_debugName;
#else
		return "";
#endif
	}
protected:
#ifdef CONV_DEBUG
	stltype::string m_debugName;
#endif

	mutable u32 m_refCounter{ 1 };
};
