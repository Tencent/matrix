// ISubordinateProxy.aidl
package com.tencent.matrix.lifecycle.supervisor;

// Declare any non-default types here with import statements
import com.tencent.matrix.util.MemInfo;

interface ISubordinateProxy {
    void dispatchState(in String scene, in String stateName, in boolean state);
    void dispatchKill(in String scene, in String targetProcess, in int targetPid);
    void dispatchDeath(in String scene, in String targetProcess, in int targetPid, in boolean isLruKill);

    MemInfo getMemInfo();
}