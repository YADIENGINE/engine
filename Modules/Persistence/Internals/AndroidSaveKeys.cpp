#include "Modules/Persistence/Internals/EncryptionKeyStore.h"
#include <android_native_app_glue.h>
#include <jni.h>
#include <array>
#include <string>
#include "Modules/Persistence/Persistence.hpp"
extern "C" android_app* GetAndroidApp(void);

namespace {
void CheckJava(JNIEnv* env) {
    if (!env->ExceptionCheck()) return;
    const auto failure = env->ExceptionOccurred();
    env->ExceptionClear();
    bool invalidKey = false;
    for (const auto* name : std::array{"java/security/InvalidKeyException", "java/security/UnrecoverableKeyException",
                                       "javax/crypto/AEADBadTagException"}) {
        const auto type = env->FindClass(name);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            continue;
        }
        if (type && env->IsInstanceOf(failure, type)) invalidKey = true;
    }
    throw SaveError(invalidKey ? SaveErrorCode::KeyUnavailable : SaveErrorCode::PlatformUnavailable,
                    "Android Keystore operation failed");
}
std::string InvokeKeyBridge(const std::string& alias, std::string_view bytes, bool encrypt, bool reset) {
    const auto* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm || !app->activity->clazz)
        throw SaveError(SaveErrorCode::PlatformUnavailable, "Android activity is unavailable");
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    const auto status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    bool attached = false;
    if (status == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            throw SaveError(SaveErrorCode::PlatformUnavailable, "Could not attach Android save thread");
        attached = true;
    } else if (status != JNI_OK) {
        throw SaveError(SaveErrorCode::PlatformUnavailable, "Android JNI is unavailable");
    }
    if (env->PushLocalFrame(32) != JNI_OK) {
        env->ExceptionClear();
        if (attached) vm->DetachCurrentThread();
        throw SaveError(SaveErrorCode::PlatformUnavailable, "Could not allocate Android JNI frame");
    }
    try {
        const auto activityType = env->GetObjectClass(app->activity->clazz);
        CheckJava(env);
        const auto getLoader = env->GetMethodID(activityType, "getClassLoader", "()Ljava/lang/ClassLoader;");
        CheckJava(env);
        const auto loader = env->CallObjectMethod(app->activity->clazz, getLoader);
        CheckJava(env);
        const auto loaderType = env->GetObjectClass(loader);
        CheckJava(env);
        const auto loadClass = env->GetMethodID(loaderType, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        CheckJava(env);
        const auto className = env->NewStringUTF("org.zortengine.persistence.SaveKeyBridge");
        CheckJava(env);
        const auto helper = static_cast<jclass>(env->CallObjectMethod(loader, loadClass, className));
        CheckJava(env);
        const auto name = env->NewStringUTF(alias.c_str());
        CheckJava(env);
        std::string result;
        if (reset) {
            const auto method = env->GetStaticMethodID(helper, "reset", "(Ljava/lang/String;)V");
            CheckJava(env);
            env->CallStaticVoidMethod(helper, method, name);
            CheckJava(env);
        } else {
            const auto method = env->GetStaticMethodID(helper, "process", "(Ljava/lang/String;[BZ)[B");
            CheckJava(env);
            const auto input = env->NewByteArray(static_cast<jsize>(bytes.size()));
            CheckJava(env);
            env->SetByteArrayRegion(input, 0, static_cast<jsize>(bytes.size()),
                                    reinterpret_cast<const jbyte*>(bytes.data()));
            CheckJava(env);
            const auto output = static_cast<jbyteArray>(
                env->CallStaticObjectMethod(helper, method, name, input, static_cast<jboolean>(encrypt)));
            CheckJava(env);
            if (!output) throw SaveError(SaveErrorCode::PlatformUnavailable, "Android key bridge returned no data");
            result.resize(static_cast<std::size_t>(env->GetArrayLength(output)));
            env->GetByteArrayRegion(output, 0, static_cast<jsize>(result.size()),
                                    reinterpret_cast<jbyte*>(result.data()));
            CheckJava(env);
            // Clear the Java copy of the unwrapped data key after copying it to native memory.
            if (!encrypt && !result.empty()) {
                const std::string zeros(result.size(), '\0');
                env->SetByteArrayRegion(output, 0, static_cast<jsize>(zeros.size()),
                                        reinterpret_cast<const jbyte*>(zeros.data()));
                CheckJava(env);
            }
        }
        env->PopLocalFrame(nullptr);
        if (attached) vm->DetachCurrentThread();
        return result;
    } catch (...) {
        env->ExceptionClear();
        env->PopLocalFrame(nullptr);
        if (attached) vm->DetachCurrentThread();
        throw;
    }
}
} // namespace

EncryptionKeyStore CreatePersistenceKeyStore(const SaveKeySettings& settings) {
    if (settings.applicationId.empty())
        throw std::invalid_argument("Android encrypted persistence requires an application ID");
    const auto alias = settings.applicationId + ".engine.save.wrap.v1";
    return EncryptionKeyStore(
        settings.file,
        [alias](std::string_view bytes, bool encrypt) { return InvokeKeyBridge(alias, bytes, encrypt, false); },
        [alias] { (void)InvokeKeyBridge(alias, {}, false, true); });
}
