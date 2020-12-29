#include "random/simplex_noise.h"
#include "math/constexpr_math.h"
#include <random>
#include <tuple>

/*
 * A fast simplex noise implementation for 2D, 3D and 4D.
 * This is a rewrite of the code used in project WCore.
 * This code was itself a port from:
 * http://www.itn.liu.se/~stegu/simplexnoise/SimplexNoise.java
 * 
 * Original description states:
 * * Based on example code by Stefan Gustavson (stegu@itn.liu.se).
 * * Optimisations by Peter Eastman (peastman@drizzle.stanford.edu).
 * * Better rank ordering method by Stefan Gustavson in 2012.
 * * 
 * * This could be speeded up even further, but it's useful as it is.
 * * 
 * * Version 2015-07-01
 * * 
 * * This code was placed in the public domain by its original author,
 * * Stefan Gustavson. You may use it as you see fit, but
 * * attribution is appreciated.
 *
 * The code retains the original comments.
 *
 */

namespace kb
{
namespace rng
{

#define NOISE_3D_USE_RANK_ALGORITHM 0

// Skewing and unskewing factors
constexpr float k_F2 = 0.5f * (math::fsqrt(3.f) - 1.f);
constexpr float k_G2 = (3.f - math::fsqrt(3.f)) / 6.f;
constexpr float k_F3 = 1.f / 3.f;
constexpr float k_G3 = 1.f / 6.f;
constexpr float k_F4 = (math::fsqrt(5.f) - 1.f) / 4.f;
constexpr float k_G4 = (5.f - math::fsqrt(5.f)) / 20.f;

// The gradients are the midpoints of the vertices of a cube.
constexpr float grad3[12][3] = {{1, 1, 0},  {-1, 1, 0},  {1, -1, 0}, {-1, -1, 0}, {1, 0, 1},  {-1, 0, 1},
                                {1, 0, -1}, {-1, 0, -1}, {0, 1, 1},  {0, -1, 1},  {0, 1, -1}, {0, -1, -1}};

// The gradients are the midpoints of the vertices of a hypercube.
constexpr float grad4[32][4] = {
    {0, 1, 1, 1},    {0, 1, 1, -1},   {0, 1, -1, 1},   {0, 1, -1, -1}, {0, -1, 1, 1},  {0, -1, 1, -1}, {0, -1, -1, 1},
    {0, -1, -1, -1}, {1, 0, 1, 1},    {1, 0, 1, -1},   {1, 0, -1, 1},  {1, 0, -1, -1}, {-1, 0, 1, 1},  {-1, 0, 1, -1},
    {-1, 0, -1, 1},  {-1, 0, -1, -1}, {1, 1, 0, 1},    {1, 1, 0, -1},  {1, -1, 0, 1},  {1, -1, 0, -1}, {-1, 1, 0, 1},
    {-1, 1, 0, -1},  {-1, -1, 0, 1},  {-1, -1, 0, -1}, {1, 1, 1, 0},   {1, 1, -1, 0},  {1, -1, 1, 0},  {1, -1, -1, 0},
    {-1, 1, 1, 0},   {-1, 1, -1, 0},  {-1, -1, 1, 0},  {-1, -1, -1, 0}};

inline float dot(const float* g, float x, float y) { return g[0] * x + g[1] * y; }
inline float dot(const float* g, float x, float y, float z) { return g[0] * x + g[1] * y + g[2] * z; }
inline float dot(const float* g, float x, float y, float z, float w)
{
    return g[0] * x + g[1] * y + g[2] * z + g[3] * w;
}

SimplexNoiseGenerator::SimplexNoiseGenerator()
{
    std::random_device r;
    std::default_random_engine rng(r());
    init(rng);
}

float SimplexNoiseGenerator::operator()(float xin, float yin)
{
    // Skew the input space to determine which simplex cell we're in
    float s = (xin + yin) * k_F2; // Hairy factor for 2D
    int i = int(std::floor(xin + s));
    int j = int(std::floor(yin + s));
    float t = float(i + j) * k_G2;
    float X0 = float(i) - t; // Unskew the cell origin back to (x,y) space
    float Y0 = float(j) - t;
    float x0 = xin - X0; // The x,y distances from the cell origin
    float y0 = yin - Y0;

    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    // i1, j1: Offsets for second (middle) corner of simplex in (i,j) coords
    // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    // upper triangle, YX order: (0,0)->(0,1)->(1,1)
    size_t i1 = (x0 > y0) ? 1 : 0;
    size_t j1 = 1 - i1;
    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6

    float x1 = x0 - float(i1) + k_G2; // Offsets for middle corner in (x,y) unskewed coords
    float y1 = y0 - float(j1) + k_G2;
    float x2 = x0 - 1.f + 2.f * k_G2; // Offsets for last corner in (x,y) unskewed coords
    float y2 = y0 - 1.f + 2.f * k_G2;

    // Work out the hashed gradient indices of the three simplex corners
    size_t ii = i & 255;
    size_t jj = j & 255;
    size_t gi0 = perm_[ii + perm_[jj]] % 12;
    size_t gi1 = perm_[ii + i1 + perm_[jj + j1]] % 12;
    size_t gi2 = perm_[ii + 1 + perm_[jj + 1]] % 12;

    // Calculate the contribution from the three corners
    float t0 = 0.5f - x0 * x0 - y0 * y0;
    float n0 = (t0 < 0) ? 0.f : t0 * t0 * t0 * t0 * dot(grad3[gi0], x0, y0); // (x,y) of grad3 used for 2D gradient
    float t1 = 0.5f - x1 * x1 - y1 * y1;
    float n1 = (t1 < 0) ? 0.f : t1 * t1 * t1 * t1 * dot(grad3[gi1], x1, y1);
    float t2 = 0.5f - x2 * x2 - y2 * y2;
    float n2 = (t2 < 0) ? 0.f : t2 * t2 * t2 * t2 * dot(grad3[gi2], x2, y2);

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    return 70.f * (n0 + n1 + n2);
}
float SimplexNoiseGenerator::operator()(float xin, float yin, float zin)
{
    // Skew the input space to determine which simplex cell we're in
    float s = (xin + yin + zin) * k_F3; // Very nice and simple skew factor for 3D
    int i = int(std::floor(xin + s));
    int j = int(std::floor(yin + s));
    int k = int(std::floor(zin + s));

    float t = float(i + j + k) * k_G3;
    float X0 = float(i) - t; // Unskew the cell origin back to (x,y,z) space
    float Y0 = float(j) - t;
    float Z0 = float(k) - t;
    float x0 = xin - X0; // The x,y,z distances from the cell origin
    float y0 = yin - Y0;
    float z0 = zin - Z0;

    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
#if NOISE_3D_USE_RANK_ALGORITHM
    // NOTE(ndx): Coherent with the 4D case, but this is actually slower on my config
    int rankx = int(x0 > y0) + int(x0 > z0);
    int ranky = int(y0 >= x0) + int(y0 > z0);
    int rankz = int(z0 >= x0) + int(z0 >= y0);

    // Offsets for second corner of simplex in (i,j,k) coords
    size_t i1 = size_t(rankx >= 2);
    size_t j1 = size_t(ranky >= 2);
    size_t k1 = size_t(rankz >= 2);
    // Offsets for third corner of simplex in (i,j,k) coords
    size_t i2 = size_t(rankx >= 1);
    size_t j2 = size_t(ranky >= 1);
    size_t k2 = size_t(rankz >= 1);
#else
    size_t i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    size_t i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords

    if(x0 >= y0)
    {
        if(y0 >= z0)
            std::tie(i1, j1, k1, i2, j2, k2) = std::make_tuple(1, 0, 0, 1, 1, 0); // X Y Z order
        else if(x0 >= z0)
            std::tie(i1, j1, k1, i2, j2, k2) = std::make_tuple(1, 0, 0, 1, 0, 1); // X Z Y order
        else
            std::tie(i1, j1, k1, i2, j2, k2) = std::make_tuple(0, 0, 1, 1, 0, 1); // Z X Y order
    }
    else
    {
        if(y0 < z0)
            std::tie(i1, j1, k1, i2, j2, k2) = std::make_tuple(0, 0, 1, 0, 1, 1); // Z Y X order
        else if(x0 < z0)
            std::tie(i1, j1, k1, i2, j2, k2) = std::make_tuple(0, 1, 0, 0, 1, 1); // Y Z X order
        else
            std::tie(i1, j1, k1, i2, j2, k2) = std::make_tuple(0, 1, 0, 1, 1, 0); // Y X Z order
    }
#endif
    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.
    float x1 = x0 - float(i1) + k_G3; // Offsets for second corner in (x,y,z) coords
    float y1 = y0 - float(j1) + k_G3;
    float z1 = z0 - float(k1) + k_G3;
    float x2 = x0 - float(i2) + 2.f * k_G3; // Offsets for third corner in (x,y,z) coords
    float y2 = y0 - float(j2) + 2.f * k_G3;
    float z2 = z0 - float(k2) + 2.f * k_G3;
    float x3 = x0 - 1.f + 3.f * k_G3; // Offsets for last corner in (x,y,z) coords
    float y3 = y0 - 1.f + 3.f * k_G3;
    float z3 = z0 - 1.f + 3.f * k_G3;

    // Work out the hashed gradient indices of the four simplex corners
    size_t ii = i & 255;
    size_t jj = j & 255;
    size_t kk = k & 255;
    size_t gi0 = perm_[ii + perm_[jj + perm_[kk]]] % 12;
    size_t gi1 = perm_[ii + i1 + perm_[jj + j1 + perm_[kk + k1]]] % 12;
    size_t gi2 = perm_[ii + i2 + perm_[jj + j2 + perm_[kk + k2]]] % 12;
    size_t gi3 = perm_[ii + 1 + perm_[jj + 1 + perm_[kk + 1]]] % 12;

    // Calculate the contribution from the four corners
    // >Replaced 0.6 term by 0.5 for continuity at simplex boundaries
    float t0 = 0.5f - x0 * x0 - y0 * y0 - z0 * z0;
    float n0 = (t0 < 0) ? 0.f : t0 * t0 * t0 * t0 * dot(grad3[gi0], x0, y0, z0);
    float t1 = 0.5f - x1 * x1 - y1 * y1 - z1 * z1;
    float n1 = (t1 < 0) ? 0.f : t1 * t1 * t1 * t1 * dot(grad3[gi1], x1, y1, z1);
    float t2 = 0.5f - x2 * x2 - y2 * y2 - z2 * z2;
    float n2 = (t2 < 0) ? 0.f : t2 * t2 * t2 * t2 * dot(grad3[gi2], x2, y2, z2);
    float t3 = 0.5f - x3 * x3 - y3 * y3 - z3 * z3;
    float n3 = (t3 < 0) ? 0.f : t3 * t3 * t3 * t3 * dot(grad3[gi3], x3, y3, z3);

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to stay just inside [-1,1]
    return 32.f * (n0 + n1 + n2 + n3);
}
float SimplexNoiseGenerator::operator()(float xin, float yin, float zin, float win)
{
    // Skew the (x,y,z,w) space to determine which cell of 24 simplices we're in
    float s = (xin + yin + zin + win) * k_F4; // Factor for 4D skewing
    int i = int(std::floor(xin + s));
    int j = int(std::floor(yin + s));
    int k = int(std::floor(zin + s));
    int l = int(std::floor(win + s));
    float t = float(i + j + k + l) * k_G4; // Factor for 4D unskewing
    float X0 = float(i) - t;               // Unskew the cell origin back to (x,y,z,w) space
    float Y0 = float(j) - t;
    float Z0 = float(k) - t;
    float W0 = float(l) - t;
    float x0 = xin - X0; // The x,y,z,w distances from the cell origin
    float y0 = yin - Y0;
    float z0 = zin - Z0;
    float w0 = win - W0;

    // For the 4D case, the simplex is a 4D shape I won't even try to describe.
    // To find out which of the 24 possible simplices we're in, we need to
    // determine the magnitude ordering of x0, y0, z0 and w0.
    // Six pair-wise comparisons are performed between each possible pair
    // of the four coordinates, and the results are used to rank the numbers.
    int rankx = int(x0 > y0) + int(x0 > z0) + int(x0 > w0);
    int ranky = int(y0 >= x0) + int(y0 > z0) + int(y0 > w0);
    int rankz = int(z0 >= x0) + int(z0 >= y0) + int(z0 > w0);
    int rankw = int(w0 >= x0) + int(w0 >= y0) + int(w0 >= z0);

    // simplex[c] is a 4-vector with the numbers 0, 1, 2 and 3 in some order.
    // Many values of c will never occur, since e.g. x>y>z>w makes x<z, y<w and x<w
    // impossible. Only the 24 indices which have non-zero entries make any sense.
    // We use a thresholding to set the coordinates in turn from the largest magnitude.

    // The integer offsets for the second simplex corner
    // Rank 3 denotes the largest coordinate.
    size_t i1 = size_t(rankx >= 3);
    size_t j1 = size_t(ranky >= 3);
    size_t k1 = size_t(rankz >= 3);
    size_t l1 = size_t(rankw >= 3);
    // The integer offsets for the third simplex corner
    // Rank 2 denotes the second largest coordinate.
    size_t i2 = size_t(rankx >= 2);
    size_t j2 = size_t(ranky >= 2);
    size_t k2 = size_t(rankz >= 2);
    size_t l2 = size_t(rankw >= 2);
    // The integer offsets for the fourth simplex corner
    // Rank 1 denotes the second smallest coordinate.
    size_t i3 = size_t(rankx >= 1);
    size_t j3 = size_t(ranky >= 1);
    size_t k3 = size_t(rankz >= 1);
    size_t l3 = size_t(rankw >= 1);
    // The fifth corner has all coordinate offsets = 1, so no need to compute that.
    float x1 = x0 - float(i1) + k_G4; // Offsets for second corner in (x,y,z,w) coords
    float y1 = y0 - float(j1) + k_G4;
    float z1 = z0 - float(k1) + k_G4;
    float w1 = w0 - float(l1) + k_G4;
    float x2 = x0 - float(i2) + 2.f * k_G4; // Offsets for third corner in (x,y,z,w) coords
    float y2 = y0 - float(j2) + 2.f * k_G4;
    float z2 = z0 - float(k2) + 2.f * k_G4;
    float w2 = w0 - float(l2) + 2.f * k_G4;
    float x3 = x0 - float(i3) + 3.f * k_G4; // Offsets for fourth corner in (x,y,z,w) coords
    float y3 = y0 - float(j3) + 3.f * k_G4;
    float z3 = z0 - float(k3) + 3.f * k_G4;
    float w3 = w0 - float(l3) + 3.f * k_G4;
    float x4 = x0 - 1.f + 4.f * k_G4; // Offsets for last corner in (x,y,z,w) coords
    float y4 = y0 - 1.f + 4.f * k_G4;
    float z4 = z0 - 1.f + 4.f * k_G4;
    float w4 = w0 - 1.f + 4.f * k_G4;
    // Work out the hashed gradient indices of the five simplex corners
    size_t ii = i & 255;
    size_t jj = j & 255;
    size_t kk = k & 255;
    size_t ll = l & 255;
    size_t gi0 = perm_[ii + perm_[jj + perm_[kk + perm_[ll]]]] % 32;
    size_t gi1 = perm_[ii + i1 + perm_[jj + j1 + perm_[kk + k1 + perm_[ll + l1]]]] % 32;
    size_t gi2 = perm_[ii + i2 + perm_[jj + j2 + perm_[kk + k2 + perm_[ll + l2]]]] % 32;
    size_t gi3 = perm_[ii + i3 + perm_[jj + j3 + perm_[kk + k3 + perm_[ll + l3]]]] % 32;
    size_t gi4 = perm_[ii + 1 + perm_[jj + 1 + perm_[kk + 1 + perm_[ll + 1]]]] % 32;
    // Calculate the contribution from the five corners
    // >Replaced 0.6 term by 0.5 for continuity at simplex boundaries
    // >Putting back 0.6 might enhance quality but I prefer continuity for now
    float t0 = 0.5f - x0 * x0 - y0 * y0 - z0 * z0 - w0 * w0;
    float t1 = 0.5f - x1 * x1 - y1 * y1 - z1 * z1 - w1 * w1;
    float t2 = 0.5f - x2 * x2 - y2 * y2 - z2 * z2 - w2 * w2;
    float t3 = 0.5f - x3 * x3 - y3 * y3 - z3 * z3 - w3 * w3;
    float t4 = 0.5f - x4 * x4 - y4 * y4 - z4 * z4 - w4 * w4;

    float n0 = (t0 < 0) ? 0.f : t0 * t0 * t0 * t0 * dot(grad4[gi0], x0, y0, z0, w0);
    float n1 = (t1 < 0) ? 0.f : t1 * t1 * t1 * t1 * dot(grad4[gi1], x1, y1, z1, w1);
    float n2 = (t2 < 0) ? 0.f : t2 * t2 * t2 * t2 * dot(grad4[gi2], x2, y2, z2, w2);
    float n3 = (t3 < 0) ? 0.f : t3 * t3 * t3 * t3 * dot(grad4[gi3], x3, y3, z3, w3);
    float n4 = (t4 < 0) ? 0.f : t4 * t4 * t4 * t4 * dot(grad4[gi4], x4, y4, z4, w4);

    // Sum up and scale the result to cover the range [-1,1]
    return 27.f * (n0 + n1 + n2 + n3 + n4);
}

} // namespace rng
} // namespace kb