#include "structure/StructureTransform.h"
namespace lholo::structure {
std::array<int, 3> transformCell(int x, int y, int z, int sizeX, int sizeZ, int mirror, int rotation) {
    if (mirror == 1)
        x = sizeX - 1 - x;
    if (mirror == 2)
        z = sizeZ - 1 - z;
    switch (rotation) {
    case 1:
        return {sizeZ - 1 - z, y, x};
    case 2:
        return {sizeX - 1 - x, y, sizeZ - 1 - z};
    case 3:
        return {z, y, sizeX - 1 - x};
    default:
        return {x, y, z};
    }
}
} // namespace lholo::structure
