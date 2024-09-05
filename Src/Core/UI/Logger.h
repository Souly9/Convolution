#pragma once

struct ApplicationInfos;

// Base class for all loggers
class Logger
{
public:
	virtual void Show(const ApplicationInfos&) const = 0;

	virtual void Clear() = 0;
};