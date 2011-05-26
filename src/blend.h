#ifndef NODE_IMG_SRC_BLEND_H
#define NODE_IMG_SRC_BLEND_H

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

Handle<Value> Blend(const Arguments& args);
int EIO_Blend(eio_req *req);
int EIO_AfterBlend(eio_req *req);

#endif