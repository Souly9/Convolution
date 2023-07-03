/***********************************************
* @author: Kai
* @title RenderInformation
* 
* Stores all information propagated upwards from the rendering backend 
* Mainly for the UI to display error codes etc
************************************************/
#pragma once
#include "Core/GlobalDefines.h"

class RenderInformation
{
public:
	struct GeneralBackendInfo
	{
		stltype::vector<stltype::string> _availableExtensions;
		stltype::vector<stltype::string> _availableLayers;
	};
	GeneralBackendInfo m_generalInfo;

	struct BackendErrors
	{
		stltype::vector<stltype::string> _errorsThisFrame{10};
		stltype::vector<stltype::string> _persistentErrors{10};
	};
	BackendErrors m_backendErrors;
};