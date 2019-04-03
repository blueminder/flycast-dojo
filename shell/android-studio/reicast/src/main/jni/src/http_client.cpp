#include <string>
#include <map>
#include <jni.h>
#include "types.h"
#include "gui/http_client.h"
#include "utils.h"

static jobject HttpClient;
static jmethodID openUrlMid;

void http_init()
{

}

int http_open_url(const std::string& url, std::vector<u8>& content, std::string& content_type)
{
	JNIEnv *env = jvm_attacher.getEnv();
	jstring jurl = env->NewStringUTF(url.c_str());
	jclass byteArrayClass = env->FindClass("[B");
	jobjectArray content_out = env->NewObjectArray(1, byteArrayClass, NULL);
	jclass stringClass = env->FindClass("java/lang/String");
	jobjectArray content_type_out = env->NewObjectArray(1, stringClass, NULL);

	int http_code = env->CallIntMethod(HttpClient, openUrlMid, jurl, content_out, content_type_out);

	jbyteArray jcontent = (jbyteArray)env->GetObjectArrayElement(content_out, 0);
	if (jcontent != NULL)
	{
	    int content_len = env->GetArrayLength(jcontent);
	    jbyte *data = (jbyte *)malloc(content_len);
	    env->GetByteArrayRegion(jcontent, 0, content_len, data);
	    content = std::vector<u8>(data, data + content_len);
		env->DeleteLocalRef(jcontent);
		free(data);
	}
	jstring jcontent_type = (jstring)env->GetObjectArrayElement(content_type_out, 0);
	if (jcontent_type != NULL)
	{
		const char *data = env->GetStringUTFChars(jcontent_type, 0);
		content_type = data;
		env->ReleaseStringUTFChars(jcontent_type, data);
		env->DeleteLocalRef(jcontent_type);
	}
	env->DeleteLocalRef(content_type_out);
	env->DeleteLocalRef(content_out);
	env->DeleteLocalRef(jurl);

	return http_code;
}

void http_term()
{

}

extern "C" {
JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_HttpClient_nativeInit(JNIEnv *env, jobject obj)  __attribute__((visibility("default")));
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_emu_HttpClient_nativeInit(JNIEnv *env, jobject obj)
{
	HttpClient = env->NewGlobalRef(obj);
	openUrlMid = env->GetMethodID(env->GetObjectClass(obj), "openUrl", "(Ljava/lang/String;[[B[Ljava/lang/String;)I");
}
