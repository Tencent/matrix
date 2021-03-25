///*
// * Tencent is pleased to support the open source community by making wechat-matrix available.
// * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
// * Licensed under the BSD 3-Clause License (the "License");
// * you may not use this file except in compliance with the License.
// * You may obtain a copy of the License at
// *
// *      https://opensource.org/licenses/BSD-3-Clause
// *
// * Unless required by applicable law or agreed to in writing,
// * software distributed under the License is distributed on an "AS IS" BASIS,
// * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// * See the License for the specific language governing permissions and
// * limitations under the License.
// */
//
//package com.tencent.matrix.batterycanary;
//
//import org.junit.Assert;
//import org.junit.Test;
//import org.junit.runner.RunWith;
//
//import mockit.Mock;
//import mockit.MockUp;
//import mockit.integration.junit4.JMockit;
//
//import static org.junit.Assert.assertEquals;
//
///**
// * Example local unit test, which will execute on the development machine (host).
// *
// * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
// */
//@SuppressWarnings("SpellCheckingInspection")
//@RunWith(JMockit.class)
//public class JMockitTest {
//
//    public static class JMock extends MockUp<FinalCounter> {
//        @Mock
//        public static int getCount() {
//            return -1;
//        }
//
//        @Mock
//        public static int getFinalCount() {
//            return -2;
//        }
//    }
//
//    @Test
//    public void addition_isCorrect() throws Exception {
//        assertEquals(4, 2 + 2);
//    }
//
//    @Test
//    public void testGetMockCount() {
//        Assert.assertEquals(1, FinalCounter.getCount());
//        new JMock();
//        Assert.assertEquals(-1, FinalCounter.getCount());
//    }
//
//    @Test
//    public void testGetMockFinalCount() {
//        Assert.assertEquals(1, FinalCounter.getFinalCount());
//        new JMock();
//        Assert.assertEquals(-2, FinalCounter.getFinalCount());
//    }
//
//    public static final class FinalCounter {
//        public static int getCount() {
//            return 1;
//        }
//
//        public static int getFinalCount() {
//            return 1;
//        }
//    }
//}
