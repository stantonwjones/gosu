#include <Gosu/Bitmap.hpp>
#include <Gosu/Platform.hpp>
#include <Gosu/IO.hpp>
#include <Gosu/Utility.hpp>

#include <cstring>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#include "../stb_image.h"

namespace
{
    int readCallback(void* user, char* data, int size)
    {
        Gosu::Reader* reader = static_cast<Gosu::Reader*>(user);
        std::size_t remaining = reader->resource().size() - reader->position();
        std::size_t actualSize = (size < remaining ? size : remaining);
        reader->read(data, actualSize);
        return actualSize;
    }

    void skipCallback(void* user, int n)
    {
        Gosu::Reader* reader = static_cast<Gosu::Reader*>(user);
        reader->setPosition(reader->position() + n);
    }

    int eofCallback(void* user)
    {
        Gosu::Reader* reader = static_cast<Gosu::Reader*>(user);
        return reader->position() == reader->resource().size();
    }

    bool isBMP(Gosu::Reader reader)
    {
        std::size_t remaining = reader.resource().size() - reader.position();
        if (remaining < 2)
            return false;
        char magicBytes[2];
        reader.read(magicBytes, sizeof magicBytes);
        reader.seek(sizeof magicBytes);
        return magicBytes[0] == 'B' && magicBytes[1] == 'M';
    }
}

void Gosu::loadImageFile(Gosu::Bitmap& bitmap, const std::wstring& filename)
{
    Buffer buffer;
    loadFile(buffer, filename);
    loadImageFile(bitmap, buffer.frontReader());
}

void Gosu::loadImageFile(Gosu::Bitmap& bitmap, Reader input)
{
    bool needsColorKey = isBMP(input);
    
    stbi_io_callbacks callbacks;
    callbacks.read = readCallback;
    callbacks.skip = skipCallback;
    callbacks.eof = eofCallback;
    int x, y, n;
    stbi_uc* bytes = stbi_load_from_callbacks(&callbacks, &input, &x, &y, &n, STBI_rgb_alpha);

    if (bytes == 0) {
        // TODO - stbi_failure_reason is not thread safe. Everything here should be wrapped in a mutex.
        throw std::runtime_error("Cannot load image: " + std::string(stbi_failure_reason()));
    }

    bitmap.resize(x, y);
    std::memcpy(bitmap.data(), bytes, x * y * sizeof(Gosu::Color));

    stbi_image_free(bytes);
    
    if (needsColorKey)
        applyColorKey(bitmap, Gosu::Color::FUCHSIA);
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

// TODO: Move into proper internal header
namespace Gosu { bool isExtension(const wchar_t* str, const wchar_t* ext); }

void Gosu::saveImageFile(const Gosu::Bitmap& bitmap, const std::wstring& filename)
{
    int ok;
    std::string utf8 = Gosu::wstringToUTF8(filename);
    
    if (isExtension(filename.c_str(), L"bmp"))
    {
        ok = stbi_write_bmp(utf8.c_str(), bitmap.width(), bitmap.height(), 4, bitmap.data());
    }
    else if (isExtension(filename.c_str(), L"tga"))
    {
        ok = stbi_write_tga(utf8.c_str(), bitmap.width(), bitmap.height(), 4, bitmap.data());
    }
    else
    {
        ok = stbi_write_png(utf8.c_str(), bitmap.width(), bitmap.height(), 4, bitmap.data(), 0);
    }
    
    if (ok == 0)
        throw std::runtime_error("Could not save image data to file: " + utf8);
}

void Gosu::saveImageFile(const Gosu::Bitmap& bitmap, Gosu::Writer writer,
    const std::wstring& formatHint)
{
    unsigned char* rgba =
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(bitmap.data()));
    
    // TODO: Use the new *_to_func functions in stb_imagewrite.h instead:
    // https://github.com/nothings/stb/blob/master/stb_image_write.h#L39
    int length;
    unsigned char* png =
        stbi_write_png_to_mem(rgba, 0, bitmap.width(), bitmap.height(), 4, &length);
    
    if (png == 0)
        throw std::runtime_error("Could not save image data to memory");
    
    try
    {
        writer.write(png, length);
    }
    catch (...)
    {
        STBIW_FREE(png);
        throw;
    }
    
    STBIW_FREE(png);
}

// Deprecated methods.

Gosu::Reader Gosu::loadFromPNG(Bitmap& bitmap, Reader reader)
{
    loadImageFile(bitmap, reader);
    reader.setPosition(reader.resource().size());
    return reader;
}

Gosu::Reader Gosu::loadFromBMP(Bitmap& bitmap, Reader reader)
{
    return loadFromPNG(bitmap, reader);
}

Gosu::Writer Gosu::saveToPNG(const Bitmap& bitmap, Writer writer)
{
    saveImageFile(bitmap, writer, L".png");
    return writer.resource().backWriter();
}

Gosu::Writer Gosu::saveToBMP(const Bitmap& bitmap, Writer writer)
{
    saveImageFile(bitmap, writer, L".bmp");
    return writer.resource().backWriter();
}
