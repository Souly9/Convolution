#include "GlobalVariables.h"

// Defined in one TU: constructed Engine then Renderer, destroyed in reverse. Constructors do no work.
Engine g_engine;
Renderer g_renderer;
