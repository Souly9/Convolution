#pragma once
#include "Core/Global/GlobalDefines.h"
#include "ImGuiManager.h"

class UIElement
{
};

template <typename T>
class SelfInstantiatingUIElement : public UIElement
{
public:
    SelfInstantiatingUIElement()
    {
        s_forcedRegistrator;
    }

    static bool Init()
    {
        s_elements.emplace_back(std::make_unique<T>());
        return true;
    }
    static bool s_forcedRegistrator;

private:
    // Stored here to avoid deletion
    static stltype::vector<std::unique_ptr<UIElement>> s_elements;
};

template <class T>
bool SelfInstantiatingUIElement<T>::s_forcedRegistrator = SelfInstantiatingUIElement<T>::Init();
template <class T>
stltype::vector<std::unique_ptr<UIElement>> SelfInstantiatingUIElement<T>::s_elements =
    stltype::vector<std::unique_ptr<UIElement>>();
