/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.dexdeps;

import java.util.Arrays;
import java.util.List;

@SuppressWarnings("PMD")
public class MethodRef {
    private String mDeclClass, mReturnType, mMethodName;
    private String[] mArgTypes;

    /**
     * Initializes a new field reference.
     */
    public MethodRef(String declClass, String[] argTypes, String returnType,
            String methodName) {
        mDeclClass = declClass;
        mArgTypes = Arrays.copyOf(argTypes, argTypes.length);
        mReturnType = returnType;
        mMethodName = methodName;
    }

    /**
     * Gets the name of the method's declaring class.
     */
    public String getDeclClassName() {
        return mDeclClass;
    }

    /**
     * Gets the method's descriptor.
     */
    public String getDescriptor() {
        return descriptorFromProtoArray(mArgTypes, mReturnType);
    }

    /**
     * Gets the method's name.
     */
    public String getName() {
        return mMethodName;
    }

    /**
     * Gets an array of method argument types.
     */
    public List<String> getArgumentTypeNames() {
        return Arrays.asList(mArgTypes);
    }

    /**
     * Gets the method's return type.  Examples: "Ljava/lang/String;", "[I".
     */
    public String getReturnTypeName() {
        return mReturnType;
    }

    /**
     * Returns the method descriptor, given the argument and return type
     * prototype strings.
     */
    private static String descriptorFromProtoArray(String[] protos,
            String returnType) {
        StringBuilder builder = new StringBuilder();

        builder.append("(");
        for (int i = 0; i < protos.length; i++) {
            builder.append(protos[i]);
        }

        builder.append(")");
        builder.append(returnType);

        return builder.toString();
    }
}
