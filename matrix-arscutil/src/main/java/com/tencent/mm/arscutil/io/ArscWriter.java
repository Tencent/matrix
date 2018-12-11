package com.tencent.mm.arscutil.io;

import com.tencent.matrix.javalib.util.Log;
import com.tencent.mm.arscutil.data.ResTable;

import java.io.File;
import java.io.IOException;

public class ArscWriter {

    private static final String TAG = "ArscUtil.ArscWriter";

    LittleEndianOutputStream dataOutput;

    public ArscWriter(String arscFile) throws IOException {
        File file = new File(arscFile);
        Log.i(TAG, "write to %s", arscFile);
        if (file.exists()) {
            file.delete();
        }
        file.getParentFile().mkdirs();
        file.createNewFile();
        dataOutput = new LittleEndianOutputStream(arscFile);
    }

    public void writeResTable(ResTable resTable) throws Exception {
        dataOutput.write(resTable.toBytes());
        dataOutput.close();
    }
}
