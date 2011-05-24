#include <string.h>
#include <v8.h>
#include <node.h>
#include <node_events.h>

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

    Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
    instance_template->SetAccessor(String::NewSymbol("width"), GetWidth);
    instance_template->SetAccessor(String::NewSymbol("height"), GetHeight);

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
        queue.pop();
        call->callback(call->baton);
        delete call;
    }
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

    Baton* baton = new BufferBaton(image, callback, args[0]->ToObject());
    image->Schedule(EIO_BeginLoad, baton);

    return args.This();
}

void Image::EIO_BeginLoad(Baton* baton) {
    baton->image->locked = true;
    eio_custom(EIO_Load, EIO_PRI_DEFAULT, EIO_AfterLoad, baton);
}

int Image::EIO_Load(eio_req *req) {
    BufferBaton* baton = static_cast<BufferBaton*>(req->data);
    Image* image = baton->image;

    return 0;
}

int Image::EIO_AfterLoad(eio_req *req) {
    HandleScope scope;
    BufferBaton* baton = static_cast<BufferBaton*>(req->data);
    Image* image = baton->image;

    delete baton;
    image->locked = false;
    image->Process();
    return 0;
}
