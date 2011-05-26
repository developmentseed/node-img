#ifndef NODE_IMG_SRC_IMAGE_H
#define NODE_IMG_SRC_IMAGE_H

#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <png.h>

#include <cstdlib>
#include <cstring>

#include <string>
#include <queue>

using namespace v8;
using namespace node;

class Image : public EventEmitter {
    class Baton {
    public:
        Image* image;
        Persistent<Function> callback;
        int error;
        std::string message;

        Baton(Image* img, Handle<Function> cb) : image(img), error(0) {
            ev_ref(EV_DEFAULT_UC);
            image->Ref();
            callback = Persistent<Function>::New(cb);
        }
        virtual bool precondition(Baton* baton) {
            return baton->image->data != NULL;
        }
        ~Baton() {
            ev_unref(EV_DEFAULT_UC);
            image->Unref();
            callback.Dispose();
        }
    };

    class LoadBaton : public Baton {
    public:
        Persistent<Object> buffer;
        size_t length;
        char* data;
        int pos;

        LoadBaton(Image* img, Handle<Function> cb, Handle<Object> buf) : Baton(img, cb), pos(0) {
            buffer = Persistent<Object>::New(buf);
            data = Buffer::Data(buf);
            length = Buffer::Length(buf);
        }
        virtual bool precondition(Baton* baton) {
            return true;
        }
        ~LoadBaton() {
            buffer.Dispose();
        }
    };

    class AsPNGBaton : public Baton {
    public:
        size_t length;
        size_t max;
        char* data;

        AsPNGBaton(Image* img, Handle<Function> cb) : Baton(img, cb), length(0), max(0), data(NULL) {}
        ~AsPNGBaton() {
            if (data != NULL) {
                free(data);
            }
        }
    };

    class OverlayBaton: public Baton {
    public:
        Image* overlay;
        OverlayBaton(Image* img, Handle<Function> cb, Image* ovl) : Baton(img, cb), overlay(ovl) {
            overlay->Ref();
        }
        virtual bool precondition(Baton* baton) {
            return ((OverlayBaton*)baton)->overlay->data != NULL;
        }
        ~OverlayBaton() {
            overlay->Unref();
        }
    };

    typedef void (*EIO_Callback)(Baton* baton);

    struct Call {
        Call(EIO_Callback cb_, Baton* baton_)
            : callback(cb_), baton(baton_) {};
        EIO_Callback callback;
        Baton* baton;
    };


public:
    static Persistent<FunctionTemplate> constructor_template;
    static void Init(Handle<Object> target);

protected:
    Image() : EventEmitter(),
        locked(false),
        width(0),
        height(0),
        data(NULL) {}
    ~Image() {
        if (data != NULL) {
            delete[] data;
        }
    }
    static Handle<Value> New(const Arguments& args);

    static inline bool HasInstance(Handle<Value> val) {
        if (!val->IsObject()) return false;
        Local<Object> obj = val->ToObject();
        return constructor_template->HasInstance(obj);
    }

    static Handle<Value> GetWidth(Local<String> name, const AccessorInfo& info);
    static Handle<Value> GetHeight(Local<String> name, const AccessorInfo& info);
    static Handle<Value> GetData(Local<String> name, const AccessorInfo& info);

    void Schedule(EIO_Callback callback, Baton* baton);
    void Process();

    static Handle<Value> Process(const Arguments& args);

    static Handle<Value> Load(const Arguments& args);
    static void EIO_BeginLoad(Baton* baton);
    static int EIO_Load(eio_req *req);
    static int EIO_AfterLoad(eio_req *req);

    static void readPNG(png_structp png_ptr, png_bytep data, png_size_t length);
    static void writePNG(png_structp png_ptr, png_bytep data, png_size_t length);

    static Handle<Value> AsPNG(const Arguments& args);
    static void EIO_BeginAsPNG(Baton* baton);
    static int EIO_AsPNG(eio_req *req);
    static int EIO_AfterAsPNG(eio_req *req);

    static Handle<Value> Overlay(const Arguments& args);
    static void EIO_BeginOverlay(Baton* baton);
    static int EIO_Overlay(eio_req *req);
    static int EIO_AfterOverlay(eio_req *req);

    bool locked;
    std::queue<Call*> queue;

    unsigned long width;
    unsigned long height;
    char* data;
};





#endif
