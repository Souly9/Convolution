#pragma once

struct AABB
{
	mathstl::Vector3 extents{ 0.5f, 0.5f, 0.5f };
	mathstl::Vector3 center{};

	bool IsValid()
	{
		return extents.LengthSquared() != 0 && center.LengthSquared() != 0;
	}
};