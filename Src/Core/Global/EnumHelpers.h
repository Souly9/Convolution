#pragma once


#define MAKE_FLAG_ENUM(name) static inline name operator|(name lhs, name rhs) { return static_cast<name>(static_cast<char>(lhs) | static_cast<char>(rhs)); }