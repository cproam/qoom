// STB image implementation unit
// Ensures stbi_load and friends are linked once in the program.
// Do not duplicate this definition elsewhere.

#define STB_IMAGE_IMPLEMENTATION
// On MSVC, silence fopen security warnings inside STB
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stb_image.h>
