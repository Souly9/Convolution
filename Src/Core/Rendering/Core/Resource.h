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

	void Grab() const
	{
		++m_refCounter;
	}
	TrackedResource(const TrackedResource& other)
	{
		m_refCounter = other.m_refCounter;
		other.Grab();
	}
	TrackedResource& operator=(const TrackedResource& other)
	{
		TrackedResource res;
		res.m_refCounter = other.m_refCounter;
		other.Grab();
		return res;
	}
	TrackedResource(TrackedResource&& other) noexcept
	{
		m_refCounter = other.m_refCounter;
		other.Grab();
	}

	TrackedResource& operator=(TrackedResource&& other) noexcept
	{
		TrackedResource res;
		res.m_refCounter = other.m_refCounter;
		other.Grab();
		return res;
	}

#ifdef CONV_DEBUG
	void SetDebugName(stltype::string&& name)
	{
		m_debugName = name;
	}
	stltype::string GetDebugName() const
	{
		return m_debugName;
	}
protected:
	stltype::string m_debugName;
#endif

protected:
	mutable u32 m_refCounter{ 1 };
};
