#include <cstdlib>
#include <unistd.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <fstream>
#include <android/log.h>
#include <sys/system_properties.h>

#include "zygisk.hpp"
#include "module.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

static std::vector<std::string> PkgList = {"com.google", "com.android.chrome","com.android.vending","com.breel.wallpapers20"};
static std::vector<std::string> P5 = {"com.google.android.googlequicksearchbox", "com.google.android.tts" , "com.google.android.apps.recorder","com.google.android.gms","com.google.android.apps.wearables.maestro.companion"};
static std::vector<std::string> P1 = {"com.google.android.apps.photos"};
static std::vector<std::string> keep = {"com.google.android.GoogleCamera","com.google.ar.core","com.google.vr.apps.ornament","com.google.android.youtube"};

class pixelify : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // Use JNI to fetch our process name
        const char *process = env->GetStringUTFChars(args->nice_name, nullptr);
        preSpecialize(process);
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        preSpecialize("system_server");
    }

    void injectBuild(const char *package_name,const char *model1,const char *product1,const char *finger1) {
        if (env == nullptr) {
            LOGW("failed to inject android.os.Build for %s due to env is null", package_name);
            return;
        }
        	
        jclass build_class = env->FindClass("android/os/Build");
        if (build_class == nullptr) {
            LOGW("failed to inject android.os.Build for %s due to build is null", package_name);
            return;
        } else {
        	LOGI("inject android.os.Build for %s with \nPRODUCT:%s \nMODEL:%s \nFINGERPRINT:%s", package_name,product1,model1,finger1);
        }

        jstring product = env->NewStringUTF(product1);
        jstring model = env->NewStringUTF(model1);
        jstring brand = env->NewStringUTF("google");
        jstring manufacturer = env->NewStringUTF("Google");
        jstring finger = env->NewStringUTF(finger1);

        jfieldID brand_id = env->GetStaticFieldID(build_class, "BRAND", "Ljava/lang/String;");
        if (brand_id != nullptr) {
            env->SetStaticObjectField(build_class, brand_id, brand);
        }

        jfieldID manufacturer_id = env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;");
        if (manufacturer_id != nullptr) {
            env->SetStaticObjectField(build_class, manufacturer_id, manufacturer);
        }

        jfieldID product_id = env->GetStaticFieldID(build_class, "PRODUCT", "Ljava/lang/String;");
        if (product_id != nullptr) {
            env->SetStaticObjectField(build_class, product_id, product);
        }

        jfieldID device_id = env->GetStaticFieldID(build_class, "DEVICE", "Ljava/lang/String;");
        if (device_id != nullptr) {
            env->SetStaticObjectField(build_class, device_id, product);
        }

        jfieldID model_id = env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;");
        if (model_id != nullptr) {
            env->SetStaticObjectField(build_class, model_id, model);
        }
        if (strcmp(finger1,"") != 0) {
	        jfieldID finger_id = env->GetStaticFieldID(build_class, "FINGERPRINT", "Ljava/lang/String;");
	        if (finger_id != nullptr) {
	            env->SetStaticObjectField(build_class, finger_id, finger);
	        }
    	}

        if(env->ExceptionCheck())
        {
            env->ExceptionClear();
        }

        env->DeleteLocalRef(brand);
        env->DeleteLocalRef(manufacturer);
        env->DeleteLocalRef(product);
        env->DeleteLocalRef(model);
        if (strcmp(finger1,"") != 0) {
        	env->DeleteLocalRef(finger);
    	}
    }

private:
    Api *api;
    JNIEnv *env;
    bool tensor =false;
    
    inline bool exist (const std::string& name) {
	    std::ifstream f(name.c_str());
	    return f.good();
	}

    void preSpecialize(const char *process) {
        unsigned r = 0;
        int fd = api->connectCompanion();
        read(fd, &r, sizeof(r));
        close(fd);

        if (exist("/data/adb/modules/Pixelify/tensor") == true) {
        	tensor=true;
        	LOGI("Adjusting spoof according to tensor chip");
        }

        std::string package_name = process;
        int type = 0;
        for (auto &s : PkgList) {
            if (package_name.find(s) != std::string::npos) {
                type=1;
                for (auto &s : P5) {
                    if (package_name.find(s) != std::string::npos) {
                        type=2;
                    }
                }
                for (auto &s : P1) {
                    if (package_name.find(s) != std::string::npos) {
                        type=3;
                    }
                }

                for (auto &s : keep) {
                    if (package_name.find(s) != std::string::npos) {
                        type=0;
                    }
                }

                if (tensor == true && type != 3) type=0;

                if (strcmp(process,"com.google.android.gms.unstable") == 0) type=0;

                if (type == 0) {
                    api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
                }  else if (type == 1) {
                    injectBuild(process,"Pixel 6 Pro","raven","google/raven/raven:12/SQ1D.220205.003/8069835:user/release-keys");
                } else if (type == 2) {
                    injectBuild(process,"Pixel 5","raven","google/raven/raven:12/SQ1D.220205.003/8069835:user/release-keys");
                } else if (type == 3) {
                    injectBuild(process,"Pixel XL","marlin","google/marlin/marlin:10/QP1A.191005.007.A3/5972272:user/release-keys");
                }
            }
        }
        if (type == 0) api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

};

static int urandom = -1;

static void companion_handler(int i) {
    if (urandom < 0) {
        urandom = open("/dev/urandom", O_RDONLY);
    }
    unsigned r;
    read(urandom, &r, sizeof(r));
    LOGD("example: companion r=[%u]\n", r);
    write(i, &r, sizeof(r));
}

REGISTER_ZYGISK_MODULE(pixelify)
REGISTER_ZYGISK_COMPANION(companion_handler)
