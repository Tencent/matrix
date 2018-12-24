/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "loader.h"

//
// Created by liyongjie on 2017/2/16.
//

using namespace std;
struct JNIModule
{
	const char *name;
	ModuleInitializer func;
	bool init;

	JNIModule(const char *name_, ModuleInitializer func_, bool init_)
		: name(name_), func(func_), init(init_) {}
};
static vector<JNIModule> *g_loaders = nullptr;


void register_module_func(const char *name, ModuleInitializer func, int init)
{
	if (!g_loaders)
		g_loaders = new vector<JNIModule>();

	g_loaders->push_back(JNIModule(name, func, !!init));
}


extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv* env;
	if (vm->GetEnv((void**) (&env), JNI_VERSION_1_6) != JNI_OK) {
        sqlitelint::LOGE( "Initialize GetEnv null");
		return -1;
	}

	vector<JNIModule>::iterator e = g_loaders->end();
	for (vector<JNIModule>::iterator it = g_loaders->begin(); it != e; ++it)
	{
		if (it->init)
		{
            sqlitelint::LOGI( "Initialize module '%s'...", it->name);
			if (it->func(vm, env) != 0)
				return -1;
		}
	}

	return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
	JNIEnv* env;
	if (vm->GetEnv((void**) (&env), JNI_VERSION_1_6) != JNI_OK) {
        sqlitelint::LOGE( "Finalize GetEnv null");
		return;
	}

	vector<JNIModule>::iterator e = g_loaders->end();
	for (vector<JNIModule>::iterator it = g_loaders->begin(); it != e; ++it)
	{
		if (!it->init)
		{
            sqlitelint::LOGI("Finalize module '%s'...", it->name);
			it->func(vm, env);
		}
	}
}
