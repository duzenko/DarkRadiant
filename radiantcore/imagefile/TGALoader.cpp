/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "TGALoader.h"

#include "ifilesystem.h"
#include "iarchive.h"
#include "idatastream.h"

typedef unsigned char byte;

#include <stdlib.h>

#include "stream/ScopedArchiveBuffer.h"
#include "stream/PointerInputStream.h"
#include "RGBAImage.h"
#include "stream/utils.h"

namespace image
{

// represents x,y origin of tga image being decoded
class Flip00 {}; // no flip
class Flip01 {}; // vertical flip only
class Flip10 {}; // horizontal flip only
class Flip11 {}; // both

template<typename PixelDecoder>
void image_decode(stream::PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip00&)
{
  RGBAPixel* end = image.pixels + (image.getHeight() * image.getWidth());
  for(RGBAPixel* row = end; row != image.pixels; row -= image.getWidth())
  {
    for(RGBAPixel* pixel = row - image.getWidth(); pixel != row; ++pixel)
    {
      decode(istream, *pixel);
    }
  }
}

template<typename PixelDecoder>
void image_decode(stream::PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip01&)
{
  RGBAPixel* end = image.pixels + (image.getHeight() * image.getWidth());
  for(RGBAPixel* row = image.pixels; row != end; row += image.getWidth())
  {
    for(RGBAPixel* pixel = row; pixel != row + image.getWidth(); ++pixel)
    {
      decode(istream, *pixel);
    }
  }
}

template<typename PixelDecoder>
void image_decode(stream::PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip10&)
{
  RGBAPixel* end = image.pixels + (image.getHeight() * image.getWidth());
  for(RGBAPixel* row = end; row != image.pixels; row -= image.getWidth())
  {
    for(RGBAPixel* pixel = row; pixel != row - image.getWidth();)
    {
      decode(istream, *--pixel);
    }
  }
}

template<typename PixelDecoder>
void image_decode(stream::PointerInputStream& istream, PixelDecoder& decode, RGBAImage& image, const Flip11&)
{
  RGBAPixel* end = image.pixels + (image.getHeight() * image.getWidth());
  for(RGBAPixel* row = image.pixels; row != end; row += image.getWidth())
  {
    for(RGBAPixel* pixel = row + image.getWidth(); pixel != row;)
    {
      decode(istream, *--pixel);
    }
  }
}

inline void istream_read_gray(stream::PointerInputStream& istream, RGBAPixel& pixel)
{
  istream.read(&pixel.blue, 1);
  pixel.red = pixel.green = pixel.blue;
  pixel.alpha = 0xff;
}

inline void istream_read_rgb(stream::PointerInputStream& istream, RGBAPixel& pixel)
{
  istream.read(&pixel.blue, 1);
  istream.read(&pixel.green, 1);
  istream.read(&pixel.red, 1);
  pixel.alpha = 0xff;
}

inline void istream_read_rgba(stream::PointerInputStream& istream, RGBAPixel& pixel)
{
  istream.read(&pixel.blue, 1);
  istream.read(&pixel.green, 1);
  istream.read(&pixel.red, 1);
  istream.read(&pixel.alpha, 1);
}

class TargaDecodeGrayPixel
{
public:
  void operator()(stream::PointerInputStream& istream, RGBAPixel& pixel)
  {
    istream_read_gray(istream, pixel);
  }
};

template<typename Flip>
void targa_decode_grayscale(stream::PointerInputStream& istream, RGBAImage& image, const Flip& flip)
{
  TargaDecodeGrayPixel decode;
  image_decode(istream, decode, image, flip);
}

class TargaDecodeRGBPixel
{
public:
  void operator()(stream::PointerInputStream& istream, RGBAPixel& pixel)
  {
    istream_read_rgb(istream, pixel);
  }
};

template<typename Flip>
void targa_decode_rgb(stream::PointerInputStream& istream, RGBAImage& image, const Flip& flip)
{
  TargaDecodeRGBPixel decode;
  image_decode(istream, decode, image, flip);
}

class TargaDecodeRGBAPixel
{
public:
  void operator()(stream::PointerInputStream& istream, RGBAPixel& pixel)
  {
    istream_read_rgba(istream, pixel);
  }
};

template<typename Flip>
void targa_decode_rgba(stream::PointerInputStream& istream, RGBAImage& image, const Flip& flip)
{
  TargaDecodeRGBAPixel decode;
  image_decode(istream, decode, image, flip);
}

typedef byte TargaPacket;
typedef byte TargaPacketSize;

inline void targa_packet_read_istream(TargaPacket& packet, stream::PointerInputStream& istream)
{
  istream.read(&packet, 1);
}

inline bool targa_packet_is_rle(const TargaPacket& packet)
{
  return (packet & 0x80) != 0;
}

inline TargaPacketSize targa_packet_size(const TargaPacket& packet)
{
  return 1 + (packet & 0x7f);
}


class TargaDecodeRGBPixelRLE
{
  TargaPacketSize m_packetSize;
  RGBAPixel m_pixel;
  TargaPacket m_packet;
public:
  TargaDecodeRGBPixelRLE() : m_packetSize(0)
  {
  }
  void operator()(stream::PointerInputStream& istream, RGBAPixel& pixel)
  {
    if(m_packetSize == 0)
    {
      targa_packet_read_istream(m_packet, istream);
      m_packetSize = targa_packet_size(m_packet);

      if(targa_packet_is_rle(m_packet))
      {
        istream_read_rgb(istream, m_pixel);
      }
    }

    if(targa_packet_is_rle(m_packet))
    {
      pixel = m_pixel;
    }
    else
    {
      istream_read_rgb(istream, pixel);
    }

    --m_packetSize;
  }
};

template<typename Flip>
void targa_decode_rle_rgb(stream::PointerInputStream& istream, RGBAImage& image, const Flip& flip)
{
	TargaDecodeRGBPixelRLE decode;
  image_decode(istream, decode, image, flip);
}

class TargaDecodeRGBAPixelRLE
{
  TargaPacketSize m_packetSize;
  RGBAPixel m_pixel;
  TargaPacket m_packet;
public:
  TargaDecodeRGBAPixelRLE() : m_packetSize(0)
  {
  }
  void operator()(stream::PointerInputStream& istream, RGBAPixel& pixel)
  {
    if(m_packetSize == 0)
    {
      targa_packet_read_istream(m_packet, istream);
      m_packetSize = targa_packet_size(m_packet);

      if(targa_packet_is_rle(m_packet))
      {
        istream_read_rgba(istream, m_pixel);
      }
    }

    if(targa_packet_is_rle(m_packet))
    {
      pixel = m_pixel;
    }
    else
    {
      istream_read_rgba(istream, pixel);
    }

    --m_packetSize;
  }
};

template<typename Flip>
void targa_decode_rle_rgba(stream::PointerInputStream& istream, RGBAImage& image, const Flip& flip)
{
	TargaDecodeRGBAPixelRLE decode;
  image_decode(istream, decode, image, flip);
}

struct TargaHeader
{
  unsigned char id_length, colormap_type, image_type;
  unsigned short colormap_index, colormap_length;
  unsigned char colormap_size;
  unsigned short x_origin, y_origin, width, height;
  unsigned char pixel_size, attributes;
};

inline void targa_header_read_istream(TargaHeader& targa_header, stream::PointerInputStream& istream)
{
	static_assert(sizeof(unsigned short) == sizeof(uint16_t), "TGA Loader Code relies on unsigned short being 16 bits wide!");

	targa_header.id_length = stream::readByte(istream);
	targa_header.colormap_type = stream::readByte(istream);
	targa_header.image_type = stream::readByte(istream);

	targa_header.colormap_index = stream::readLittleEndian<uint16_t>(istream);
	targa_header.colormap_length = stream::readLittleEndian<uint16_t>(istream);
	targa_header.colormap_size = stream::readByte(istream);
	targa_header.x_origin = stream::readLittleEndian<uint16_t>(istream);
	targa_header.y_origin = stream::readLittleEndian<uint16_t>(istream);
	targa_header.width = stream::readLittleEndian<uint16_t>(istream);
	targa_header.height = stream::readLittleEndian<uint16_t>(istream);
	targa_header.pixel_size = stream::readByte(istream);
	targa_header.attributes = stream::readByte(istream);

  if (targa_header.id_length != 0)
    istream.seek(targa_header.id_length);	// skip TARGA image comment
}

template<typename Type>
class ScopeDelete
{
  Type* m_value;
  ScopeDelete(const ScopeDelete&);
  ScopeDelete& operator=(const ScopeDelete&);
public:
  ScopeDelete(Type* value) : m_value(value)
  {
  }
  ~ScopeDelete()
  {
    delete m_value;
  }
  Type* get_pointer() const
  {
    return m_value;
  }
};

template<typename Flip>
RGBAImagePtr Targa_decodeImageData(const TargaHeader& targa_header, stream::PointerInputStream& istream, const Flip& flip)
{
  RGBAImagePtr image (new RGBAImage(targa_header.width, targa_header.height));

  if (targa_header.image_type == 2 || targa_header.image_type == 3)
  {
    switch (targa_header.pixel_size)
    {
    case 8:
      targa_decode_grayscale(istream, *image, flip);
      break;
    case 24:
      targa_decode_rgb(istream, *image, flip);
      break;
    case 32:
      targa_decode_rgba(istream, *image, flip);
      break;
    default:
      rError() << "LoadTGA: illegal pixel_size '" << targa_header.pixel_size << "'\n";
      return RGBAImagePtr();
    }
  }
  else if (targa_header.image_type == 10)
  {
    switch (targa_header.pixel_size)
    {
    case 24:
      targa_decode_rle_rgb(istream, *image, flip);
      break;
    case 32:
      targa_decode_rle_rgba(istream, *image, flip);
      break;
    default:
      rError() << "LoadTGA: illegal pixel_size '" << targa_header.pixel_size << "'\n";
      return RGBAImagePtr();
    }
  }

  return image;
}

const unsigned int TGA_FLIP_HORIZONTAL = 0x10;
const unsigned int TGA_FLIP_VERTICAL = 0x20;

RGBAImagePtr LoadTGABuff(const byte* buffer)
{
	stream::PointerInputStream istream(buffer);
  TargaHeader targa_header;

  targa_header_read_istream(targa_header, istream);

  if (targa_header.image_type != 2 && targa_header.image_type != 10 && targa_header.image_type != 3)
  {
    rError() << "LoadTGA: TGA type " << targa_header.image_type << " not supported\n";
    rError() << "LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n";
    return RGBAImagePtr();
  }

  if (targa_header.colormap_type != 0)
  {
    rError() << "LoadTGA: colormaps not supported\n";
    return RGBAImagePtr();
  }

  if ((targa_header.pixel_size != 32 && targa_header.pixel_size != 24)
      && targa_header.image_type != 3)
  {
    rError() << "LoadTGA: Only 32 or 24 bit images supported\n";
    return RGBAImagePtr();
  }

  if ((targa_header.attributes & (TGA_FLIP_HORIZONTAL|TGA_FLIP_VERTICAL)) == 0)
  {
    return Targa_decodeImageData(targa_header, istream, Flip00());
  }

  if((targa_header.attributes & TGA_FLIP_HORIZONTAL) == 0 &&
	 (targa_header.attributes & TGA_FLIP_VERTICAL) != 0)
  {
    return Targa_decodeImageData(targa_header, istream, Flip01());
  }

  if ((targa_header.attributes & TGA_FLIP_HORIZONTAL) != 0 &&
	  (targa_header.attributes & TGA_FLIP_VERTICAL) == 0)
  {
    return Targa_decodeImageData(targa_header, istream, Flip10());
  }

  if ((targa_header.attributes & TGA_FLIP_HORIZONTAL) != 0 &&
	  (targa_header.attributes & TGA_FLIP_VERTICAL) != 0)
  {
    return Targa_decodeImageData(targa_header, istream, Flip11());
  }

  // unreachable
  return RGBAImagePtr();
}

ImagePtr TGALoader::load(ArchiveFile& file) const
{
    archive::ScopedArchiveBuffer buffer(file);
    return LoadTGABuff(buffer.buffer);
}

ImageTypeLoader::Extensions TGALoader::getExtensions() const
{
    Extensions extensions;
    extensions.push_back("tga");
    return extensions;
}

}
