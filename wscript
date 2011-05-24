import Options
from os.path import exists
from shutil import copy2 as copy

TARGET = 'img'
TARGET_FILE = '%s.node' % TARGET

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")
  conf.check(lib='png', libpath=['/usr/local/lib', '/opt/local/lib'])

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.cxxflags = ["-g", "-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-Wall"]
  obj.target = TARGET
  obj.source = "src/img.cc src/image.cc"
  obj.uselib = "PNG"

def shutdown():
  if Options.commands['clean']:
    if exists(TARGET_FILE):
      unlink(TARGET_FILE)
