#ifndef NODE_IMAGEBLEND_SRC_IMAGE_H
#define NODE_IMAGEBLEND_SRC_IMAGE_H

#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>

#include <queue>

using namespace v8;
using namespace node;

class Image : public ObjectWrap {
    class Baton {
    public:
        Image* image;
        Persistent<Function> callback;

        Baton(Image* img, Handle<Function> cb) : image(img) {
            ev_ref(EV_DEFAULT_UC);
            image->Ref();
            callback = Persistent<Function>::New(cb);
        }
        ~Baton() {
            ev_unref(EV_DEFAULT_UC);
            image->Unref();
            callback.Dispose();
        }
    };

    class BufferBaton : public Baton {
    public:
        Persistent<Object> buffer;
        size_t length;
        char* data;
        BufferBaton(Image* img, Handle<Function> cb, Handle<Object> buf) : Baton(img, cb) {
            buffer = Persistent<Object>::New(buf);
            data = Buffer::Data(buf);
            length = Buffer::Length(buf);
        }
        ~BufferBaton() {
            buffer.Dispose();
        }
    };

    typedef void (*EIO_Callback)(Baton* baton);

    struct Call {
        Call(EIO_Callback cb_, Baton* baton_) : callback(cb_), baton(baton_) {};
        EIO_Callback callback;
        Baton* baton;
    };


public:
    static Persistent<FunctionTemplate> constructor_template;
    static void Init(Handle<Object> target);

protected:
    Image() :
        ObjectWrap(),
        locked(false),
        width(0),
        height(0) {}
    static Handle<Value> New(const Arguments& args);

    static Handle<Value> GetWidth(Local<String> name, const AccessorInfo& info);
    static Handle<Value> GetHeight(Local<String> name, const AccessorInfo& info);

    void Schedule(EIO_Callback callback, Baton* baton);
    void Process();

    static Handle<Value> Load(const Arguments& args);
    static void EIO_BeginLoad(Baton* baton);
    static int EIO_Load(eio_req *req);
    static int EIO_AfterLoad(eio_req *req);

    bool locked;
    std::queue<Call*> queue;

    int width;
    int height;
};





#endif