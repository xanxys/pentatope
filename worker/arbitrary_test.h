// Utilities to generate random instances of a variety of types.
#pragma once

#include <random>
#include <vector>

#include <scene.h>
#include <space.h>

std::vector<pentatope::Object> arbitraryObjects(std::mt19937& rg, int n_target = -1);
pentatope::Ray arbitraryRay(std::mt19937& rg);
