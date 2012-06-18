/*
	This file is part of Heriswap.

	@author Soupe au Caillou - Pierre-Eric Pelloux-Prayer
	@author Soupe au Caillou - Gautier Pelloux-Prayer

	Heriswap is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, version 3.

	Heriswap is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Heriswap.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "AdAPIAndroidImpl.h"
#include "base/Log.h"
#include <string>

static jmethodID jniMethodLookup(JNIEnv* env, jclass c, const std::string& name, const std::string& signature) {
    jmethodID mId = env->GetStaticMethodID(c, name.c_str(), signature.c_str());
    if (!mId) {
        LOGW("JNI Error : could not find method '%s'/'%s'", name.c_str(), signature.c_str());
    }
    return mId;
}

struct AdAPIAndroidImpl::AdAPIAndroidImplData {
 jclass cls;
 jmethodID showAd;
 jmethodID done;
 bool initialized;
};

AdAPIAndroidImpl::AdAPIAndroidImpl() {
	datas = new AdAPIAndroidImplData();
	datas->initialized = false;
}

AdAPIAndroidImpl::~AdAPIAndroidImpl() {
	uninit();
    delete datas;
}

void AdAPIAndroidImpl::init(JNIEnv* pEnv) {
    env = pEnv;

    datas->cls = (jclass)env->NewGlobalRef(env->FindClass("net/damsy/soupeaucaillou/heriswap/HeriswapJNILib"));
    datas->showAd = jniMethodLookup(env, datas->cls, "showAd", "()Z");
    datas->done = jniMethodLookup(env, datas->cls, "done", "()Z");
    
    datas->initialized = true;
}

void AdAPIAndroidImpl::uninit() {
	if (datas->initialized) {
    	env->DeleteGlobalRef(datas->cls);
    	datas->initialized = false;
	}
}

bool AdAPIAndroidImpl::showAd() {
    return env->CallStaticBooleanMethod(datas->cls, datas->showAd);
}

bool AdAPIAndroidImpl::done() {
    env->CallStaticBooleanMethod(datas->cls, datas->done);
}

