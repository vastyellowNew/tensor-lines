#include "TensorProductBezierTriangle1_3.hh"

#include "utils.hh"

namespace pev
{

template<typename T, typename C>
using TPBT1_3 = TensorProductBezierTriangle<T, C, 1, 3>;


template<typename T, typename C>
const typename TPBT1_3<T, C>::DomainPoints& TPBT1_3<T, C>::domainPoints()
{
    static const auto result = (DomainPoints{} <<
        1.,   0.,   0.,   1.,   0.,   0.,
        1.,   0.,   0., 2./3, 1./3,   0.,
        1.,   0.,   0., 2./3,   0., 1./3,
        1.,   0.,   0., 1./3, 2./3,   0.,
        1.,   0.,   0., 1./3, 1./3, 1./3,
        1.,   0.,   0., 1./3,   0., 2./3,
        1.,   0.,   0.,   0.,   1.,   0.,
        1.,   0.,   0.,   0., 2./3, 1./3,
        1.,   0.,   0.,   0., 1./3, 2./3,
        1.,   0.,   0.,   0.,   0.,   1.,
        0.,   1.,   0.,   1.,   0.,   0.,
        0.,   1.,   0., 2./3, 1./3,   0.,
        0.,   1.,   0., 2./3,   0., 1./3,
        0.,   1.,   0., 1./3, 2./3,   0.,
        0.,   1.,   0., 1./3, 1./3, 1./3,
        0.,   1.,   0., 1./3,   0., 2./3,
        0.,   1.,   0.,   0.,   1.,   0.,
        0.,   1.,   0.,   0., 2./3, 1./3,
        0.,   1.,   0.,   0., 1./3, 2./3,
        0.,   1.,   0.,   0.,   0.,   1.,
        0.,   0.,   1.,   1.,   0.,   0.,
        0.,   0.,   1., 2./3, 1./3,   0.,
        0.,   0.,   1., 2./3,   0., 1./3,
        0.,   0.,   1., 1./3, 2./3,   0.,
        0.,   0.,   1., 1./3, 1./3, 1./3,
        0.,   0.,   1., 1./3,   0., 2./3,
        0.,   0.,   1.,   0.,   1.,   0.,
        0.,   0.,   1.,   0., 2./3, 1./3,
        0.,   0.,   1.,   0., 1./3, 2./3,
        0.,   0.,   1.,   0.,   0.,   1.
    ).finished();
    return result;
}


template<typename T, typename C>
typename TPBT1_3<T, C>::Basis TPBT1_3<T, C>::makeBasis(const Coords& pos)
{
    return (Basis{} <<
            1 * pos[0] * pos[3] * pos[3] * pos[3],
            3 * pos[0] * pos[3] * pos[3] * pos[4],
            3 * pos[0] * pos[3] * pos[3] * pos[5],
            3 * pos[0] * pos[3] * pos[4] * pos[4],
            6 * pos[0] * pos[3] * pos[4] * pos[5],
            3 * pos[0] * pos[3] * pos[5] * pos[5],
            1 * pos[0] * pos[4] * pos[4] * pos[4],
            3 * pos[0] * pos[4] * pos[4] * pos[5],
            3 * pos[0] * pos[4] * pos[5] * pos[5],
            1 * pos[0] * pos[5] * pos[5] * pos[5],
            1 * pos[1] * pos[3] * pos[3] * pos[3],
            3 * pos[1] * pos[3] * pos[3] * pos[4],
            3 * pos[1] * pos[3] * pos[3] * pos[5],
            3 * pos[1] * pos[3] * pos[4] * pos[4],
            6 * pos[1] * pos[3] * pos[4] * pos[5],
            3 * pos[1] * pos[3] * pos[5] * pos[5],
            1 * pos[1] * pos[4] * pos[4] * pos[4],
            3 * pos[1] * pos[4] * pos[4] * pos[5],
            3 * pos[1] * pos[4] * pos[5] * pos[5],
            1 * pos[1] * pos[5] * pos[5] * pos[5],
            1 * pos[2] * pos[3] * pos[3] * pos[3],
            3 * pos[2] * pos[3] * pos[3] * pos[4],
            3 * pos[2] * pos[3] * pos[3] * pos[5],
            3 * pos[2] * pos[3] * pos[4] * pos[4],
            6 * pos[2] * pos[3] * pos[4] * pos[5],
            3 * pos[2] * pos[3] * pos[5] * pos[5],
            1 * pos[2] * pos[4] * pos[4] * pos[4],
            3 * pos[2] * pos[4] * pos[4] * pos[5],
            3 * pos[2] * pos[4] * pos[5] * pos[5],
            1 * pos[2] * pos[5] * pos[5] * pos[5]
            ).finished();
}


template<typename T, typename C>
template<int I, int D>
typename TPBT1_3<T, C>::Coeffs
TPBT1_3<T, C>::splitCoeffs(const Coeffs& in)
{
    static_assert(D >= 0 && D < 2, "Split dimension D must be 0 or 1");
    static_assert(I >= 0 && I < 4, "Subdivision index must be between 0 and 3");
    auto out = Coeffs{};
    if(D == 0 && I == 0)
    {
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        out[3] = in[3];
        out[4] = in[4];
        out[5] = in[5];
        out[6] = in[6];
        out[7] = in[7];
        out[8] = in[8];
        out[9] = in[9];
        out[10] = 1./2 * in[0] + 1./2 * in[10];
        out[11] = 1./2 * in[1] + 1./2 * in[11];
        out[12] = 1./2 * in[2] + 1./2 * in[12];
        out[13] = 1./2 * in[3] + 1./2 * in[13];
        out[14] = 1./2 * in[4] + 1./2 * in[14];
        out[15] = 1./2 * in[5] + 1./2 * in[15];
        out[16] = 1./2 * in[6] + 1./2 * in[16];
        out[17] = 1./2 * in[7] + 1./2 * in[17];
        out[18] = 1./2 * in[8] + 1./2 * in[18];
        out[19] = 1./2 * in[9] + 1./2 * in[19];
        out[20] = 1./2 * in[0] + 1./2 * in[20];
        out[21] = 1./2 * in[1] + 1./2 * in[21];
        out[22] = 1./2 * in[2] + 1./2 * in[22];
        out[23] = 1./2 * in[3] + 1./2 * in[23];
        out[24] = 1./2 * in[4] + 1./2 * in[24];
        out[25] = 1./2 * in[5] + 1./2 * in[25];
        out[26] = 1./2 * in[6] + 1./2 * in[26];
        out[27] = 1./2 * in[7] + 1./2 * in[27];
        out[28] = 1./2 * in[8] + 1./2 * in[28];
        out[29] = 1./2 * in[9] + 1./2 * in[29];
    }
    else if(D == 0 && I == 1)
    {
        out[0] = 1./2 * in[0] + 1./2 * in[10];
        out[1] = 1./2 * in[1] + 1./2 * in[11];
        out[2] = 1./2 * in[2] + 1./2 * in[12];
        out[3] = 1./2 * in[3] + 1./2 * in[13];
        out[4] = 1./2 * in[4] + 1./2 * in[14];
        out[5] = 1./2 * in[5] + 1./2 * in[15];
        out[6] = 1./2 * in[6] + 1./2 * in[16];
        out[7] = 1./2 * in[7] + 1./2 * in[17];
        out[8] = 1./2 * in[8] + 1./2 * in[18];
        out[9] = 1./2 * in[9] + 1./2 * in[19];
        out[10] = in[10];
        out[11] = in[11];
        out[12] = in[12];
        out[13] = in[13];
        out[14] = in[14];
        out[15] = in[15];
        out[16] = in[16];
        out[17] = in[17];
        out[18] = in[18];
        out[19] = in[19];
        out[20] = 1./2 * in[10] + 1./2 * in[20];
        out[21] = 1./2 * in[11] + 1./2 * in[21];
        out[22] = 1./2 * in[12] + 1./2 * in[22];
        out[23] = 1./2 * in[13] + 1./2 * in[23];
        out[24] = 1./2 * in[14] + 1./2 * in[24];
        out[25] = 1./2 * in[15] + 1./2 * in[25];
        out[26] = 1./2 * in[16] + 1./2 * in[26];
        out[27] = 1./2 * in[17] + 1./2 * in[27];
        out[28] = 1./2 * in[18] + 1./2 * in[28];
        out[29] = 1./2 * in[19] + 1./2 * in[29];
    }
    else if(D == 0 && I == 2)
    {
        out[0] = 1./2 * in[0] + 1./2 * in[20];
        out[1] = 1./2 * in[1] + 1./2 * in[21];
        out[2] = 1./2 * in[2] + 1./2 * in[22];
        out[3] = 1./2 * in[3] + 1./2 * in[23];
        out[4] = 1./2 * in[4] + 1./2 * in[24];
        out[5] = 1./2 * in[5] + 1./2 * in[25];
        out[6] = 1./2 * in[6] + 1./2 * in[26];
        out[7] = 1./2 * in[7] + 1./2 * in[27];
        out[8] = 1./2 * in[8] + 1./2 * in[28];
        out[9] = 1./2 * in[9] + 1./2 * in[29];
        out[10] = 1./2 * in[10] + 1./2 * in[20];
        out[11] = 1./2 * in[11] + 1./2 * in[21];
        out[12] = 1./2 * in[12] + 1./2 * in[22];
        out[13] = 1./2 * in[13] + 1./2 * in[23];
        out[14] = 1./2 * in[14] + 1./2 * in[24];
        out[15] = 1./2 * in[15] + 1./2 * in[25];
        out[16] = 1./2 * in[16] + 1./2 * in[26];
        out[17] = 1./2 * in[17] + 1./2 * in[27];
        out[18] = 1./2 * in[18] + 1./2 * in[28];
        out[19] = 1./2 * in[19] + 1./2 * in[29];
        out[20] = in[20];
        out[21] = in[21];
        out[22] = in[22];
        out[23] = in[23];
        out[24] = in[24];
        out[25] = in[25];
        out[26] = in[26];
        out[27] = in[27];
        out[28] = in[28];
        out[29] = in[29];
    }
    else if(D == 0 && I == 3)
    {
        out[0] = 1./2 * in[0] + 1./2 * in[10];
        out[1] = 1./2 * in[1] + 1./2 * in[11];
        out[2] = 1./2 * in[2] + 1./2 * in[12];
        out[3] = 1./2 * in[3] + 1./2 * in[13];
        out[4] = 1./2 * in[4] + 1./2 * in[14];
        out[5] = 1./2 * in[5] + 1./2 * in[15];
        out[6] = 1./2 * in[6] + 1./2 * in[16];
        out[7] = 1./2 * in[7] + 1./2 * in[17];
        out[8] = 1./2 * in[8] + 1./2 * in[18];
        out[9] = 1./2 * in[9] + 1./2 * in[19];
        out[10] = 1./2 * in[10] + 1./2 * in[20];
        out[11] = 1./2 * in[11] + 1./2 * in[21];
        out[12] = 1./2 * in[12] + 1./2 * in[22];
        out[13] = 1./2 * in[13] + 1./2 * in[23];
        out[14] = 1./2 * in[14] + 1./2 * in[24];
        out[15] = 1./2 * in[15] + 1./2 * in[25];
        out[16] = 1./2 * in[16] + 1./2 * in[26];
        out[17] = 1./2 * in[17] + 1./2 * in[27];
        out[18] = 1./2 * in[18] + 1./2 * in[28];
        out[19] = 1./2 * in[19] + 1./2 * in[29];
        out[20] = 1./2 * in[0] + 1./2 * in[20];
        out[21] = 1./2 * in[1] + 1./2 * in[21];
        out[22] = 1./2 * in[2] + 1./2 * in[22];
        out[23] = 1./2 * in[3] + 1./2 * in[23];
        out[24] = 1./2 * in[4] + 1./2 * in[24];
        out[25] = 1./2 * in[5] + 1./2 * in[25];
        out[26] = 1./2 * in[6] + 1./2 * in[26];
        out[27] = 1./2 * in[7] + 1./2 * in[27];
        out[28] = 1./2 * in[8] + 1./2 * in[28];
        out[29] = 1./2 * in[9] + 1./2 * in[29];
    }
    else if(D == 1 && I == 0)
    {
        out[0] = in[0];
        out[1] = 1./2 * in[0] + 1./2 * in[1];
        out[2] = 1./2 * in[0] + 1./2 * in[2];
        out[3] = 1./4 * in[0] + 1./2 * in[1] + 1./4 * in[3];
        out[4] = 1./4 * in[0] + 1./4 * in[1] + 1./4 * in[2] + 1./4 * in[4];
        out[5] = 1./4 * in[0] + 1./2 * in[2] + 1./4 * in[5];
        out[6] = 1./8 * in[0] + 3./8 * in[1] + 3./8 * in[3] + 1./8 * in[6];
        out[7] = 1./8 * in[0] + 1./4 * in[1] + 1./8 * in[2] + 1./8 * in[3]
                 + 1./4 * in[4] + 1./8 * in[7];
        out[8] = 1./8 * in[0] + 1./8 * in[1] + 1./4 * in[2] + 1./4 * in[4]
                 + 1./8 * in[5] + 1./8 * in[8];
        out[9] = 1./8 * in[0] + 3./8 * in[2] + 3./8 * in[5] + 1./8 * in[9];
        out[10] = in[10];
        out[11] = 1./2 * in[10] + 1./2 * in[11];
        out[12] = 1./2 * in[10] + 1./2 * in[12];
        out[13] = 1./4 * in[10] + 1./2 * in[11] + 1./4 * in[13];
        out[14] = 1./4 * in[10] + 1./4 * in[11] + 1./4 * in[12] + 1./4 * in[14];
        out[15] = 1./4 * in[10] + 1./2 * in[12] + 1./4 * in[15];
        out[16] = 1./8 * in[10] + 3./8 * in[11] + 3./8 * in[13] + 1./8 * in[16];
        out[17] = 1./8 * in[10] + 1./4 * in[11] + 1./8 * in[12] + 1./8 * in[13]
                  + 1./4 * in[14] + 1./8 * in[17];
        out[18] = 1./8 * in[10] + 1./8 * in[11] + 1./4 * in[12] + 1./4 * in[14]
                  + 1./8 * in[15] + 1./8 * in[18];
        out[19] = 1./8 * in[10] + 3./8 * in[12] + 3./8 * in[15] + 1./8 * in[19];
        out[20] = in[20];
        out[21] = 1./2 * in[20] + 1./2 * in[21];
        out[22] = 1./2 * in[20] + 1./2 * in[22];
        out[23] = 1./4 * in[20] + 1./2 * in[21] + 1./4 * in[23];
        out[24] = 1./4 * in[20] + 1./4 * in[21] + 1./4 * in[22] + 1./4 * in[24];
        out[25] = 1./4 * in[20] + 1./2 * in[22] + 1./4 * in[25];
        out[26] = 1./8 * in[20] + 3./8 * in[21] + 3./8 * in[23] + 1./8 * in[26];
        out[27] = 1./8 * in[20] + 1./4 * in[21] + 1./8 * in[22] + 1./8 * in[23]
                  + 1./4 * in[24] + 1./8 * in[27];
        out[28] = 1./8 * in[20] + 1./8 * in[21] + 1./4 * in[22] + 1./4 * in[24]
                  + 1./8 * in[25] + 1./8 * in[28];
        out[29] = 1./8 * in[20] + 3./8 * in[22] + 3./8 * in[25] + 1./8 * in[29];
    }
    else if(D == 1 && I == 1)
    {

        out[0] = 1./8 * in[0] + 3./8 * in[1] + 3./8 * in[3] + 1./8 * in[6];
        out[1] = 1./4 * in[1] + 1./2 * in[3] + 1./4 * in[6];
        out[2] = 1./8 * in[1] + 1./8 * in[2] + 1./4 * in[3] + 1./4 * in[4]
                 + 1./8 * in[6] + 1./8 * in[7];
        out[3] = 1./2 * in[3] + 1./2 * in[6];
        out[4] = 1./4 * in[3] + 1./4 * in[4] + 1./4 * in[6] + 1./4 * in[7];
        out[5] = 1./8 * in[3] + 1./4 * in[4] + 1./8 * in[5] + 1./8 * in[6]
                 + 1./4 * in[7] + 1./8 * in[8];
        out[6] = in[6];
        out[7] = 1./2 * in[6] + 1./2 * in[7];
        out[8] = 1./4 * in[6] + 1./2 * in[7] + 1./4 * in[8];
        out[9] = 1./8 * in[6] + 3./8 * in[7] + 3./8 * in[8] + 1./8 * in[9];
        out[10] = 1./8 * in[10] + 3./8 * in[11] + 3./8 * in[13] + 1./8 * in[16];
        out[11] = 1./4 * in[11] + 1./2 * in[13] + 1./4 * in[16];
        out[12] = 1./8 * in[11] + 1./8 * in[12] + 1./4 * in[13] + 1./4 * in[14]
                  + 1./8 * in[16] + 1./8 * in[17];
        out[13] = 1./2 * in[13] + 1./2 * in[16];
        out[14] = 1./4 * in[13] + 1./4 * in[14] + 1./4 * in[16] + 1./4 * in[17];
        out[15] = 1./8 * in[13] + 1./4 * in[14] + 1./8 * in[15] + 1./8 * in[16]
                  + 1./4 * in[17] + 1./8 * in[18];
        out[16] = in[16];
        out[17] = 1./2 * in[16] + 1./2 * in[17];
        out[18] = 1./4 * in[16] + 1./2 * in[17] + 1./4 * in[18];
        out[19] = 1./8 * in[16] + 3./8 * in[17] + 3./8 * in[18] + 1./8 * in[19];
        out[20] = 1./8 * in[20] + 3./8 * in[21] + 3./8 * in[23] + 1./8 * in[26];
        out[21] = 1./4 * in[21] + 1./2 * in[23] + 1./4 * in[26];
        out[22] = 1./8 * in[21] + 1./8 * in[22] + 1./4 * in[23] + 1./4 * in[24]
                  + 1./8 * in[26] + 1./8 * in[27];
        out[23] = 1./2 * in[23] + 1./2 * in[26];
        out[24] = 1./4 * in[23] + 1./4 * in[24] + 1./4 * in[26] + 1./4 * in[27];
        out[25] = 1./8 * in[23] + 1./4 * in[24] + 1./8 * in[25] + 1./8 * in[26]
                  + 1./4 * in[27] + 1./8 * in[28];
        out[26] = in[26];
        out[27] = 1./2 * in[26] + 1./2 * in[27];
        out[28] = 1./4 * in[26] + 1./2 * in[27] + 1./4 * in[28];
        out[29] = 1./8 * in[26] + 3./8 * in[27] + 3./8 * in[28] + 1./8 * in[29];
    }
    else if(D == 1 && I == 2)
    {
        out[0] = 1./8 * in[0] + 3./8 * in[2] + 3./8 * in[5] + 1./8 * in[9];
        out[1] = 1./8 * in[1] + 1./8 * in[2] + 1./4 * in[4] + 1./4 * in[5]
                 + 1./8 * in[8] + 1./8 * in[9];
        out[2] = 1./4 * in[2] + 1./2 * in[5] + 1./4 * in[9];
        out[3] = 1./8 * in[3] + 1./4 * in[4] + 1./8 * in[5] + 1./8 * in[7]
                 + 1./4 * in[8] + 1./8 * in[9];
        out[4] = 1./4 * in[4] + 1./4 * in[5] + 1./4 * in[8] + 1./4 * in[9];
        out[5] = 1./2 * in[5] + 1./2 * in[9];
        out[6] = 1./8 * in[6] + 3./8 * in[7] + 3./8 * in[8] + 1./8 * in[9];
        out[7] = 1./4 * in[7] + 1./2 * in[8] + 1./4 * in[9];
        out[8] = 1./2 * in[8] + 1./2 * in[9];
        out[9] = in[9];
        out[10] = 1./8 * in[10] + 3./8 * in[12] + 3./8 * in[15] + 1./8 * in[19];
        out[11] = 1./8 * in[11] + 1./8 * in[12] + 1./4 * in[14] + 1./4 * in[15]
                  + 1./8 * in[18] + 1./8 * in[19];
        out[12] = 1./4 * in[12] + 1./2 * in[15] + 1./4 * in[19];
        out[13] = 1./8 * in[13] + 1./4 * in[14] + 1./8 * in[15] + 1./8 * in[17]
                  + 1./4 * in[18] + 1./8 * in[19];
        out[14] = 1./4 * in[14] + 1./4 * in[15] + 1./4 * in[18] + 1./4 * in[19];
        out[15] = 1./2 * in[15] + 1./2 * in[19];
        out[16] = 1./8 * in[16] + 3./8 * in[17] + 3./8 * in[18] + 1./8 * in[19];
        out[17] = 1./4 * in[17] + 1./2 * in[18] + 1./4 * in[19];
        out[18] = 1./2 * in[18] + 1./2 * in[19];
        out[19] = in[19];
        out[20] = 1./8 * in[20] + 3./8 * in[22] + 3./8 * in[25] + 1./8 * in[29];
        out[21] = 1./8 * in[21] + 1./8 * in[22] + 1./4 * in[24] + 1./4 * in[25]
                  + 1./8 * in[28] + 1./8 * in[29];
        out[22] = 1./4 * in[22] + 1./2 * in[25] + 1./4 * in[29];
        out[23] = 1./8 * in[23] + 1./4 * in[24] + 1./8 * in[25] + 1./8 * in[27]
                  + 1./4 * in[28] + 1./8 * in[29];
        out[24] = 1./4 * in[24] + 1./4 * in[25] + 1./4 * in[28] + 1./4 * in[29];
        out[25] = 1./2 * in[25] + 1./2 * in[29];
        out[26] = 1./8 * in[26] + 3./8 * in[27] + 3./8 * in[28] + 1./8 * in[29];
        out[27] = 1./4 * in[27] + 1./2 * in[28] + 1./4 * in[29];
        out[28] = 1./2 * in[28] + 1./2 * in[29];
        out[29] = in[29];
    }
    else
    {
        out[0] = 1./8 * in[0] + 3./8 * in[1] + 3./8 * in[3] + 1./8 * in[6];
        out[1] = 1./8 * in[1] + 1./8 * in[2] + 1./4 * in[3] + 1./4 * in[4]
                 + 1./8 * in[6] + 1./8 * in[7];
        out[2] = 1./8 * in[0] + 1./4 * in[1] + 1./8 * in[2] + 1./8 * in[3]
                 + 1./4 * in[4] + 1./8 * in[7];
        out[3] = 1./8 * in[3] + 1./4 * in[4] + 1./8 * in[5] + 1./8 * in[6]
                 + 1./4 * in[7] + 1./8 * in[8];
        out[4] = 1./8 * in[1] + 1./8 * in[2] + 1./8 * in[3] + 1./4 * in[4]
                 + 1./8 * in[5] + 1./8 * in[7] + 1./8 * in[8];
        out[5] = 1./8 * in[0] + 1./8 * in[1] + 1./4 * in[2] + 1./4 * in[4]
                 + 1./8 * in[5] + 1./8 * in[8];
        out[6] = 1./8 * in[6] + 3./8 * in[7] + 3./8 * in[8] + 1./8 * in[9];
        out[7] = 1./8 * in[3] + 1./4 * in[4] + 1./8 * in[5] + 1./8 * in[7]
                 + 1./4 * in[8] + 1./8 * in[9];
        out[8] = 1./8 * in[1] + 1./8 * in[2] + 1./4 * in[4] + 1./4 * in[5]
                 + 1./8 * in[8] + 1./8 * in[9];
        out[9] = 1./8 * in[0] + 3./8 * in[2] + 3./8 * in[5] + 1./8 * in[9];
        out[10] = 1./8 * in[10] + 3./8 * in[11] + 3./8 * in[13] + 1./8 * in[16];
        out[11] = 1./8 * in[11] + 1./8 * in[12] + 1./4 * in[13] + 1./4 * in[14]
                  + 1./8 * in[16] + 1./8 * in[17];
        out[12] = 1./8 * in[10] + 1./4 * in[11] + 1./8 * in[12] + 1./8 * in[13]
                  + 1./4 * in[14] + 1./8 * in[17];
        out[13] = 1./8 * in[13] + 1./4 * in[14] + 1./8 * in[15] + 1./8 * in[16]
                  + 1./4 * in[17] + 1./8 * in[18];
        out[14] = 1./8 * in[11] + 1./8 * in[12] + 1./8 * in[13] + 1./4 * in[14]
                  + 1./8 * in[15] + 1./8 * in[17] + 1./8 * in[18];
        out[15] = 1./8 * in[10] + 1./8 * in[11] + 1./4 * in[12] + 1./4 * in[14]
                  + 1./8 * in[15] + 1./8 * in[18];
        out[16] = 1./8 * in[16] + 3./8 * in[17] + 3./8 * in[18] + 1./8 * in[19];
        out[17] = 1./8 * in[13] + 1./4 * in[14] + 1./8 * in[15] + 1./8 * in[17]
                  + 1./4 * in[18] + 1./8 * in[19];
        out[18] = 1./8 * in[11] + 1./8 * in[12] + 1./4 * in[14] + 1./4 * in[15]
                  + 1./8 * in[18] + 1./8 * in[19];
        out[19] = 1./8 * in[10] + 3./8 * in[12] + 3./8 * in[15] + 1./8 * in[19];
        out[20] = 1./8 * in[20] + 3./8 * in[21] + 3./8 * in[23] + 1./8 * in[26];
        out[21] = 1./8 * in[21] + 1./8 * in[22] + 1./4 * in[23] + 1./4 * in[24]
                  + 1./8 * in[26] + 1./8 * in[27];
        out[22] = 1./8 * in[20] + 1./4 * in[21] + 1./8 * in[22] + 1./8 * in[23]
                  + 1./4 * in[24] + 1./8 * in[27];
        out[23] = 1./8 * in[23] + 1./4 * in[24] + 1./8 * in[25] + 1./8 * in[26]
                  + 1./4 * in[27] + 1./8 * in[28];
        out[24] = 1./8 * in[21] + 1./8 * in[22] + 1./8 * in[23] + 1./4 * in[24]
                  + 1./8 * in[25] + 1./8 * in[27] + 1./8 * in[28];
        out[25] = 1./8 * in[20] + 1./8 * in[21] + 1./4 * in[22] + 1./4 * in[24]
                  + 1./8 * in[25] + 1./8 * in[28];
        out[26] = 1./8 * in[26] + 3./8 * in[27] + 3./8 * in[28] + 1./8 * in[29];
        out[27] = 1./8 * in[23] + 1./4 * in[24] + 1./8 * in[25] + 1./8 * in[27]
                  + 1./4 * in[28] + 1./8 * in[29];
        out[28] = 1./8 * in[21] + 1./8 * in[22] + 1./4 * in[24] + 1./4 * in[25]
                  + 1./8 * in[28] + 1./8 * in[29];
        out[29] = 1./8 * in[20] + 3./8 * in[22] + 3./8 * in[25] + 1./8 * in[29];
    }
    return out;
}

template<typename T, typename C>
typename TPBT1_3<T, C>::Coeffs
TPBT1_3<T, C>::computeCoeffs(const Coeffs& in)
{
    auto out = Coeffs{};
    out[0] = in[0];
    out[1] = -5./6 * in[0] + 3. * in[1] + -3./2 * in[3] + 1./3 * in[6];
    out[2] = -5./6 * in[0] + 3. * in[2] + -3./2 * in[5] + 1./3 * in[9];
    out[3] = 1./3 * in[0] + -3./2 * in[1] + 3. * in[3] + -5./6 * in[6];
    out[4] = 1./3 * in[0] + -3./4 * in[1] + -3./4 * in[2] + -3./4 * in[3]
             + 9./2 * in[4] + -3./4 * in[5] + 1./3 * in[6] + -3./4 * in[7]
             + -3./4 * in[8] + 1./3 * in[9];
    out[5] = 1./3 * in[0] + -3./2 * in[2] + 3. * in[5] + -5./6 * in[9];
    out[6] = in[6];
    out[7] = -5./6 * in[6] + 3. * in[7] + -3./2 * in[8] + 1./3 * in[9];
    out[8] = 1./3 * in[6] + -3./2 * in[7] + 3. * in[8] + -5./6 * in[9];
    out[9] = in[9];
    out[10] = in[10];
    out[11] = -5./6 * in[10] + 3. * in[11] + -3./2 * in[13] + 1./3 * in[16];
    out[12] = -5./6 * in[10] + 3. * in[12] + -3./2 * in[15] + 1./3 * in[19];
    out[13] = 1./3 * in[10] + -3./2 * in[11] + 3. * in[13] + -5./6 * in[16];
    out[14] = 1./3 * in[10] + -3./4 * in[11] + -3./4 * in[12] + -3./4 * in[13]
             + 9./2 * in[14] + -3./4 * in[15] + 1./3 * in[16] + -3./4 * in[17]
             + -3./4 * in[18] + 1./3 * in[19];
    out[15] = 1./3 * in[10] + -3./2 * in[12] + 3. * in[15] + -5./6 * in[19];
    out[16] = in[16];
    out[17] = -5./6 * in[16] + 3. * in[17] + -3./2 * in[18] + 1./3 * in[19];
    out[18] = 1./3 * in[16] + -3./2 * in[17] + 3. * in[18] + -5./6 * in[19];
    out[19] = in[19];
    out[20] = in[20];
    out[21] = -5./6 * in[20] + 3. * in[21] + -3./2 * in[23] + 1./3 * in[26];
    out[22] = -5./6 * in[20] + 3. * in[22] + -3./2 * in[25] + 1./3 * in[29];
    out[23] = 1./3 * in[20] + -3./2 * in[21] + 3. * in[23] + -5./6 * in[26];
    out[24] = 1./3 * in[20] + -3./4 * in[21] + -3./4 * in[22] + -3./4 * in[23]
             + 9./2 * in[24] + -3./4 * in[25] + 1./3 * in[26] + -3./4 * in[27]
             + -3./4 * in[28] + 1./3 * in[29];
    out[25] = 1./3 * in[20] + -3./2 * in[22] + 3. * in[25] + -5./6 * in[29];
    out[26] = in[26];
    out[27] = -5./6 * in[26] + 3. * in[27] + -3./2 * in[28] + 1./3 * in[29];
    out[28] = 1./3 * in[26] + -3./2 * in[27] + 3. * in[28] + -5./6 * in[29];
    out[29] = in[29];
    return out;
}

}