#ifndef UTILS_H_
#define UTILS_H_
#include <jni.h>

void setAPK (const char* apkPath);

extern JavaVM* g_jvm;

// Convenience class to get the java environment for the current thread.
// Also attach the threads, and detach it on destruction, if needed.
class JVMAttacher {
public:
    JVMAttacher() : _env(NULL), _detach_thread(false) {
    }
    JNIEnv *getEnv()
    {
        if (_env == NULL)
        {
            if (g_jvm == NULL) {
                die("g_jvm == NULL");
                return NULL;
            }
            int rc = g_jvm->GetEnv((void **)&_env, JNI_VERSION_1_6);
            if (rc  == JNI_EDETACHED) {
                if (g_jvm->AttachCurrentThread(&_env, NULL) != 0) {
                    die("AttachCurrentThread failed");
                    return NULL;
                }
                _detach_thread = true;
            }
            else if (rc == JNI_EVERSION) {
                die("JNI version error");
                return NULL;
            }
        }
        return _env;
    }

    ~JVMAttacher()
    {
        if (_detach_thread)
            g_jvm->DetachCurrentThread();
    }

private:
    JNIEnv *_env;
    bool _detach_thread;
};

extern thread_local JVMAttacher jvm_attacher;

#endif /* UTILS_H_ */
