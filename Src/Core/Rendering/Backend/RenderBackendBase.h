#pragma once

// Declared only: each backend provides the specialization, and an undefined primary
// can't be implicitly instantiated by headers that merely hold a RenderBackend pointer
template <typename API>
class RenderBackendImpl;
