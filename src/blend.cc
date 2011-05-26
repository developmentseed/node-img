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


int EIO_Blend(eio_req *req) {
    BlendBaton* baton = static_cast<BlendBaton*>(req->data);

    // EncodeBaton* encode = new EncodeBaton();
    unsigned long width;
    unsigned long height;
    bool alpha;
    unsigned char* result = NULL;
    
    PNGBuffer top = baton->buffers.back();
    ImageReader* reader = ImageReader::create(top.first, top.second);
    
    // // Iterate from last to first image.
    // PNGBuffers::reverse_iterator begin = baton->buffers.rbegin();
    // PNGBuffers::reverse_iterator end = baton->buffers.rend();
    // for (PNGBuffers::reverse_iterator image = begin; image < end && alpha; image++) {
    //     // current item: (*image)
    //     // read image header
    //     if (image == begin) {
    //     //     if (image has no alpha) {
    //             baton->result = (*image).first;
    //             baton->length = (*image).second;
    //             break;
    //     //     }
    //     //     decode image with alpha
    //     //     set result to decoded image
    //     } else {
    //     //     if (dimensions dont match) {
    //     //         baton->error = true;
    //     //         free(result);
    //     //         result = NULL;
    //     //         break;
    //     //     }
    //     //     alpha = image has alpha;
    //     //
    //     //     decode image with alpha
    //     //     blend result onto decoded image
    //     //     free(result)
    //     //     result = decoded image;
    //     }
    // }
    // 
    // if (result) {
    //     Blend_Encode(result, baton, width, height, alpha);
    // }

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
