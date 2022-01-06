// ISupervisorProxy.aidl
package com.tencent.matrix.lifecycle.supervisor;

// Declare any non-default types here with import statements

import com.tencent.matrix.lifecycle.supervisor.ProcessToken;
import com.tencent.matrix.lifecycle.supervisor.ISubordinateProxy;

interface ISupervisorProxy {
    void registerSubordinate(in ProcessToken[] tokens, in ISubordinateProxy subordinateProxy);

    void onStateChanged(in ProcessToken token);

    void onSceneChanged(in String scene);

    void onProcessKilled(in ProcessToken token);
    void onProcessRescuedFromKill(in ProcessToken token);
    void onProcessKillCanceled(in ProcessToken token);

    String getRecentScene();
}