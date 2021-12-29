// ISupervisorProxy.aidl
package com.tencent.matrix.lifecycle.supervisor;

// Declare any non-default types here with import statements

import com.tencent.matrix.lifecycle.supervisor.ProcessToken;

interface ISupervisorProxy {
    void stateRegister(in ProcessToken[] tokens);

    void onStateChanged(in ProcessToken token);

    void onSceneChanged(in String scene);

    void onProcessBackground(in ProcessToken token);
    void onProcessForeground(in ProcessToken token);

    void onProcessKilled(in ProcessToken token);
    void onProcessRescuedFromKill(in ProcessToken token);
    void onProcessKillCanceled(in ProcessToken token);
}