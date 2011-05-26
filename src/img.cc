#include <v8.h>
#include <node.h>

#include "image.h"
#include "blend.h"
#include "macros.h"

extern "C" void init (v8::Handle<v8::Object> target) {
    Image::Init(target);

    NODE_SET_METHOD(target, "blend", Blend);

    DEFINE_CONSTANT_STRING(target, PNG_LIBPNG_VER_STRING, libpng);
}
