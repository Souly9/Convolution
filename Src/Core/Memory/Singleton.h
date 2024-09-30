#pragma once

template<typename T>
class Singleton
{
public:
	static T& Get()
	{
		return m_instance;
	}

protected:
	Singleton() = default;
	~Singleton() = default;
	Singleton(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(const Singleton&) = delete;
	Singleton& operator=(Singleton&&) = delete;

	static inline T m_instance{};
};