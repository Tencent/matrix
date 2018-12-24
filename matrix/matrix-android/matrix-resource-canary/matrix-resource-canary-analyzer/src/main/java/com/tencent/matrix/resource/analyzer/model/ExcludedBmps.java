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

package com.tencent.matrix.resource.analyzer.model;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Created by tangyinsheng on 2017/8/9.
 */

public final class ExcludedBmps extends ExcludedRefs {
    public final Set<PatternInfo> mClassNamePatterns;

    public static final class PatternInfo {
        public Pattern mPattern;
        public boolean mForGCRootOnly;

        PatternInfo(Pattern pattern, boolean forGCRootOnly) {
            mPattern = pattern;
            mForGCRootOnly = forGCRootOnly;
        }
    }

    ExcludedBmps(BuilderWithParams builder) {
        super(builder);
        mClassNamePatterns = Collections.unmodifiableSet(builder.mClassNamePatterns);
    }

    public static Builder builder() {
        return new BuilderWithParams();
    }

    public interface Builder extends ExcludedRefs.Builder {
        Builder addClassNamePattern(String regex, boolean forGCRootOnly);
        ExcludedBmps build();
    }

    static final class BuilderWithParams extends ExcludedRefs.BuilderWithParams implements Builder {
        final Set<PatternInfo> mClassNamePatterns = new HashSet<>();

        @Override
        public Builder addClassNamePattern(String regex, boolean forGCRootOnly) {
            if (regex == null || regex.length() == 0) {
                throw new IllegalArgumentException("bad regex: " + regex);
            }
            mClassNamePatterns.add(new PatternInfo(Pattern.compile(regex), forGCRootOnly));
            return this;
        }

        @Override
        public ExcludedBmps build() {
            return new ExcludedBmps(this);
        }
    }
}
