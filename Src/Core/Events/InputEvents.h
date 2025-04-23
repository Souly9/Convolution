#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "EventUtils.h"

enum KeyType
{
	ForwardMove,
	BackwardMove,
	LeftMove,
	RightMove
};

struct KeyPressEventData
{
	KeyType key;
};
using KeyPressEventCallback = stltype::fixed_function<8, void(const KeyPressEventData&)>;
struct KeyHoldEventData
{
	KeyType key;
};
using KeyHoldEventCallback = stltype::fixed_function<8, void(const KeyHoldEventData&)>;
struct LeftMouseClickEventData
{
	double mousePosX, mousePosY;
};
using LeftMouseClickEventCallback = stltype::fixed_function<8, void(const LeftMouseClickEventData&)>;
struct MouseMoveEventData
{
	DirectX::XMUINT2 mousePos;
};
using MouseMoveEventCallback = stltype::fixed_function<8, void(const MouseMoveEventData&)>;
struct ScrollEventData
{
	mathstl::Vector2 scrollOffset;
};
using ScrollEventCallback = stltype::fixed_function<8, void(const ScrollEventData&)>;