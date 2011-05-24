#include <v8.h>
#include <node.h>

#include "image.h"

extern "C" void init (v8::Handle<v8::Object> target) {
    Image::Init(target);
}
