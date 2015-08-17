#include <nan.h>
#include <v8-profiler.h>
#include <stdlib.h>
#if defined(_WIN32)
  #include <time.h>
  #define snprintf _snprintf
#else
  #include <sys/time.h>
#endif

using namespace v8;

class FileOutputStream: public OutputStream {
  public:
    FileOutputStream(FILE* stream): stream_(stream) { }
    virtual int GetChunkSize() {
      return 65536;
    }
    virtual void EndOfStream() { }
    virtual WriteResult WriteAsciiChunk(char* data, int size) {
      const size_t len = static_cast<size_t>(size);
      size_t off = 0;
      while (off < len && !feof(stream_) && !ferror(stream_))
        off += fwrite(data + off, 1, len - off, stream_);
      return off == len ? kContinue : kAbort;
    }

  private:
    FILE* stream_;
};

static void OnFatalError(const char* location, const char* message) {
  if (location)
    fprintf(stderr, "FATAL ERROR: %s %s\n", location, message);
  else
    fprintf(stderr, "FATAL ERROR: %s\n", message);

  // time_t rawtime;
  // struct tm* timeinfo;
  // time(&rawtime);
  // timeinfo = localtime(&rawtime);

  // char filename[256];
  // strftime(filename, sizeof(filename),"%Y%m%dT%H%M%S.heapsnapshot", timeinfo);
  char filename[] = "/tmp/node_fatal_error.heapsnapshot";

  fprintf(stderr, "Writing heap snapshot to %s\n", filename);

  FILE* fp = fopen(filename, "w");
  if (fp == NULL) {
    fprintf(stderr, "Unable to write heap snapshot\n");
    abort();
  }

#if NODE_VERSION_AT_LEAST(0, 11, 13)
  Isolate* isolate = Isolate::GetCurrent();
  const HeapSnapshot* snap =
      isolate->GetHeapProfiler()->TakeHeapSnapshot(String::Empty(isolate));
#else
  const HeapSnapshot* snap = HeapProfiler::TakeSnapshot(String::Empty());
#endif
  FileOutputStream stream(fp);
  snap->Serialize(&stream, HeapSnapshot::kJSON);
  fclose(fp);

  fprintf(stderr, "Heap dump complete\n");

  exit(1);
}

NAN_METHOD(Method) {
  NanScope();
  V8::SetFatalErrorHandler(OnFatalError);
  NanReturnValue(NanNew("done"));
}

void Init(Handle<Object> target) {
  V8::SetFatalErrorHandler(OnFatalError);
  target->Set(NanNew("call"),
      NanNew<FunctionTemplate>(Method)->GetFunction());
}

NODE_MODULE(ofe, Init)
