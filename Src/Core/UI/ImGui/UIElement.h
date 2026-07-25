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
        (void)s_forcedRegistrator;
    }

    static bool Init()
    {
        s_elements.emplace_back(stltype::make_unique<T>());
        return true;
    }
    static bool s_forcedRegistrator;

private:
    // Stored here to avoid deletion
    static stltype::vector<stltype::unique_ptr<UIElement>> s_elements;
};

template <class T>
bool SelfInstantiatingUIElement<T>::s_forcedRegistrator = SelfInstantiatingUIElement<T>::Init();
template <typename T>
stltype::vector<stltype::unique_ptr<UIElement>> SelfInstantiatingUIElement<T>::s_elements =
    stltype::vector<stltype::unique_ptr<UIElement>>();
