/* babl - dynamically extendable universal pixel conversion library.
 * Copyright (C) 2017 Øyvind Kolås.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#define MAX_SPACES   100

#include "config.h"
#include "babl-internal.h"
#include "base/util.h"

static BablSpace space_db[MAX_SPACES];

static void babl_chromatic_adaptation_matrix (const double *whitepoint,
                                              const double *target_whitepoint,
                                              double *chad_matrix)
{
  double bradford[9]={ 0.8951000, 0.2664000, -0.1614000,
                      -0.7502000, 1.7135000,  0.0367000,
                      0.0389000, -0.0685000, 1.0296000};
  double bradford_inv[9]={0.9869929,-0.1470543, 0.1599627,
                          0.4323053, 0.5183603, 0.0492912,
                          -0.0085287, 0.0400428, 0.9684867};

  double vec_a[3];
  double vec_b[3];

  babl_matrix_mul_vector (bradford, whitepoint, vec_a);
  babl_matrix_mul_vector (bradford, target_whitepoint, vec_b);

  memset (chad_matrix, 0, sizeof (double) * 9);

  chad_matrix[0] = vec_b[0] / vec_a[0];
  chad_matrix[4] = vec_b[1] / vec_a[1];
  chad_matrix[8] = vec_b[2] / vec_a[2];

  babl_matrix_mul_matrix (bradford_inv, chad_matrix, chad_matrix);
  babl_matrix_mul_matrix (chad_matrix, bradford, chad_matrix);
}

static void babl_space_compute_matrices (BablSpace *space)
{
#define _ space->
  /* transform spaces xy(Y) specified data to XYZ */
  double red_XYZ[3]        = { _ xr / _ yr, 1.0, ( 1.0 - _ xr - _ yr) / _ yr};
  double green_XYZ[3]      = { _ xg / _ yg, 1.0, ( 1.0 - _ xg - _ yg) / _ yg};
  double blue_XYZ[3]       = { _ xb / _ yb, 1.0, ( 1.0 - _ xb - _ yb) / _ yb};
  double whitepoint_XYZ[3] = { _ xw / _ yw, 1.0, ( 1.0 - _ xw - _ yw) / _ yw};
  double D50_XYZ[3]        = {0.9642,       1.0, 0.8249};
#undef _

  double mat[9] = {red_XYZ[0], green_XYZ[0], blue_XYZ[0],
                   red_XYZ[1], green_XYZ[1], blue_XYZ[1],
                   red_XYZ[2], green_XYZ[2], blue_XYZ[2]};
  double inv_mat[9];
  double S[3];
  double chad[9];

  babl_matrix_invert (mat, inv_mat);
  babl_matrix_mul_vector (inv_mat, whitepoint_XYZ, S);

  mat[0] *= S[0]; mat[1] *= S[1]; mat[2] *= S[2];
  mat[3] *= S[0]; mat[4] *= S[1]; mat[5] *= S[2];
  mat[6] *= S[0]; mat[7] *= S[1]; mat[8] *= S[2];

  babl_chromatic_adaptation_matrix (whitepoint_XYZ, D50_XYZ, chad);

  babl_matrix_mul_matrix (chad, mat, mat);

  memcpy (space->RGBtoXYZ, mat, sizeof (mat));
#if 0
  {
    int i;
    fprintf (stderr, "\n%s RGBtoXYZ:\n", space->name);
    for (i = 0; i < 9; i++)
    {
      fprintf (stderr, "%f ", mat[i]);
      if (i%3 == 2)
        fprintf (stderr, "\n");
    }
  }
#endif

  babl_matrix_invert (mat, mat);

#if 0
  {
    int i;
    fprintf (stderr, "\n%s XYZtoRGB:\n", space->name);
    for (i = 0; i < 9; i++)
    {
      fprintf (stderr, "%f ", mat[i]);
      if (i%3 == 2)
        fprintf (stderr, "\n");
    }
  }
#endif

  memcpy (space->XYZtoRGB, mat, sizeof (mat));
}

const Babl *
babl_space (const char *name)
{
  int i;
  for (i = 0; space_db[i].instance.class_type; i++)
    if (!strcmp (space_db[i].instance.name, name))
      return (Babl*)&space_db[i];
  return NULL;
}

const Babl *
babl_space_from_rgbxyz_matrix (const char *name,
                               double wx, double wy, double wz,
                               double rx, double gx, double bx,
                               double ry, double gy, double by,
                               double rz, double gz, double bz,
                               const Babl *trc_red,
                               const Babl *trc_green,
                               const Babl *trc_blue)
{
  int i=0;
  static BablSpace space;
  space.instance.class_type = BABL_SPACE;
  space.instance.id         = 0;

  space.xr = rx;
  space.yr = gx;
  space.xg = bx;
  space.yg = ry;
  space.xb = gy;
  space.yb = by;
  space.xw = rz;
  space.yw = gz;
  space.pad = bz;

  space.whitepoint[0] = wx;
  space.whitepoint[1] = wy;
  space.whitepoint[2] = wz;

  space.trc[0] = trc_red;
  space.trc[1] = trc_green?trc_green:trc_red;
  space.trc[2] = trc_blue?trc_blue:trc_red;

  for (i = 0; space_db[i].instance.class_type; i++)
  {
    int offset = ((char*)&space_db[i].xr) - (char*)(&space_db[i]);
    int size   = ((char*)&space_db[i].trc) + sizeof(space_db[i].trc) - ((char*)&space_db[i].xr);

    if (memcmp ((char*)(&space_db[i]) + offset, ((char*)&space) + offset, size)==0)
      {
        return (void*)&space_db[i];
      }
  }
  if (i >= MAX_SPACES-1)
  {
    babl_log ("too many BablSpaces");
    return NULL;
  }
  /* transplant matrixes */
  //babl_space_compute_matrices (&space_db[i]);
  space.RGBtoXYZ[0] = rx;
  space.RGBtoXYZ[1] = gx;
  space.RGBtoXYZ[2] = bx;
  space.RGBtoXYZ[3] = ry;
  space.RGBtoXYZ[4] = gy;
  space.RGBtoXYZ[5] = by;
  space.RGBtoXYZ[6] = rz;
  space.RGBtoXYZ[7] = gz;
  space.RGBtoXYZ[8] = bz;
  babl_matrix_invert (space.RGBtoXYZ, space.XYZtoRGB);

  babl_matrix_to_float (space.RGBtoXYZ, space.RGBtoXYZf);
  babl_matrix_to_float (space.XYZtoRGB, space.XYZtoRGBf);

  space_db[i]=space;
  space_db[i].instance.name = space_db[i].name;
  if (name)
    sprintf (space_db[i].name, "%s", name);
  else
    sprintf (space_db[i].name, "space-%.4f,%.4f_%.4f,%.4f_%.4f_%.4f,%.4f_%.4f,%.4f_%s,%s,%s",
                       rx, gx, bx,
                       ry, gy, by,
                       rz, gz, bz,
             babl_get_name (space.trc[0]),
             babl_get_name(space.trc[1]), babl_get_name(space.trc[2]));


  return (Babl*)&space_db[i];
}


const Babl *
babl_space_from_chromaticities (const char *name,
                                double wx, double wy,
                                double rx, double ry,
                                double gx, double gy,
                                double bx, double by,
                                const Babl *trc_red,
                                const Babl *trc_green,
                                const Babl *trc_blue)
{
  int i=0;
  static BablSpace space;
  space.instance.class_type = BABL_SPACE;
  space.instance.id         = 0;

  space.xr = rx;
  space.yr = ry;
  space.xg = gx;
  space.yg = gy;
  space.xb = bx;
  space.yb = by;
  space.xw = wx;
  space.yw = wy;
  space.trc[0] = trc_red;
  space.trc[1] = trc_green?trc_green:trc_red;
  space.trc[2] = trc_blue?trc_blue:trc_red;

  space.whitepoint[0] = wx / wy;
  space.whitepoint[1] = 1.0;
  space.whitepoint[2] = (1.0 - wx - wy) / wy;

  for (i = 0; space_db[i].instance.class_type; i++)
  {
    int offset = ((char*)&space_db[i].xr) - (char*)(&space_db[i]);
    int size   = ((char*)&space_db[i].trc) + sizeof(space_db[i].trc) - ((char*)&space_db[i].xr);

    if (memcmp ((char*)(&space_db[i]) + offset, ((char*)&space) + offset, size)==0)
      {
        return (void*)&space_db[i];
      }
  }
  if (i >= MAX_SPACES-1)
  {
    babl_log ("too many BablSpaces");
    return NULL;
  }
  space_db[i]=space;
  space_db[i].instance.name = space_db[i].name;
  if (name)
    sprintf (space_db[i].name, "%s", name);
  else
    sprintf (space_db[i].name, "space-%.4f,%.4f_%.4f,%.4f_%.4f,%.4f_%.4f,%.4f_%s,%s,%s",
             wx,wy,rx,ry,bx,by,gx,gy,babl_get_name (space.trc[0]),
             babl_get_name(space.trc[1]), babl_get_name(space.trc[2]));

  /* compute matrixes */
  babl_space_compute_matrices (&space_db[i]);

  return (Babl*)&space_db[i];
}

void
babl_space_class_for_each (BablEachFunction each_fun,
                           void            *user_data)
{
  int i=0;
  for (i = 0; space_db[i].instance.class_type; i++)
    if (each_fun (BABL (&space_db[i]), user_data))
      return;
}

void
babl_space_class_init (void)
{
  babl_space_from_chromaticities ("sRGB",
               0.3127,  0.3290, /* D65 */
               0.6400,  0.3300,
               0.3000,  0.6000,
               0.1500,  0.0600,
               babl_trc("sRGB"), NULL, NULL);

  babl_space_from_chromaticities (
      "Adobe",
      0.3127,  0.3290, /* D65 */
      0.6400,  0.3300,
      0.2100,  0.7100,
      0.1500,  0.0600,
      babl_trc("2.2"), NULL, NULL);

  babl_space_from_chromaticities (
      "ProPhoto",
      0.34567, 0.3585,  /* D50 */
      0.7347,  0.2653,
      0.1596,  0.8404,
      0.0366,  0.0001,
      babl_trc("1.8"), NULL, NULL);

  babl_space_from_chromaticities (
      "Apple",
      0.3127,  0.3290, /* D65 */
      0.6250,  0.3400,
      0.2800,  0.5950,
      0.1550,  0.0700,
      babl_trc("1.8"), NULL, NULL);

  babl_space_from_chromaticities (
     "WideGamutRGB",
     0.34567, 0.3585,  /* D50 */
     0.7350,  0.2650,
     0.1150,  0.8260,
     0.1570,  0.0180,
     babl_trc("2.2"), NULL, NULL);

#if 0
  babl_space_from_chromaticities (
      "Best",
      0.34567, 0.3585,  /* D50 */
      0.7347,  0.2653,
      0.2150,  0.7750,
      0.1300,  0.0350,
      babl_trc("2.2"), NULL, NULL);

  babl_space_from_chromaticities (
      "Beta",
      0.34567, 0.3585,  /* D50 */
      0.6888,  0.3112,
      0.1986,  0.7551,
      0.1265,  0.0352,
      babl_trc("2.2"), NULL, NULL);

  babl_space_from_chromaticities (
      "Bruce",
      0.3127,  0.3290, /* D65 */
      0.6400,  0.3300,
      0.2800,  0.6500,
      0.1500,  0.0600,
      babl_trc("1.8"), NULL, NULL);

  babl_space_from_chromaticities (
      "PAL",
      0.3127,  0.3290, /* D65 */
      0.6400,  0.3300,
      0.2900,  0.6000,
      0.1500,  0.0600,
      babl_trc("2.2"), NULL, NULL);

  babl_space_from_chromaticities (
      "SMPTE-C",
      0.3127,  0.3290, /* D65 */
      0.6300,  0.3300,
      0.3100,  0.5950,
      0.1550,  0.0700,
      babl_trc("2.2"), NULL, NULL);

  babl_space_from_chromaticities (
      "ColorMatch",
      0.34567, 0.3585,  /* D50 */
      0.6300,  0.3400,
      0.2950,  0.6050,
      0.1500,  0.0750,
      babl_trc("1.8"), NULL, NULL);

  babl_space_from_chromaticities (
     "Don RGB 4",
     0.34567, 0.3585,  /* D50 */
     0.6960,  0.3000,
     0.2150,  0.7650,
     0.1300,  0.0350,
     babl_trc("1.8"), NULL, NULL);
#endif
}

void babl_space_to_xyz (const Babl *space, const double *rgb, double *xyz)
{
  _babl_space_to_xyz (space, rgb, xyz);
}

void babl_space_from_xyz (const Babl *space, const double *xyz, double *rgb)
{
  _babl_space_from_xyz (space, xyz, rgb);
}

const double * babl_space_get_rgbtoxyz (const Babl *space)
{
  return space->space.RGBtoXYZ;
}

///////////////////


static void prep_conversion (const Babl *babl)
{
  Babl *conversion = (void*) babl;
  const Babl *source_space = babl_conversion_get_source_space (conversion);
  float *matrixf;
  int i;
  float *lut;

  double matrix[9];
  babl_matrix_mul_matrix (
     (conversion->conversion.destination)->format.space->space.XYZtoRGB,
     (conversion->conversion.source)->format.space->space.RGBtoXYZ,
     matrix);

  matrixf = babl_calloc (sizeof (float), 9 + 256); // we leak this matrix , which is a singleton
  babl_matrix_to_float (matrix, matrixf);
  conversion->conversion.data = matrixf;

  lut = matrixf + 9;
  for (i = 0; i < 256; i++)
  {
    lut[i] = babl_trc_to_linear (source_space->space.trc[0], i/255.0);
    // XXX: should have green and blue luts as well
  }
}

#define TRC_IN(rgba_in, rgba_out)  do{ int i;\
  for (i = 0; i < samples; i++) \
  { \
    rgba_out[i*4+3] = rgba_in[i*4+3]; \
  } \
  if ((source_space->space.trc[0] == source_space->space.trc[1]) && \
      (source_space->space.trc[1] == source_space->space.trc[2])) \
  { \
    const Babl *trc = (void*)source_space->space.trc[0]; \
    babl_trc_to_linear_buf(trc, rgba_in, rgba_out, 4, 4, 3, samples); \
  } \
  else \
  { \
    int c; \
    for (c = 0; c < 3; c ++) \
    { \
      const Babl *trc = (void*)source_space->space.trc[c]; \
      babl_trc_to_linear_buf(trc, rgba_in + c, rgba_out + c, 4, 4, 1, samples); \
    } \
  } \
}while(0)

#define TRC_OUT(rgba_in, rgba_out)  do{\
  { \
    int c; \
    if ((destination_space->space.trc[0] == destination_space->space.trc[1]) && \
        (destination_space->space.trc[1] == destination_space->space.trc[2])) \
    { \
      const Babl *trc = (void*)destination_space->space.trc[0]; \
      babl_trc_from_linear_buf(trc, rgba_in, rgba_out, 4, 4, 3, samples); \
    } \
    else \
    { \
      for (c = 0; c < 3; c ++) \
      { \
        const Babl *trc = (void*)destination_space->space.trc[c]; \
        babl_trc_from_linear_buf(trc, rgba_in + c, rgba_out + c, 4, 4, 1, samples); \
      } \
    } \
  }\
} while(0)




static inline long
universal_nonlinear_rgb_converter (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  const Babl *source_space = babl_conversion_get_source_space (conversion);
  const Babl *destination_space = babl_conversion_get_destination_space (conversion);

  float * matrixf = conversion->conversion.data;
  float *rgba_in = (void*)src_char;
  float *rgba_out = (void*)dst_char;

  TRC_IN(rgba_in, rgba_out);

  babl_matrix_mul_vectorff_buf4 (matrixf, rgba_out, rgba_out, samples);

  TRC_OUT(rgba_out, rgba_out);

  return samples;
}

static inline long
universal_nonlinear_rgb_linear_converter (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  const Babl *source_space = babl_conversion_get_source_space (conversion);
  float * matrixf = conversion->conversion.data;
  float *rgba_in = (void*)src_char;
  float *rgba_out = (void*)dst_char;

  TRC_IN(rgba_in, rgba_out);

  babl_matrix_mul_vectorff_buf4 (matrixf, rgba_out, rgba_out, samples);

  return samples;
}

static inline long
universal_nonlinear_rgba_u8_converter (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  const Babl *destination_space = conversion->conversion.destination->format.space;

  float * matrixf = conversion->conversion.data;
  float * in_trc_lut = matrixf + 9;
  int i;
  uint8_t *rgba_in_u8 = (void*)src_char;
  uint8_t *rgba_out_u8 = (void*)dst_char;

  float *rgb = babl_malloc (sizeof(float) * 4 * samples);

  for (i = 0; i < samples; i++)
  {
    rgb[i*4+0]=in_trc_lut[rgba_in_u8[i*4+0]];
    rgb[i*4+1]=in_trc_lut[rgba_in_u8[i*4+1]];
    rgb[i*4+2]=in_trc_lut[rgba_in_u8[i*4+2]];
    rgba_out_u8[i*4+3] = rgba_in_u8[i*4+3];
  }

  babl_matrix_mul_vectorff_buf4 (matrixf, rgb, rgb, samples);

  {
    const Babl *from_trc_red   = (void*)destination_space->space.trc[0];
    const Babl *from_trc_green = (void*)destination_space->space.trc[1];
    const Babl *from_trc_blue  = (void*)destination_space->space.trc[2];
    for (i = 0; i < samples * 4; i+=4)
    {
      rgba_out_u8[i+0] = babl_trc_from_linear (from_trc_red,   rgb[i+0]) * 255.5f;
      rgba_out_u8[i+1] = babl_trc_from_linear (from_trc_green, rgb[i+1]) * 255.5f;
      rgba_out_u8[i+2] = babl_trc_from_linear (from_trc_blue,  rgb[i+2]) * 255.5f;
    }
  }
  babl_free (rgb);

  return samples;
}


static inline long
universal_rgba_converter (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  float *matrixf = conversion->conversion.data;
  float *rgba_in = (void*)src_char;
  float *rgba_out = (void*)dst_char;

  babl_matrix_mul_vectorff_buf4 (matrixf, rgba_in, rgba_out, samples);

  return samples;
}

static inline long
universal_rgb_converter (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  float *matrixf = conversion->conversion.data;
  float *rgb_in = (void*)src_char;
  float *rgb_out = (void*)dst_char;

  babl_matrix_mul_vectorff_buf3 (matrixf, rgb_in, rgb_out, samples);

  return samples;
}

#if defined(USE_SSE2)

#define m(matr, j, i)  matr[j*3+i]

#include <emmintrin.h>

static inline void babl_matrix_mul_vectorff_buf4_sse2 (const float *mat,
                                                       const float *v_in,
                                                       float *v_out,
                                                       int samples)
{
  const __v4sf m___0 = {m(mat, 0, 0), m(mat, 1, 0), m(mat, 2, 0), 0};
  const __v4sf m___1 = {m(mat, 0, 1), m(mat, 1, 1), m(mat, 2, 1), 0};
  const __v4sf m___2 = {m(mat, 0, 2), m(mat, 1, 2), m(mat, 2, 2), 1};
  int i;
  for (i = 0; i < samples; i ++)
  {
    __v4sf a, b, c = _mm_load_ps(&v_in[0]);
    a = (__v4sf) _mm_shuffle_epi32((__m128i)c, _MM_SHUFFLE(0,0,0,0));
    b = (__v4sf) _mm_shuffle_epi32((__m128i)c, _MM_SHUFFLE(1,1,1,1));
    c = (__v4sf) _mm_shuffle_epi32((__m128i)c, _MM_SHUFFLE(3,2,2,2));
    _mm_store_ps (v_out, m___0 * a + m___1 * b + m___2 * c);
    v_out += 4;
    v_in  += 4;
  }
  _mm_empty ();
}

#undef m


static inline long
universal_nonlinear_rgb_converter_sse2 (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  const Babl *source_space = babl_conversion_get_source_space (conversion);
  const Babl *destination_space = babl_conversion_get_destination_space (conversion);
  float * matrixf = conversion->conversion.data;
  float *rgba_in = (void*)src_char;
  float *rgba_out = (void*)dst_char;

  TRC_IN(rgba_in, rgba_out);

  babl_matrix_mul_vectorff_buf4_sse2 (matrixf, rgba_out, rgba_out, samples);

  TRC_OUT(rgba_out, rgba_out);

  return samples;
}


static inline long
universal_rgba_converter_sse2 (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  float *matrixf = conversion->conversion.data;
  float *rgba_in = (void*)src_char;
  float *rgba_out = (void*)dst_char;

  babl_matrix_mul_vectorff_buf4_sse2 (matrixf, rgba_in, rgba_out, samples);

  return samples;
}

static inline long
universal_nonlinear_rgba_u8_converter_sse2 (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  const Babl *destination_space = conversion->conversion.destination->format.space;

  float * matrixf = conversion->conversion.data;
  float * in_trc_lut = matrixf + 9;
  int i;
  uint8_t *rgba_in_u8 = (void*)src_char;
  uint8_t *rgba_out_u8 = (void*)dst_char;

  float *rgba_out = babl_malloc (sizeof(float) * 4 * samples);
  static float linear_lut_u8[258]={-1.0,};

  if (linear_lut_u8[0] < 0)
  {
    for (i = 0; i < 256; i++)
      linear_lut_u8[i] = i / 255.0;
  }

  for (i = 0; i < samples * 4; i+= 4)
  {
    rgba_out[i+0]=in_trc_lut[rgba_in_u8[i+0]];
    rgba_out[i+1]=in_trc_lut[rgba_in_u8[i+1]];
    rgba_out[i+2]=in_trc_lut[rgba_in_u8[i+2]];
    rgba_out_u8[i+3] = rgba_in_u8[i+3];
  }

  babl_matrix_mul_vectorff_buf4_sse2 (matrixf, rgba_out, rgba_out, samples);

  {
    int c;
    TRC_OUT(rgba_out, rgba_out);

    for (i = 0; i < samples * 4; i+= 4)
      for (c = 0; c < 3; c ++)
        rgba_out_u8[i+c] = rgba_out[i+c] * 255.5f;
  }

  babl_free (rgba_out);

  return samples;
}

static inline long
universal_nonlinear_rgb_linear_converter_sse2 (const Babl *conversion,unsigned char *src_char, unsigned char *dst_char, long samples)
{
  const Babl *source_space = babl_conversion_get_source_space (conversion);
  float * matrixf = conversion->conversion.data;
  float *rgba_in = (void*)src_char;
  float *rgba_out = (void*)dst_char;

  TRC_IN(rgba_in, rgba_out);

  babl_matrix_mul_vectorff_buf4_sse2 (matrixf, rgba_out, rgba_out, samples);

  return samples;
}
#endif


static int
add_rgb_adapter (Babl *babl,
                 void *space)
{
  if (babl != space)
  {

#if defined(USE_SSE2)
    if ((babl_cpu_accel_get_support () & BABL_CPU_ACCEL_X86_SSE) &&
        (babl_cpu_accel_get_support () & BABL_CPU_ACCEL_X86_SSE2))
    {
       prep_conversion(babl_conversion_new(babl_format_with_space("RGBA float", space),
                       babl_format_with_space("RGBA float", babl),
                       "linear", universal_rgba_converter_sse2,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("RGBA float", babl),
                       babl_format_with_space("RGBA float", space),
                       "linear", universal_rgba_converter_sse2,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A float", space),
                       babl_format_with_space("R'G'B'A float", babl),
                       "linear", universal_nonlinear_rgb_converter_sse2,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A float", babl),
                       babl_format_with_space("R'G'B'A float", space),
                       "linear", universal_nonlinear_rgb_converter_sse2,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A u8", space),
                       babl_format_with_space("R'G'B'A u8", babl),
                       "linear", universal_nonlinear_rgba_u8_converter_sse2,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A u8", babl),
                       babl_format_with_space("R'G'B'A u8", space),
                       "linear", universal_nonlinear_rgba_u8_converter_sse2,
                       NULL));
    }
    else
#endif
    {
       prep_conversion(babl_conversion_new(babl_format_with_space("RGBA float", space),
                       babl_format_with_space("RGBA float", babl),
                       "linear", universal_rgba_converter,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("RGBA float", babl),
                       babl_format_with_space("RGBA float", space),
                       "linear", universal_rgba_converter,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A float", space),
                       babl_format_with_space("R'G'B'A float", babl),
                       "linear", universal_nonlinear_rgb_converter,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A float", babl),
                       babl_format_with_space("R'G'B'A float", space),
                       "linear", universal_nonlinear_rgb_converter,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A u8", space),
                       babl_format_with_space("R'G'B'A u8", babl),
                       "linear", universal_nonlinear_rgba_u8_converter,
                       NULL));
       prep_conversion(babl_conversion_new(babl_format_with_space("R'G'B'A u8", babl),
                       babl_format_with_space("R'G'B'A u8", space),
                       "linear", universal_nonlinear_rgba_u8_converter,
                       NULL));
    }

    prep_conversion(babl_conversion_new(babl_format_with_space("RGB float", space),
                    babl_format_with_space("RGB float", babl),
                    "linear", universal_rgb_converter,
                    NULL));
    prep_conversion(babl_conversion_new(babl_format_with_space("RGB float", babl),
                    babl_format_with_space("RGB float", space),
                    "linear", universal_rgb_converter,
                    NULL));
  }
  return 0;
}

/* The first time a new Babl space is used - for creation of a fish, is when
 * this function is called, it adds conversions hooks that provides its formats
 * with conversions internally as well as for conversions to and from other RGB
 * spaces.
 */
void _babl_space_add_universal_rgb (const Babl *space)
{
  babl_space_class_for_each (add_rgb_adapter, (void*)space);
}


const Babl *babl_space_match_trc_matrix (const Babl *trc_red,
                                         const Babl *trc_green,
                                         const Babl *trc_blue,
                                         float rx, float ry, float rz,
                                         float gx, float gy, float gz,
                                         float bx, float by, float bz)
{
  // XXX: extend to iteratre through registered spaces and finding
  //      already registered ones that are close enough duplicates
  //      first registered space (thus also internal babl ones) wins.
  //      this makes using babl to get icc meta data difficult, perhaps
  //      the icc meta data should be passed out-of-band?

  if (trc_red == babl_trc ("sRGB") &&
      trc_green == babl_trc ("sRGB") &&
      trc_blue == babl_trc ("sRGB") &&
      fabs(rx - 0.436042) < 0.001 &&
      fabs(ry - 0.222492) < 0.001 &&
      fabs(rz - 0.013916) < 0.001 &&
      fabs(gx - 0.385122) < 0.001 &&
      fabs(gy - 0.716915) < 0.001 &&
      fabs(gz - 0.097063) < 0.001 &&
      fabs(bx - 0.143053) < 0.001 &&
      fabs(by - 0.060609) < 0.001 &&
      fabs(bz - 0.713939) < 0.001)
   {
     return babl_space ("sRGB");
   }
  return NULL;
}
