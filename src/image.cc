#include <string.h>
#include <v8.h>
#include <node.h>
#include <node_events.h>

#include <cstdlib>
#include <cstring>

#include "image.h"
#include "macros.h"

Persistent<FunctionTemplate> Image::constructor_template;

void Image::Init(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->Inherit(EventEmitter::constructor_template);
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    constructor_template->SetClassName(String::NewSymbol("Image"));

    NODE_SET_PROTOTYPE_METHOD(constructor_template, "load", Load);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "process", Process);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "overlay", Overlay);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "asPNG", AsPNG);

    Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
    instance_template->SetAccessor(String::NewSymbol("width"), GetWidth);
    instance_template->SetAccessor(String::NewSymbol("height"), GetHeight);
    instance_template->SetAccessor(String::NewSymbol("data"), GetData);

    target->Set(String::NewSymbol("Image"), constructor_template->GetFunction());
}

Handle<Value> Image::New(const Arguments& args) {
    HandleScope scope;
    Image *img = new Image;
    img->Wrap(args.This());
    return args.This();
}

void Image::Schedule(EIO_Callback callback, Baton* baton) {
    queue.push(new Call(callback, baton));
    Process();
}

void Image::Process() {
    while (!locked && !queue.empty()) {
        Call* call = queue.front();
        if (call->baton->precondition(call->baton)) {
            queue.pop();
            call->callback(call->baton);
            delete call;
        } else {
            break;
        }
    }
}

Handle<Value> Image::Process(const Arguments& args) {
    HandleScope scope;
    Image* image = ObjectWrap::Unwrap<Image>(args.This());
    image->Process();
    return args.This();
}

Handle<Value> Image::GetWidth(Local<String> name, const AccessorInfo& info) {
    HandleScope scope;
    Image* image = ObjectWrap::Unwrap<Image>(info.This());
    return scope.Close(Number::New(image->width));
}

Handle<Value> Image::GetHeight(Local<String> name, const AccessorInfo& info) {
    HandleScope scope;
    Image *image = ObjectWrap::Unwrap<Image>(info.This());
    return scope.Close(Number::New(image->height));
}

Handle<Value> Image::GetData(Local<String> name, const AccessorInfo& info) {
    HandleScope scope;
    Image *image = ObjectWrap::Unwrap<Image>(info.This());

    if (image->data == NULL) {
        return scope.Close(Undefined());
    } else {
        // Returns a copy of the buffer for now.
        Buffer *buffer = Buffer::New(image->data, 4 * image->width * image->height);
        return scope.Close(buffer->handle_);
    }
}

//Image#load(buffer) decodes the PNG/JPEG buffer passed in and sets .data to the resulting RGBA buffer
// emits 'load' when done and calls the callback if provided.
Handle<Value> Image::Load(const Arguments& args) {
    HandleScope scope;
    Image* image = ObjectWrap::Unwrap<Image>(args.This());

    OPTIONAL_ARGUMENT_FUNCTION(1, callback);
    if (args.Length() < 1 || !Buffer::HasInstance(args[0])) {
        return ThrowException(Exception::TypeError(
            String::New("Buffer required as first argument")));
    }

    Baton* baton = new LoadBaton(image, callback, args[0]->ToObject());
    image->Schedule(EIO_BeginLoad, baton);

    return args.This();
}

void Image::EIO_BeginLoad(Baton* baton) {
    baton->image->locked = true;
    eio_custom(EIO_Load, EIO_PRI_DEFAULT, EIO_AfterLoad, baton);
}

void Image::readPNG(png_structp png_ptr, png_bytep data, png_size_t length) {
    LoadBaton* baton = static_cast<LoadBaton*>(png_get_io_ptr(png_ptr));

    // Read `length` bytes into `data`.
    if (baton->pos + length > baton->length) {
        png_error(png_ptr, "Read Error");
        return;
    }

    memcpy(data, baton->data + baton->pos, length);
    baton->pos += length;
}

int Image::EIO_Load(eio_req *req) {
    LoadBaton* baton = static_cast<LoadBaton*>(req->data);
    Image* image = baton->image;

    assert(image->data == NULL);
    assert(baton->pos == 0);

    // Decode baton->data
    if (png_sig_cmp((png_bytep)baton->data, 0, 8) == 0) {
        // This is a PNG image
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        assert(png_ptr);
        // if (!png_ptr) {
        //     baton->error = 1;
        //     return 0;
        // }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        assert(info_ptr);
        // if (!info_ptr) {
        //     png_destroy_read_struct(&png_ptr, NULL, NULL);
        //     baton->error = 2;
        //     return 0;
        // }

        png_set_read_fn(png_ptr, (png_voidp)baton, readPNG);
        png_read_info(png_ptr, info_ptr);

        png_uint_32 width = 0;
        png_uint_32 height = 0;
        int depth = 0;
        int color = -1;
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &depth, &color, NULL, NULL, NULL);

        // From http://trac.mapnik.org/browser/trunk/src/png_reader.cpp
        if (color == PNG_COLOR_TYPE_PALETTE)
            png_set_expand(png_ptr);
        if (color == PNG_COLOR_TYPE_GRAY)
            png_set_expand(png_ptr);
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
            png_set_expand(png_ptr);
        if (depth == 16)
            png_set_strip_16(png_ptr);
        if (depth < 8)
            png_set_packing(png_ptr);
        if (color == PNG_COLOR_TYPE_GRAY ||
            color == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png_ptr);

        // Force to RGBA
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

        double gamma;
        if (png_get_gAMA(png_ptr, info_ptr, &gamma))
            png_set_gamma(png_ptr, 2.2, gamma);

        png_read_update_info(png_ptr, info_ptr);

        unsigned int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
        assert(width * 4 == rowbytes);

        char* data = new char[height * rowbytes];
        assert(data);
        // if (image->data == NULL) {
        //     baton->error = 3;
        //     return 0;
        // }

        png_bytep row_pointers[height];
        for (unsigned i = 0; i < height; i++) {
            row_pointers[i] = (unsigned char*)(data + (i * rowbytes));
        }

        // Read image data
        png_read_image(png_ptr, row_pointers);

        png_read_end(png_ptr, NULL);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        image->width = width;
        image->height = height;
        image->data = data;
    }

    return 0;
}

int Image::EIO_AfterLoad(eio_req *req) {
    HandleScope scope;
    LoadBaton* baton = static_cast<LoadBaton*>(req->data);
    Image* image = baton->image;

    if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
        Local<Value> argv[] = {
            Local<Value>::New(Null()),
            Local<Value>::New(image->handle_)
        };
        TRY_CATCH_CALL(image->handle_, baton->callback, 2, argv);
    }

    Local<Value> args[] = {
        String::NewSymbol("load"),
        Local<Value>::New(image->handle_)
    };
    EMIT_EVENT(image->handle_, 2, args);

    delete baton;
    image->locked = false;
    image->Process();
    return 0;
}


//Image#AsPNG(buffer) decodes the PNG/JPEG buffer passed in and sets .data to the resulting RGBA buffer
// emits 'AsPNG' when done and calls the callback if provided.
Handle<Value> Image::AsPNG(const Arguments& args) {
    HandleScope scope;
    Image* image = ObjectWrap::Unwrap<Image>(args.This());

    // First argument is a hash with config options depth/color
    OPTIONAL_ARGUMENT_FUNCTION(1, callback);

    Baton* baton = new AsPNGBaton(image, callback);
    image->Schedule(EIO_BeginAsPNG, baton);

    return args.This();
}

void Image::EIO_BeginAsPNG(Baton* baton) {
    baton->image->locked = true;
    eio_custom(EIO_AsPNG, EIO_PRI_DEFAULT, EIO_AfterAsPNG, baton);
}

void Image::writePNG(png_structp png_ptr, png_bytep data, png_size_t length) {
    AsPNGBaton* baton = static_cast<AsPNGBaton*>(png_get_io_ptr(png_ptr));

    if (baton->data == NULL || baton->max < baton->length + length) {
        int increase = baton->length ? 4 * length : 32768;
        baton->data = (char*)realloc(baton->data, baton->max + increase);
        baton->max += increase;
    }

    // TODO: implement OOM check
    assert(baton->data);

    memcpy(baton->data + baton->length, data, length);
    baton->length += length;
}

int Image::EIO_AsPNG(eio_req *req) {
    AsPNGBaton* baton = static_cast<AsPNGBaton*>(req->data);
    Image* image = baton->image;

    assert(image->data != NULL);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr);
    // if (!png_ptr) {
    //     baton->error = 1;
    //     return 0;
    // }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr);
    // if (!info_ptr) {
    //     png_destroy_writestruct(&png_ptr, NULL, NULL);
    //     baton->error = 2;
    //     return 0;
    // }

    png_set_compression_level(png_ptr, Z_BEST_SPEED);

    png_set_IHDR(png_ptr, info_ptr, image->width, image->height, 8,
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_write_fn(png_ptr, (png_voidp)baton, writePNG, NULL);
    png_write_info(png_ptr, info_ptr);

    // Enable this when setting PNG_COLOR_TYPE_RGB instead of PNG_COLOR_TYPE_RGB_ALPHA.
    // png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);

    png_bytep row_pointers[image->height];
    for (unsigned i = 0; i < image->height; i++) {
        row_pointers[i] = (unsigned char*)(image->data + (4 * image->width * i));
    }

    // Write image data
    png_write_image(png_ptr, row_pointers);

    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return 0;
}

int Image::EIO_AfterAsPNG(eio_req *req) {
    HandleScope scope;
    AsPNGBaton* baton = static_cast<AsPNGBaton*>(req->data);
    Image* image = baton->image;

    if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
        if (baton->data != NULL && baton->length > 0) {
            Local<Value> argv[] = {
                Local<Value>::New(Null()),
                // TODO: Currently creates a copy of the data.
                // TODO: Buffer::New returns a persistent handle. ->Ref() it?
                Local<Value>::New(Buffer::New(baton->data, baton->length)->handle_)
            };
            TRY_CATCH_CALL(image->handle_, baton->callback, 2, argv);
        } else {
            // TODO: send proper error message.
            Local<Value> argv[] = { Local<Value>::New(True()) };
            TRY_CATCH_CALL(image->handle_, baton->callback, 1, argv);
        }
    }

    delete baton;
    image->locked = false;
    image->Process();
    return 0;
}

Handle<Value> Image::Overlay(const Arguments& args) {
    HandleScope scope;
    Image* image = ObjectWrap::Unwrap<Image>(args.This());

    OPTIONAL_ARGUMENT_FUNCTION(1, callback);
    // TODO: Allow arbitrary RGBA buffers to be passed in.
    if (args.Length() < 1 || !Image::HasInstance(args[0])) {
        return ThrowException(Exception::TypeError(
            String::New("Image required as first argument")));
    }

    Image* overlay = ObjectWrap::Unwrap<Image>(args[0]->ToObject());
    Baton* baton = new OverlayBaton(image, callback, overlay);
    image->Schedule(EIO_BeginOverlay, baton);

    return args.This();
}

void Image::EIO_BeginOverlay(Baton* baton) {
    baton->image->locked = true;
    eio_custom(EIO_Overlay, EIO_PRI_DEFAULT, EIO_AfterOverlay, baton);
}

int Image::EIO_Overlay(eio_req *req) {
    OverlayBaton* baton = static_cast<OverlayBaton*>(req->data);
    Image* image = baton->image;

    assert(image->data != NULL);
    assert(baton->overlay->data != NULL);

    // TODO: Need better checks for this.
    assert(image->width == baton->overlay->width);
    assert(image->height == baton->overlay->height);

    for (unsigned long y = 0; y < image->height; y++) {
        int offset = 4 * y * image->width;
        unsigned int* src = (unsigned int*)(baton->overlay->data + offset);
        unsigned int* dst = (unsigned int*)(image->data + offset);

        for (unsigned long x = 0; x < image->width; x++) {
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

    return 0;
}

int Image::EIO_AfterOverlay(eio_req *req) {
    HandleScope scope;
    OverlayBaton* baton = static_cast<OverlayBaton*>(req->data);
    Image* image = baton->image;

    if (!baton->callback.IsEmpty() && baton->callback->IsFunction()) {
        Local<Value> argv[] = {
            Local<Value>::New(Null()),
            Local<Value>::New(image->handle_)
        };
        TRY_CATCH_CALL(image->handle_, baton->callback, 2, argv);
    }

    delete baton;
    image->locked = false;
    image->Process();
    return 0;
}

