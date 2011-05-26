#ifndef NODE_IMG_SRC_MACROS_H
#define NODE_IMG_SRC_MACROS_H

#define REQUIRE_ARGUMENTS(n)                                                   \
    if (args.Length() < (n)) {                                                 \
        return ThrowException(                                                 \
            Exception::TypeError(String::New("Expected " #n "arguments"))      \
        );                                                                     \
    }


#define REQUIRE_ARGUMENT_EXTERNAL(i, var)                                      \
    if (args.Length() <= (i) || !args[i]->IsExternal()) {                      \
        return ThrowException(                                                 \
            Exception::TypeError(String::New("Argument " #i " invalid"))       \
        );                                                                     \
    }                                                                          \
    Local<External> var = Local<External>::Cast(args[i]);


#define REQUIRE_ARGUMENT_FUNCTION(i, var)                                      \
    if (args.Length() <= (i) || !args[i]->IsFunction()) {                      \
        return ThrowException(Exception::TypeError(                            \
            String::New("Argument " #i " must be a function"))                 \
        );                                                                     \
    }                                                                          \
    Local<Function> var = Local<Function>::Cast(args[i]);


#define REQUIRE_ARGUMENT_STRING(i, var)                                        \
    if (args.Length() <= (i) || !args[i]->IsString()) {                        \
        return ThrowException(Exception::TypeError(                            \
            String::New("Argument " #i " must be a string"))                   \
        );                                                                     \
    }                                                                          \
    String::Utf8Value var(args[i]->ToString());


#define OPTIONAL_ARGUMENT_FUNCTION(i, var)                                     \
    Local<Function> var;                                                       \
    if (args.Length() > i && !args[i]->IsUndefined()) {                        \
        if (!args[i]->IsFunction()) {                                          \
            return ThrowException(Exception::TypeError(                        \
                String::New("Argument " #i " must be a function"))             \
            );                                                                 \
        }                                                                      \
        var = Local<Function>::Cast(args[i]);                                  \
    }


#define OPTIONAL_ARGUMENT_INTEGER(i, var, default)                             \
    int var;                                                                   \
    if (args.Length() <= (i)) {                                                \
        var = (default);                                                       \
    }                                                                          \
    else if (args[i]->IsInt32()) {                                             \
        var = args[i]->Int32Value();                                           \
    }                                                                          \
    else {                                                                     \
        return ThrowException(Exception::TypeError(                            \
            String::New("Argument " #i " must be an integer"))                 \
        );                                                                     \
    }


#define DEFINE_CONSTANT_INTEGER(target, constant, name)                        \
    (target)->Set(                                                             \
        String::NewSymbol(#name),                                              \
        Integer::New(constant),                                                \
        static_cast<PropertyAttribute>(ReadOnly | DontDelete)                  \
    );

#define DEFINE_CONSTANT_STRING(target, constant, name)                         \
    (target)->Set(                                                             \
        String::NewSymbol(#name),                                              \
        String::NewSymbol(constant),                                           \
        static_cast<PropertyAttribute>(ReadOnly | DontDelete)                  \
    );

#define EMIT_EVENT(obj, argc, argv)                                            \
    TRY_CATCH_CALL((obj),                                                      \
        Local<Function>::Cast((obj)->Get(String::NewSymbol("emit"))),          \
        argc, argv                                                             \
    );

#define TRY_CATCH_CALL(context, callback, argc, argv)                          \
{   TryCatch try_catch;                                                        \
    (callback)->Call((context), (argc), (argv));                               \
    if (try_catch.HasCaught()) {                                               \
        FatalException(try_catch);                                             \
    }                                                                          }

#define EIO_DEFINITION(name)                                                   \
    static Handle<Value> name(const Arguments& args);                          \
    static void EIO_Begin##name(Baton* baton);                                 \
    static int EIO_##name(eio_req *req);                                       \
    static int EIO_After##name(eio_req *req);

#endif
