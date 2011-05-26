#include "reader.h"

ImageReader* ImageReader::create(const char* surface, size_t len) {
    if (png_sig_cmp((png_bytep)surface, 0, 8) == 0) {
        return new PNGImageReader(surface, len);
    }

    return NULL;
}

PNGImageReader::PNGImageReader(const char* src, size_t len) : ImageReader() {
    source = src;
    length = len;

    // Decode PNG header.
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);
    png_set_read_fn(png, this, readCallback);
    png_read_info(png, info);
    png_get_IHDR(png, info, &width, &height, &depth, &color, NULL, NULL, NULL);
    alpha = (color & PNG_COLOR_MASK_ALPHA) > 0;
}

PNGImageReader::~PNGImageReader() {
    png_destroy_read_struct(&png, &info, NULL);
}

void PNGImageReader::readCallback(png_structp png, png_bytep data, png_size_t length) {
    PNGImageReader* reader = static_cast<PNGImageReader*>(png_get_io_ptr(png));

    // Read `length` bytes into `data`.
    if (reader->pos + length > reader->length) {
        png_error(png, "Read Error");
        return;
    }

    memcpy(data, reader->source + reader->pos, length);
    reader->pos += length;
}


void PNGImageReader::decode(char* surface, bool alpha) {

}
