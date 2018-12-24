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

package sample.tencent.matrix.io;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.widget.Toast;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.iocanary.IOCanaryPlugin;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.util.MatrixLog;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

import sample.tencent.matrix.R;
import sample.tencent.matrix.issue.IssueFilter;


/**
 * @author liyongjie
 * Created by liyongjie on 2017/6/9.
 */

public class TestIOActivity extends Activity {
    private static final String TAG = "Matrix.TestIoActivity";
    private static final int EXTERNAL_STORAGE_REQ_CODE = 0x1;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.test_io);
        IssueFilter.setCurrentFilter(IssueFilter.ISSUE_IO);
        requestPer();

        Plugin plugin = Matrix.with().getPluginByClass(IOCanaryPlugin.class);
        if (!plugin.isPluginStarted()) {
            MatrixLog.i(TAG, "plugin-io start");
            plugin.start();
        }

    }

    private void requestPer() {
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {

            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                Toast.makeText(this, "please give me the permission", Toast.LENGTH_SHORT).show();
            } else {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        EXTERNAL_STORAGE_REQ_CODE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case EXTERNAL_STORAGE_REQ_CODE:
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                }
                break;
        }
    }

    public void onClick(View v) {
        if (v.getId() == R.id.file_io) {
            toastStartTest("file_io");
            writeLongSth();
        } else if (v.getId() == R.id.close_leak) {

            toastStartTest("close_leak");
            leakSth();
        } else if (v.getId() == R.id.small_buffer) {

            toastStartTest("small_buffer");
            smallBuffer();
        }
    }

    public void toastStartTest(String val) {
        Toast.makeText(this, "starting io -> " + val + " test", Toast.LENGTH_SHORT).show();
    }

    private void smallBuffer() {
        readSth();
    }

    private void writeLongSth() {
        try {
            File f = new File("/sdcard/a_long.txt");
            if (f.exists()) {
                f.delete();
            }
            byte[] data = new byte[512];
            for (int i = 0; i < data.length; i++) {
                data[i] = 'a';
            }
            FileOutputStream fos = new FileOutputStream(f);
            for (int i = 0; i < 10000 * 8; i++) {
                fos.write(data);
            }

            fos.flush();
            fos.close();
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void writeSth() {
        try {
            File f = new File("/sdcard/a.txt");
            if (f.exists()) {
                f.delete();
            }
            byte[] data = new byte[4096];
            for (int i = 0; i < data.length; i++) {
                data[i] = 'a';
            }
            FileOutputStream fos = new FileOutputStream(f);
            for (int i = 0; i < 10; i++) {
                fos.write(data);
            }

            fos.flush();
            fos.close();
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void readSth() {
        try {
            File f = new File("/sdcard/a_long.txt");
            byte[] buf = new byte[400];
            FileInputStream fis = new FileInputStream(f);
            int count = 0;
            while (fis.read(buf) != -1) {
//                MatrixLog.i(TAG, "read %d", ++count);
            }
            fis.close();
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }


    private void leakSth() {
        writeSth();
        try {
            File f = new File("/sdcard/a.txt");
            byte[] buf = new byte[400];
            FileInputStream fis = new FileInputStream(f);
            int count = 0;
            while (fis.read(buf) != -1) {
//                MatrixLog.i(TAG, "read %d", ++count);
            }
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }

        //need to trigger gc to detect leak
        new Thread(new Runnable() {
            @Override
            public void run() {
                Runtime.getRuntime().gc();
                Runtime.getRuntime().runFinalization();
                Runtime.getRuntime().gc();
            }
        }).start();

    }

}
