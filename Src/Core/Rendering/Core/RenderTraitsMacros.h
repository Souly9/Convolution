#pragma once

template <typename WrapperType, typename BaseType>
class ResPtr
{
public:
    ResPtr() = default;
    ResPtr(decltype(nullptr)) : m_ptr(nullptr) {}
    ResPtr(BaseType* ptr) : m_ptr(static_cast<WrapperType*>(ptr)) {}
    ResPtr(WrapperType* ptr) : m_ptr(ptr) {}

    WrapperType* operator->() const { return m_ptr; }
    WrapperType& operator*() const { return *m_ptr; }

    operator WrapperType*() const { return m_ptr; }
    operator BaseType*() const { return m_ptr; } // BaseType is a base class of WrapperType

    ResPtr& operator=(BaseType* ptr) { m_ptr = static_cast<WrapperType*>(ptr); return *this; }
    ResPtr& operator=(WrapperType* ptr) { m_ptr = ptr; return *this; }
    ResPtr& operator=(decltype(nullptr)) { m_ptr = nullptr; return *this; }

    bool operator==(const ResPtr& other) const { return m_ptr == other.m_ptr; }
    bool operator!=(const ResPtr& other) const { return m_ptr != other.m_ptr; }
    bool operator==(decltype(nullptr)) const { return m_ptr == nullptr; }
    bool operator!=(decltype(nullptr)) const { return m_ptr != nullptr; }

    explicit operator bool() const { return m_ptr != nullptr; }
    WrapperType* Get() const { return m_ptr; }

private:
    WrapperType* m_ptr{nullptr};
};

template <typename W, typename B>
inline bool operator==(decltype(nullptr), const ResPtr<W, B>& ptr) { return ptr == nullptr; }

template <typename W, typename B>
inline bool operator!=(decltype(nullptr), const ResPtr<W, B>& ptr) { return ptr != nullptr; }

#define DECLARE_RENDER_RESOURCE_TRAITS(ClassName, TraitName) \
    ClassName() = default; \
    ClassName(const typename APITraits<API>::TraitName& other) \
        : APITraits<API>::TraitName(other) {} \
    ClassName(typename APITraits<API>::TraitName&& other) noexcept \
        : APITraits<API>::TraitName(static_cast<typename APITraits<API>::TraitName&&>(other)) {} \
    ClassName& operator=(const typename APITraits<API>::TraitName& other) \
    { \
        APITraits<API>::TraitName::operator=(other); \
        return *this; \
    } \
    ClassName& operator=(typename APITraits<API>::TraitName&& other) noexcept \
    { \
        APITraits<API>::TraitName::operator=(static_cast<typename APITraits<API>::TraitName&&>(other)); \
        return *this; \
    } \
    /* Implicit conversion from TraitName* to ClassName* (base pointer to wrapper pointer) */ \
    ClassName(typename APITraits<API>::TraitName* p) : APITraits<API>::TraitName(*p) {} \
    /* Implicit pointer conversion helpers */ \
    static inline ClassName* Cast(typename APITraits<API>::TraitName* p) { return static_cast<ClassName*>(p); } \
    static inline const ClassName* Cast(const typename APITraits<API>::TraitName* p) { return static_cast<const ClassName*>(p); } \
    /* Implicit conversion operator for pointer types between wrapper and backend */ \
    operator typename APITraits<API>::TraitName*() { return static_cast<typename APITraits<API>::TraitName*>(this); } \
    operator const typename APITraits<API>::TraitName*() const { return static_cast<const typename APITraits<API>::TraitName*>(this); } \
    using Ptr = ResPtr<ClassName, typename APITraits<API>::TraitName>;

