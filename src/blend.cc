#include <string.h>
#include <v8.h>
#include <node.h>
#include <node_events.h>

#include "blend.h"
#include "reader.h"
#include "macros.h"

typedef std::pair<char*, size_t> PNGBuffer;
typedef std::vector<PNGBuffer> PNGBuffers;
typedef Persistent<Object> PersistentObject;
typedef std::vector<PersistentObject> PersistentObjects;

struct BlendBaton {
    Persistent<Function> callback;
    PersistentObjects references;
    PNGBuffers buffers;

    bool error;

    char* result;
    size_t length;
    size_t max;

    BlendBaton(Handle<Function> cb)
        : error(false), result(NULL), length(0), max(0) {
        ev_ref(EV_DEFAULT_UC);
        callback = Persistent<Function>::New(cb);
    }
    void add(Handle<Object> buffer) {
        references.push_back(Persistent<Object>::New(buffer));
        buffers.push_back(std::make_pair<char*, size_t>(Buffer::Data(buffer), Buffer::Length(buffer)));
    }
    ~BlendBaton() {
        ev_unref(EV_DEFAULT_UC);

        buffers.clear();

        PersistentObjects::iterator cur = references.begin();
        PersistentObjects::iterator end = references.end();
        for (; cur < end; cur++) (*cur).Dispose();

        if (result) {
            free(result);
        }

        callback.Dispose();
    }
};

Handle<Value> ThrowOrCall(Handle<Function> callback, const char* message) {
    if (callback.IsEmpty()) {
        return ThrowException(Exception::TypeError(String::New(message)));
    } else {
        Local<Value> argv[] = { Exception::TypeError(String::New(message)) };
        TRY_CATCH_CALL(Context::GetCurrent()->Global(), callback, 1, argv);
        return Undefined();
    }
}

Handle<Value> Blend(const Arguments& args) {
    HandleScope scope;

    OPTIONAL_ARGUMENT_FUNCTION(1, callback);

    if (args.Length() < 1 || !args[0]->IsArray()) {
        return ThrowOrCall(callback, "First argument must be an array of Buffers.");
    }
    Local<Array> buffers = Local<Array>::Cast(args[0]);

    uint32_t length = buffers->Length();

    if (length < 1) {
        return ThrowOrCall(callback, "First argument must contain at least one Buffer.");
    }

    BlendBaton* baton = new BlendBaton(callback);
    for (uint32_t i = 0; i < length; i++) {
        if (!Buffer::HasInstance(buffers->Get(i))) {
            delete baton;
            return ThrowOrCall(callback, "All elements must be Buffers.");
        } else {
            baton->add(buffers->Get(i)->ToObject());
        }
    }

    eio_custom(EIO_Blend, EIO_PRI_DEFAULT, EIO_AfterBlend, baton);

    return scope.Close(Undefined());
}

void Blend_writePNG(png_structp png_ptr, png_bytep data, png_size_t length) {
    BlendBaton* baton = (BlendBaton*)png_get_io_ptr(png_ptr);

    if (baton->result == NULL || baton->max < baton->length + length) {
        int increase = baton->length ? 4 * length : 32768;
        baton->result = (char*)realloc(baton->result, baton->max + increase);
        baton->max += increase;
    }

    // TODO: implement OOM check
    assert(baton->result);

    memcpy(baton->result + baton->length, data, length);
    baton->length += length;
}


void Blend_Encode(unsigned const char* source, BlendBaton* baton,
        unsigned long width, unsigned long height, bool alpha) {
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    png_set_compression_level(png_ptr, Z_BEST_SPEED);
    png_set_compression_buffer_size(png_ptr, 32768);

    png_set_IHDR(png_ptr, info_ptr, width, height, 8,
                 alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_set_write_fn(png_ptr, (png_voidp)baton, Blend_writePNG, NULL);
    png_write_info(png_ptr, info_ptr);

    if (!alpha) {
        png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
    }

    png_bytep row_pointers[height];
    for (unsigned i = 0; i < height; i++) {
        row_pointers[i] = (unsigned char*)(source + (4 * width * i));
    }

    // Write image data
    png_write_image(png_ptr, row_pointers);

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
}


void blendAlpha(unsigned char* dst_img, const unsigned char* src_img,
                unsigned long width, unsigned long height) {
    // both dst and src are RGBA
    for (unsigned long y = 0; y < height; y++) {
        int offset = 4 * y * width;
        const unsigned int* src = (const unsigned int*)(src_img + offset);
        unsigned int* dst = (unsigned int*)(dst_img + offset);

        for (unsigned long x = 0; x < width; x++) {
             unsigned int rgba0 = dst[x];
             unsigned int rgba1 = src[x];

             // From http://trac.mapnik.org/browser/trunk/include/mapnik/graphics.hpp#L337
             unsigned a1 = (rgba1 >> 24) & 0xff;
             if (a1 == 0) continue;
             if (a1 == 0xff) {
                 dst[x] = rgba1;
                 continue;
             }
             unsigned r1 = rgba1 & 0xff;
             unsigned g1 = (rgba1 >> 8 ) & 0xff;
             unsigned b1 = (rgba1 >> 16) & 0xff;

             unsigned a0 = (rgba0 >> 24) & 0xff;
             unsigned r0 = (rgba0 & 0xff) * a0;
             unsigned g0 = ((rgba0 >> 8 ) & 0xff) * a0;
             unsigned b0 = ((rgba0 >> 16) & 0xff) * a0;

             a0 = ((a1 + a0) << 8) - a0*a1;

             r0 = ((((r1 << 8) - r0) * a1 + (r0 << 8)) / a0);
             g0 = ((((g1 << 8) - g0) * a1 + (g0 << 8)) / a0);
             b0 = ((((b1 << 8) - b0) * a1 + (b0 << 8)) / a0);
             a0 = a0 >> 8;
             dst[x] = (a0 << 24)| (b0 << 16) | (g0 << 8) | (r0);
        }
    }
}

void blend(unsigned char* dst_img, const unsigned char* src_img,
                unsigned long width, unsigned long height) {
    // dst is RGB, src is RGBA

}

int EIO_Blend(eio_req *req) {
    BlendBaton* baton = static_cast<BlendBaton*>(req->data);

    PNGBuffer top = baton->buffers.back();
    ImageReader* base = ImageReader::create(top.first, top.second);

    if (!base->alpha) {
        // Exit early when the topmost image isn't transparent.
        baton->result = top.first;
        baton->length = top.second;
        delete base;
        return 0;
    }

    unsigned char* result = (unsigned char*)malloc(base->width * base->height * 4);
    base->decode(result, true);

    bool alpha = true;

    // Iterate from the second to last to first image.
    PNGBuffers::reverse_iterator image = baton->buffers.rbegin();
    PNGBuffers::reverse_iterator end = baton->buffers.rend();
    for (image++; image < end && alpha && !baton->error; image++) {
        ImageReader* layer = ImageReader::create((*image).first, (*image).second);

        if (layer->width != base->width || layer->height != base->height) {
            baton->error = true;
        } else {
            // alpha = layer->alpha;

            size_t size = base->width * base->height * (alpha ? 4 : 3);
            unsigned char* surface = (unsigned char*)malloc(size);
            layer->decode(surface, alpha);

            if (alpha) {
                blendAlpha(surface, result, base->width, base->height);
            } else {
                blend(surface, result, base->width, base->height);
            }

            free(result);
            result = surface;
            surface = NULL;
        }

        delete layer;
    }

    if (result) {
        if (!baton->error) {
            Blend_Encode(result, baton, base->width, base->height, alpha);
        }

        free(result);
        result = NULL;
    }

    delete base;
    return 0;
}

int EIO_AfterBlend(eio_req *req) {
    HandleScope scope;
    BlendBaton* baton = static_cast<BlendBaton*>(req->data);

    if (!baton->callback.IsEmpty()) {
        if (!baton->error) {
            Local<Value> argv[] = {
                Local<Value>::New(Null()),
                Local<Value>::New(Buffer::New(baton->result, baton->length)->handle_)
            };
            TRY_CATCH_CALL(Context::GetCurrent()->Global(), baton->callback, 2, argv);
        } else {
            Local<Value> argv[] = {
                Local<Value>::New(Exception::TypeError(String::New("Unspecified error")))
            };
            TRY_CATCH_CALL(Context::GetCurrent()->Global(), baton->callback, 2, argv);
        }
    }

    delete baton;
    return 0;
}
