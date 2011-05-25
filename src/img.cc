#include <v8.h>
#include <node.h>

#include "image.h"
#include "macros.h"

extern "C" void init (v8::Handle<v8::Object> target) {
    Image::Init(target);

    DEFINE_CONSTANT_STRING(target, PNG_LIBPNG_VER_STRING, libpng);
}
