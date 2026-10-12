#pragma once

/* Replacement for the CMake-generated export header of the upstream miniz
   project. The sources are compiled directly into the executable, so no
   import/export annotations are needed. */

#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#define MINIZ_DEPRECATED
