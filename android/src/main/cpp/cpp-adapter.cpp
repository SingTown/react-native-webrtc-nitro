#include <jni.h>
#include "WebrtcOnLoad.hpp"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return margelo::nitro::webrtc::initialize(vm);
}
