#pragma once

// Defines an event, its callbacks and a mutex since callbacks can be added from multiple threads
#define EVENT(name) stltype::fixed_function<16, void(const name##EventData&)> m_on##name{}; \
stltype::vector<name##EventCallback> m_on##name##Callbacks{}; \
threadSTL::Futex m_##name##Futex{};

// Defines the events registering and trigger function
#define EVENT_FUNCTIONS(name) void On##name(const name##EventData& data) \
{ \
m_##name##Futex.Lock(); \
for (auto& callback : m_on##name##Callbacks) { callback(data); } \
m_##name##Futex.Unlock(); \
} \
void Add##name##EventCallback(name##EventCallback&& callback) \
{ \
m_##name##Futex.Lock(); \
m_on##name##Callbacks.push_back(std::move(callback)); \
m_##name##Futex.Unlock(); \
}