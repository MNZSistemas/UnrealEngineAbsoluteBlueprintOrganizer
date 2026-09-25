// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

/** True when the engine being compiled against is Major.Minor or newer. Compile-time only. */
#define UEABO_ENGINE_AT_LEAST(Major, Minor) \
	((ENGINE_MAJOR_VERSION > (Major)) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))
