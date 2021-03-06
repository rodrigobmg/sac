/*
    This file is part of Soupe Au Caillou.

    @author Soupe au Caillou - Jordane Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer
    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer

    Soupe Au Caillou is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    Soupe Au Caillou is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Soupe Au Caillou.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <jni.h>
#include <map>
#include <string>
#include <sstream>

#include "base/Log.h"

class JNIHelper {
    public:
    static jclass findClass(JNIEnv* env, const std::string& className);
    static jmethodID findStaticMethod(JNIEnv* env,
                                      jclass c,
                                      const std::string& name,
                                      const std::string& signature);
    static jmethodID findMethod(JNIEnv* env,
                                jclass c,
                                const std::string& name,
                                const std::string& signature);
};

template <typename T> class JNIWrapper {
    public:
    JNIWrapper(const std::string& pClassName, bool pHasInstance)
        : className(pClassName), hasInstance(pHasInstance) {}

    void
    declareMethod(T t, const std::string& name, const std::string& signature) {
        LOGE_IF(methodNamesSignatures.find(t) != methodNamesSignatures.end(),
                "Method with id '" << t
                                   << "' already declared (class=" << className
                                   << ", signature=" << signature << ")");
        methodNamesSignatures.insert(
            std::make_pair(t, std::make_pair(name, signature)));
    }

    void init(JNIEnv* pEnv) {
        LOGV(1, "Initialisation JNI class wrapper: " << className);
        // store JNIEnv
        env = pEnv;
        // store class
        javaClass = JNIHelper::findClass(env, className);
        // store class instance if requested
        if (hasInstance) {
            std::stringstream getInstanceSign;
            getInstanceSign << "()L" << className << ';';
            jmethodID getInstance = JNIHelper::findStaticMethod(
                env, javaClass, "Instance", getInstanceSign.str());
            instance = env->NewGlobalRef(
                env->CallStaticObjectMethod(javaClass, getInstance));
            if (!instance) {
                LOGF("Could not retrieve '" << className << "' instance");
            }
        }
        LOGV(1, "JNI method count " << methodNamesSignatures.size());
        // lookup methods
        for (typename std::map<T, std::pair<std::string, std::string>>::
                 const_iterator it = methodNamesSignatures.begin();
             it != methodNamesSignatures.end();
             ++it) {
            const std::string& name = it->second.first;
            const std::string& sign = it->second.second;
            jmethodID mid = 0;
            if (hasInstance) {
                mid = JNIHelper::findMethod(env, javaClass, name, sign);
            } else {
                mid = JNIHelper::findStaticMethod(env, javaClass, name, sign);
            }
            methods.insert(std::make_pair(it->first, mid));
        }
    }

    private:
    std::string className;
    bool hasInstance;
    std::map<T, std::pair<std::string, std::string>> methodNamesSignatures;

    protected:
    JNIEnv* env;
    jclass javaClass;
    jobject instance;
    std::map<T, jmethodID> methods;
};
