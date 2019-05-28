package com.tencent.matrix.trace.config;

import com.tencent.mrs.plugin.IDynamicConfig;

import junit.framework.TestCase;

import org.junit.Before;
import org.junit.Test;

import java.util.Set;

public class TraceConfigTestCase extends TestCase {

    private IDynamicConfig dynamicConfig ;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        dynamicConfig = new MockDynamicConfig() ;
    }

    /**
     * 测试设置了 dynamicConfig, 但是没有设置 splash activities的场景
     */
    @Test
    public void testNotSetSplashActivities() {
        TraceConfig config = new TraceConfig.Builder().dynamicConfig(dynamicConfig).build() ;

        Set<String> activities = config.getSplashActivities() ;
        assertNotNull(activities);
        assertFalse( activities.contains("SplashActivity"));
    }

    /**
     * 测试 dynamicConfig 和 splashActivities 都为 null的场景不会产生crash
     */
    @Test
    public void testNotSetDynamicConfig() {
        TraceConfig config = new TraceConfig.Builder().build() ;
        Set<String> activities = config.getSplashActivities() ;
        assertNotNull(activities);
        assertEquals(0, activities.size());
        assertFalse( activities.contains("SplashActivity"));
    }

    /**
     * 模拟通过 dynamicConfig 设置 splashActivities
     */
    @Test
    public void testSetSplashActivities() {
        final String splashActivities = "com.tencent.matrix.SplashActivity1;com.tencent.matrix.SplashActivity2";
        // 设置
        TraceConfig config = new TraceConfig.Builder().dynamicConfig(new MockDynamicConfig() {
            @Override
            public String get(String key, String defStr) {
                if ( IDynamicConfig.ExptEnum.clicfg_matrix_trace_care_scene_set.name().equalsIgnoreCase(key) ) {
                    return splashActivities ;
                }
                return super.get(key, defStr);
            }
        }).build() ;

        Set<String> activities = config.getSplashActivities() ;
        assertNotNull(activities);
        assertEquals(2, activities.size());
        assertTrue( activities.contains("com.tencent.matrix.SplashActivity1"));
        assertTrue( activities.contains("com.tencent.matrix.SplashActivity2"));
    }


    /**
     * 模拟dynamicConfig为null, 通过 TraceConfig 设置 splashActivities
     */
    @Test
    public void testSetSplashActivitiesWithTraceConfig() {
        final String splashActivities = "com.tencent.matrix.SplashActivity1;com.tencent.matrix.SplashActivity2";
        TraceConfig config = new TraceConfig.Builder().build() ;
        // 设置 splashActivities
        config.splashActivities = splashActivities ;

        Set<String> activities = config.getSplashActivities() ;
        assertNotNull(activities);
        assertEquals(2, activities.size());
        assertTrue( activities.contains("com.tencent.matrix.SplashActivity1"));
        assertTrue( activities.contains("com.tencent.matrix.SplashActivity2"));
    }

    /**
     * 测试只设置了一个SplashActivity的场景
     */
    @Test
    public void testOneSplashActivity() {
        final String splashActivities = "com.tencent.matrix.SplashActivity1";
        TraceConfig config = new TraceConfig.Builder().build() ;
        // 设置 splashActivities
        config.splashActivities = splashActivities ;

        Set<String> activities = config.getSplashActivities() ;
        assertNotNull(activities);
        assertEquals(1, activities.size());
        assertTrue( activities.contains("com.tencent.matrix.SplashActivity1"));
        assertFalse( activities.contains("com.tencent.matrix.SplashActivity2"));
    }

    /**
     * null empty impl IDynamicConfig
     */
    private static class MockDynamicConfig implements IDynamicConfig {

        @Override
        public String get(String key, String defStr) {
            // return null to mock null value to test case
            return null;
//          return defStr;
        }

        @Override
        public int get(String key, int defInt) {
            return 0;
        }

        @Override
        public long get(String key, long defLong) {
            return 0;
        }

        @Override
        public boolean get(String key, boolean defBool) {
            return false;
        }

        @Override
        public float get(String key, float defFloat) {
            return 0;
        }
    } // end of MockDynamicConfig
}
