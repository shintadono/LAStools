/*
===============================================================================

  FILE:  lasreader_asc.cpp

  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    info@rapidlasso.de  -  https://rapidlasso.de

  COPYRIGHT:

    (c) 2007-2014, rapidlasso GmbH - fast tools to catch reality

    This is free software; you can redistribute and/or modify it under the
    terms of the GNU Lesser General Licence as published by the Free Software
    Foundation. See the LICENSE.txt file for more information.

    This software is distributed WITHOUT ANY WARRANTY and without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "lasreader_asc.hpp"

#include "lasmessage.hpp"
#include "lasvlrpayload.hpp"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" FILE * fopen_compressed(const char* filename, const char* mode, bool* piped);

BOOL LASreaderASC::open(const CHAR* file_name, BOOL comma_not_point)
{
  if (file_name == 0)
  {
    laserror("file name pointer is zero");
    return FALSE;
  }

  clean();
  this->comma_not_point = comma_not_point;

  file = fopen_compressed(file_name, "r", &piped);
  if (file == 0)
  {
    laserror("cannot open file '%s'", file_name);
    return FALSE;
  }

  if (setvbuf(file, NULL, _IOFBF, 10 * LAS_TOOLS_IO_IBUFFER_SIZE) != 0)
  {
    LASMessage(LAS_WARNING, "setvbuf() failed with buffer size %d", 10 * LAS_TOOLS_IO_IBUFFER_SIZE);
  }

  // clean the header

  header.clean();

  // populate the header as much as it makes sense

  snprintf(header.system_identifier, LAS_HEADER_CHAR_LEN, LAS_TOOLS_COPYRIGHT);
  snprintf(header.generating_software, LAS_HEADER_CHAR_LEN, "via LASreaderASC (%d)", LAS_TOOLS_VERSION);

  // maybe set creation date

#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA attr;
  SYSTEMTIME creation;
  GetFileAttributesEx(file_name, GetFileExInfoStandard, &attr);
  FileTimeToSystemTime(&attr.ftCreationTime, &creation);
  int startday[13] = { -1, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  header.file_creation_day = startday[creation.wMonth] + creation.wDay;
  header.file_creation_year = creation.wYear;
  // leap year handling
  if ((((creation.wYear) % 4) == 0) && (creation.wMonth > 2)) header.file_creation_day++;
#else
  header.file_creation_day = 333;
  header.file_creation_year = 2012;
#endif

  header.point_data_format = 0;
  header.point_data_record_length = 20;

  // initialize point

  point.init(&header, header.point_data_format, header.point_data_record_length, &header);

  // read header of ASC file

  if (line == 0)
  {
    line_size = 1024;
    line = (CHAR*)malloc(sizeof(CHAR) * line_size);
  }

  CHAR dummy[32];
  BOOL complete = FALSE;
  ncols = 0;
  nrows = 0;
  F64 xllcorner = F64_MAX;
  F64 yllcorner = F64_MAX;
  xllcenter = F64_MAX;
  xllcenter = F64_MAX;
  cellsize = 0;
  nodata = -9999;
  header_lines = 0;

#pragma warning(push)
#pragma warning(disable : 6387)

  while (!complete)
  {
    if (!fgets(line, line_size, file)) break;

    // special handling for European numbers

    if (comma_not_point)
    {
      I32 i, len = (I32)strlen(line);
      for (i = 0; i < len; i++)
      {
        if (line[i] == ',') line[i] = '.';
      }
    }

    if (strstr(line, "ncols") || strstr(line, "NCOLS"))
    {
      sscanf_las(line, "%s %d", dummy, &ncols);
      free(line);
      line_size = 1024 + 50 * ncols;
      line = (CHAR*)malloc(sizeof(CHAR) * line_size);
    }
#pragma warning(pop)
    else if (strstr(line, "nrows") || strstr(line, "NROWS"))
    {
      sscanf_las(line, "%s %d", dummy, &nrows);
    }
    else if (strstr(line, "xllcorner") || strstr(line, "XLLCORNER"))
    {
      sscanf_las(line, "%s %lf", dummy, &xllcorner);
    }
    else if (strstr(line, "yllcorner") || strstr(line, "YLLCORNER"))
    {
      sscanf_las(line, "%s %lf", dummy, &yllcorner);
    }
    else if (strstr(line, "xllcenter") || strstr(line, "XLLCENTER"))
    {
      sscanf_las(line, "%s %lf", dummy, &xllcenter);
    }
    else if (strstr(line, "yllcenter") || strstr(line, "YLLCENTER"))
    {
      sscanf_las(line, "%s %lf", dummy, &yllcenter);
    }
    else if (strstr(line, "cellsize") || strstr(line, "CELLSIZE"))
    {
      sscanf_las(line, "%s %f", dummy, &cellsize);
    }
    else if (strstr(line, "nodata_value") || strstr(line, "NODATA_VALUE") || strstr(line, "nodata_VALUE") || strstr(line, "NODATA_value"))
    {
      sscanf_las(line, "%s %f", dummy, &nodata);
    }
    else if ((ncols != 0) && (nrows != 0) && (((xllcorner != F64_MAX) && (yllcorner != F64_MAX)) || ((xllcenter != F64_MAX) && (yllcenter != F64_MAX))) && (cellsize > 0))
    {
      if (ncols == 1)
      {
        F32 e0, e1;
        if (sscanf(line, "%f %f", &e0, &e1) == 1)
        {
          complete = TRUE;
        }
      }
      else if (ncols == 2)
      {
        F32 e0, e1, e2;
        if (sscanf(line, "%f %f %f", &e0, &e1, &e2) == 2)
        {
          complete = TRUE;
        }
      }
      else if (ncols == 3)
      {
        F32 e0, e1, e2, e3;
        if (sscanf(line, "%f %f %f %f", &e0, &e1, &e2, &e3) == 3)
        {
          complete = TRUE;
        }
      }
      else if (ncols == 4)
      {
        F32 e0, e1, e2, e3, e4;
        if (sscanf(line, "%f %f %f %f %f", &e0, &e1, &e2, &e3, &e4) == 4)
        {
          complete = TRUE;
        }
      }
      else
      {
        F32 e0, e1, e2, e3, e4;
        if (sscanf(line, "%f %f %f %f %f", &e0, &e1, &e2, &e3, &e4) == 5)
        {
          complete = TRUE;
        }
      }
    }
    header_lines++;
  }

  if (!complete)
  {
    laserror("was not able to find header");
    return FALSE;
  }

  // shift the llcorner to the pixel center

  if ((xllcorner != F64_MAX) && (yllcorner != F64_MAX))
  {
    xllcenter = xllcorner + 0.5 * cellsize;
    yllcenter = yllcorner + 0.5 * cellsize;
  }

  // init the bounding box x y

  header.min_x = xllcenter;
  header.min_y = yllcenter;
  header.max_x = xllcenter + (ncols - 1) * cellsize;
  header.max_y = yllcenter + (nrows - 1) * cellsize;

  // init the bounding box z and count the rasters

  F64 elevation = 0;
  npoints = 0;
  header.min_z = F64_MAX;
  header.max_z = F64_MIN;

  // skip leading spaces
  line_curr = 0;
  while ((line[line_curr] != '\0') && (line[line_curr] <= ' ')) line_curr++;

  for (row = 0; row < nrows; row++)
  {
    for (col = 0; col < ncols; col++)
    {
      if (line[line_curr] == '\0')
      {
        if (!fgets(line, line_size, file))
        {
          LASMessage(LAS_WARNING, "end-of-file after %d of %d rows and %d of %d cols. read %lld points", row, nrows, col, ncols, p_idx);
        }

        // special handling for European numbers

        if (comma_not_point)
        {
          I32 i, len = (I32)strlen(line);
          for (i = 0; i < len; i++)
          {
            if (line[i] == ',') line[i] = '.';
          }
        }

        // skip leading spaces
        line_curr = 0;
        while ((line[line_curr] != '\0') && (line[line_curr] <= ' ')) line_curr++;
      }
      // get elevation value
      sscanf_las(&(line[line_curr]), "%lf", &elevation);
      // skip parsed number
      while ((line[line_curr] != '\0') && (line[line_curr] > ' ')) line_curr++;
      // skip following spaces
      while ((line[line_curr] != '\0') && (line[line_curr] <= ' ')) line_curr++;
      // should we use the raster
      if (elevation != nodata)
      {
        npoints++;
        if (header.max_z < elevation) header.max_z = elevation;
        if (header.min_z > elevation) header.min_z = elevation;
      }
    }
  }

  // close the ASC file

  close();

  // check the header values

  header.number_of_point_records = (U32)npoints;

  if (npoints)
  {
    // populate scale and offset

    populate_scale_and_offset();

    // check bounding box for this scale and offset

    populate_bounding_box();
  }
  else
  {
    LASMessage(LAS_WARNING, "ASC raster contains only no data values");
    header.min_z = 0;
    header.max_z = 0;
  }

  // add the VLR for Raster LAZ 

  LASvlrRasterLAZ vlrRasterLAZ;
  vlrRasterLAZ.nbands = 1;
  vlrRasterLAZ.nbits = 32;
  vlrRasterLAZ.ncols = ncols;
  vlrRasterLAZ.nrows = nrows;
  vlrRasterLAZ.reserved1 = 0;
  vlrRasterLAZ.reserved2 = 0;
  vlrRasterLAZ.stepx = cellsize;
  vlrRasterLAZ.stepx_y = 0.0;
  vlrRasterLAZ.stepy = cellsize;
  vlrRasterLAZ.stepy_x = 0.0;
  vlrRasterLAZ.llx = xllcenter - 0.5 * cellsize;
  vlrRasterLAZ.lly = yllcenter - 0.5 * cellsize;
  vlrRasterLAZ.sigmaxy = 0.0;

  header.add_vlr("Raster LAZ", 7113, (U16)vlrRasterLAZ.get_payload_size(), vlrRasterLAZ.get_payload(), FALSE, "by LAStools of rapidlasso GmbH", FALSE);

  // reopen

  return reopen(file_name);
}

void LASreaderASC::set_scale_factor(const F64* scale_factor)
{
  if (scale_factor)
  {
    if (this->scale_factor == 0) this->scale_factor = new F64[3];
    this->scale_factor[0] = scale_factor[0];
    this->scale_factor[1] = scale_factor[1];
    this->scale_factor[2] = scale_factor[2];
  }
  else if (this->scale_factor)
  {
    delete[] this->scale_factor;
    this->scale_factor = 0;
  }
}

void LASreaderASC::set_offset(const F64* offset)
{
  if (offset)
  {
    if (this->offset == 0) this->offset = new F64[3];
    this->offset[0] = offset[0];
    this->offset[1] = offset[1];
    this->offset[2] = offset[2];
  }
  else if (this->offset)
  {
    delete[] this->offset;
    this->offset = 0;
  }
}

BOOL LASreaderASC::seek(const I64 p_index)
{
  return FALSE;
}

BOOL LASreaderASC::read_point_default()
{
  F64 elevation;
  while (p_idx < npoints)
  {
    if (line[line_curr] == '\0')
    {
      if (!fgets(line, line_size, file))
      {
        LASMessage(LAS_WARNING, "end-of-file after %d of %d rows and %d of %d cols. read %lld points", row, nrows, col, ncols, p_idx);
        npoints = p_idx;
        return FALSE;
      }

      // special handling for European numbers

      if (comma_not_point)
      {
        I32 i, len = (I32)strlen(line);
        for (i = 0; i < len; i++)
        {
          if (line[i] == ',') line[i] = '.';
        }
      }
      line_curr = 0;
      // skip leading spaces
      while ((line[line_curr] != '\0') && (line[line_curr] <= ' ')) line_curr++;
    }
    if (col == ncols)
    {
      col = 0;
      row++;
    }
    // get elevation value
    sscanf_las(&(line[line_curr]), "%lf", &elevation);
    // skip parsed number
    while ((line[line_curr] != '\0') && (line[line_curr] > ' ')) line_curr++;
    // skip following spaces
    while ((line[line_curr] != '\0') && (line[line_curr] <= ' ')) line_curr++;
    // should we use the raster
    if (elevation != nodata)
    {
      F64 x = xllcenter + col * cellsize;
      F64 y = yllcenter + (nrows - row - 1) * cellsize;
      F64 z = elevation; 

      if (opener->is_offset_adjust() == FALSE)
      {
        // compute the quantized x, y, and z values
        if (!point.set_x(x))
        {
          overflow_I32_x++;
        }
        if (!point.set_y(y))
        {
          overflow_I32_y++;
        }
        if (!point.set_z(z))
        {
          overflow_I32_z++;
        }
      }
      else 
      {
        I64 X = 0;
        I64 Y = 0;
        I64 Z = 0;

        if (x >= orig_x_offset)
          X = ((I64)((x / orig_x_scale_factor) + 0.5));
        else
          X = ((I64)((x / orig_x_scale_factor) - 0.5));
        if (y >= orig_y_offset)
          Y = ((I64)(((y - orig_y_offset) / orig_y_scale_factor) + 0.5));
        else
          Y = ((I64)(((y - orig_y_offset) / orig_y_scale_factor) - 0.5));
        if (z >= orig_z_offset)
          Z = ((I64)(((z - orig_z_offset) / orig_z_scale_factor) + 0.5));
        else
          Z = ((I64)(((z - orig_z_offset) / orig_z_scale_factor) - 0.5));

        if (I32_FITS_IN_RANGE(X))
          point.set_X(X);
        else
          overflow_I32_x++;
        if (I32_FITS_IN_RANGE(Y))
          point.set_Y(Y);
        else
          overflow_I32_y++;
        if (I32_FITS_IN_RANGE(Z))
          point.set_Z(Z);
        else
          overflow_I32_z++;
      }
      p_idx++;
      p_cnt++;
      col++;
      return TRUE;
    }
    else
    {
      col++;
    }
  }
  return FALSE;
}

ByteStreamIn* LASreaderASC::get_stream() const
{
  return 0;
}

void LASreaderASC::close(BOOL close_stream)
{
  if (overflow_I32_x)
  {
    LASMessage(LAS_WARNING, "total of %lld integer overflows in x", overflow_I32_x);
    overflow_I32_x = 0;
  }
  if (overflow_I32_y)
  {
    LASMessage(LAS_WARNING, "total of %lld integer overflows in y", overflow_I32_y);
    overflow_I32_y = 0;
  }
  if (overflow_I32_z)
  {
    LASMessage(LAS_WARNING, "total of %lld integer overflows in z", overflow_I32_z);
    overflow_I32_z = 0;
  }
  if (file)
  {
    if (piped) while (fgets(line, line_size, file));
    fclose(file);
    file = 0;
  }
}

BOOL LASreaderASC::reopen(const CHAR* file_name)
{
  if (file_name == 0)
  {
    laserror("file name pointer is zero");
    return FALSE;
  }

  file = fopen_compressed(file_name, "r", &piped);
  if (file == 0)
  {
    laserror("cannot reopen file '%s'", file_name);
    return FALSE;
  }

  if (setvbuf(file, NULL, _IOFBF, 10 * LAS_TOOLS_IO_IBUFFER_SIZE) != 0)
  {
    LASMessage(LAS_WARNING, "setvbuf() failed with buffer size %d", 10 * LAS_TOOLS_IO_IBUFFER_SIZE);
  }

  // read the header lines

  I32 i;
  for (i = 0; i < header_lines; i++)
  {
    fgets(line, line_size, file);
  }

  // special handling for European numbers

  if (comma_not_point)
  {
    I32 i, len = (I32)strlen(line);
    for (i = 0; i < len; i++)
    {
      if (line[i] == ',') line[i] = '.';
    }
  }

  col = 0;
  row = 0;
  p_idx = 0;
  p_cnt = 0;
  // skip leading spaces
  line_curr = 0;
  while ((line[line_curr] != '\0') && (line[line_curr] <= ' ')) line_curr++;

  return TRUE;
}

void LASreaderASC::clean()
{
  if (file)
  {
    fclose(file);
    file = 0;
  }
  if (line)
  {
    free(line);
    line = 0;
  }
  header_lines = 0;
  line_size = 0;
  line_curr = 0;
  piped = false;
  comma_not_point = FALSE;
  col = 0;
  ncols = 0;
  nrows = 0;
  xllcenter = F64_MAX;
  yllcenter = F64_MAX;
  cellsize = 0;
  nodata = -9999;
  overflow_I32_x = 0;
  overflow_I32_y = 0;
  overflow_I32_z = 0;
}

LASreaderASC::LASreaderASC(LASreadOpener* opener) :LASreader(opener)
{
  file = 0;
  line = 0;
  scale_factor = 0;
  offset = 0;
  orig_x_offset = 0.0;
  orig_y_offset = 0.0;
  orig_z_offset = 0.0;
  orig_x_scale_factor = 0.01;
  orig_y_scale_factor = 0.01;
  orig_z_scale_factor = 0.01;
  clean();
}

LASreaderASC::~LASreaderASC()
{
  clean();
  if (scale_factor)
  {
    delete[] scale_factor;
    scale_factor = 0;
  }
  if (offset)
  {
    delete[] offset;
    offset = 0;
  }
}

void LASreaderASC::populate_scale_and_offset()
{
  // if not specified in the command line, set a reasonable scale_factor
  if (scale_factor)
  {
    header.x_scale_factor = scale_factor[0];
    header.y_scale_factor = scale_factor[1];
    header.z_scale_factor = scale_factor[2];
  }
  else
  {
    if (-360 < header.min_x && -360 < header.min_y && header.max_x < 360 && header.max_y < 360) // do we have longitude / latitude coordinates
    {
      header.x_scale_factor = 1e-7;
      header.y_scale_factor = 1e-7;
    }
    else // then we assume utm or mercator / lambertian projections
    {
      header.x_scale_factor = 0.01;
      header.y_scale_factor = 0.01;
    }
    header.z_scale_factor = 0.01;
  }
  orig_x_scale_factor = header.x_scale_factor;
  orig_y_scale_factor = header.y_scale_factor;
  orig_z_scale_factor = header.z_scale_factor;

  // if not specified in the command line, set a reasonable offset
  if (offset)
  {
    header.x_offset = offset[0];
    header.y_offset = offset[1];
    header.z_offset = offset[2];
  }
  else
  {
    if (F64_IS_FINITE(header.min_x) && F64_IS_FINITE(header.max_x))
      header.x_offset = ((I64)((header.min_x + header.max_x) / header.x_scale_factor / 20000000)) * 10000000 * header.x_scale_factor;
    else
      header.x_offset = 0;

    if (F64_IS_FINITE(header.min_y) && F64_IS_FINITE(header.max_y))
      header.y_offset = ((I64)((header.min_y + header.max_y) / header.y_scale_factor / 20000000)) * 10000000 * header.y_scale_factor;
    else
      header.y_offset = 0;

    if (F64_IS_FINITE(header.min_z) && F64_IS_FINITE(header.max_z))
      header.z_offset = ((I64)((header.min_z + header.max_z) / header.z_scale_factor / 20000000)) * 10000000 * header.z_scale_factor;
    else
      header.z_offset = 0;
  }
  orig_x_offset = header.x_offset;
  orig_y_offset = header.y_offset;
  orig_z_offset = header.z_offset;
}

void LASreaderASC::populate_bounding_box()
{
  // compute quantized and then unquantized bounding box

  F64 dequant_min_x = header.get_x((I32)(header.get_X(header.min_x)));
  F64 dequant_max_x = header.get_x((I32)(header.get_X(header.max_x)));
  F64 dequant_min_y = header.get_y((I32)(header.get_Y(header.min_y)));
  F64 dequant_max_y = header.get_y((I32)(header.get_Y(header.max_y)));
  F64 dequant_min_z = header.get_z((I32)(header.get_Z(header.min_z)));
  F64 dequant_max_z = header.get_z((I32)(header.get_Z(header.max_z)));

  // make sure there is not sign flip

  if ((header.min_x > 0) != (dequant_min_x > 0))
  {
    LASMessage(LAS_WARNING, "quantization sign flip for min_x from %g to %g.\n" \
      "\tset scale factor for x coarser than %g with '-rescale'", header.min_x, dequant_min_x, header.x_scale_factor);
  }
  else
  {
    header.min_x = dequant_min_x;
  }
  if ((header.max_x > 0) != (dequant_max_x > 0))
  {
    LASMessage(LAS_WARNING, "quantization sign flip for max_x from %g to %g.\n" \
      "\tset scale factor for x coarser than %g with '-rescale'", header.max_x, dequant_max_x, header.x_scale_factor);
  }
  else
  {
    header.max_x = dequant_max_x;
  }
  if ((header.min_y > 0) != (dequant_min_y > 0))
  {
    LASMessage(LAS_WARNING, "quantization sign flip for min_y from %g to %g.\n" \
      "\tset scale factor for y coarser than %g with '-rescale'", header.min_y, dequant_min_y, header.y_scale_factor);
  }
  else
  {
    header.min_y = dequant_min_y;
  }
  if ((header.max_y > 0) != (dequant_max_y > 0))
  {
    LASMessage(LAS_WARNING, "quantization sign flip for max_y from %g to %g.\n" \
      "\tset scale factor for y coarser than %g with '-rescale'", header.max_y, dequant_max_y, header.y_scale_factor);
  }
  else
  {
    header.max_y = dequant_max_y;
  }
  if ((header.min_z > 0) != (dequant_min_z > 0))
  {
    LASMessage(LAS_WARNING, "quantization sign flip for min_z from %g to %g.\n" \
      "\tset scale factor for z coarser than %g with '-rescale'", header.min_z, dequant_min_z, header.z_scale_factor);
  }
  else
  {
    header.min_z = dequant_min_z;
  }
  if ((header.max_z > 0) != (dequant_max_z > 0))
  {
    LASMessage(LAS_WARNING, "quantization sign flip for max_z from %g to %g.\n" \
      "\tset scale factor for z coarser than %g with '-rescale'", header.max_z, dequant_max_z, header.z_scale_factor);
  }
  else
  {
    header.max_z = dequant_max_z;
  }
}

LASreaderASCrescale::LASreaderASCrescale(LASreadOpener* opener, F64 x_scale_factor, F64 y_scale_factor, F64 z_scale_factor) : LASreaderASC(opener)
{
  scale_factor[0] = x_scale_factor;
  scale_factor[1] = y_scale_factor;
  scale_factor[2] = z_scale_factor;
}

BOOL LASreaderASCrescale::open(const CHAR* file_name, BOOL comma_not_point)
{
  LASreaderASC::set_scale_factor(scale_factor);
  if (!LASreaderASC::open(file_name, comma_not_point)) return FALSE;
  return TRUE;
}

LASreaderASCreoffset::LASreaderASCreoffset(LASreadOpener* opener, F64 x_offset, F64 y_offset, F64 z_offset) : LASreaderASC(opener)
{
  this->offset[0] = x_offset;
  this->offset[1] = y_offset;
  this->offset[2] = z_offset;
}

BOOL LASreaderASCreoffset::open(const CHAR* file_name, BOOL comma_not_point)
{
  LASreaderASC::set_offset(offset);
  if (!LASreaderASC::open(file_name, comma_not_point)) return FALSE;
  return TRUE;
}

LASreaderASCrescalereoffset::LASreaderASCrescalereoffset(LASreadOpener* opener, F64 x_scale_factor, F64 y_scale_factor, F64 z_scale_factor, F64 x_offset, F64 y_offset, F64 z_offset) :
  LASreaderASC(opener),
  LASreaderASCrescale(opener, x_scale_factor, y_scale_factor, z_scale_factor),
  LASreaderASCreoffset(opener, x_offset, y_offset, z_offset)
{
}

BOOL LASreaderASCrescalereoffset::open(const CHAR* file_name, BOOL comma_not_point)
{
  LASreaderASC::set_scale_factor(scale_factor);
  LASreaderASC::set_offset(offset);
  if (!LASreaderASC::open(file_name, comma_not_point)) return FALSE;
  return TRUE;
}
