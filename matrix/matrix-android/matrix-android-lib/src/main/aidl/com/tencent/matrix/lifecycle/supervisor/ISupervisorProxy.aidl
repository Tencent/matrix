// ISupervisorProxy.aidl
package com.tencent.matrix.lifecycle.supervisor;

// Declare any non-default types here with import statements

import com.tencent.matrix.lifecycle.supervisor.ProcessToken;

interface ISupervisorProxy {
    void stateRegister(in ProcessToken token);
    void stateTurnOn(in ProcessToken token);
    void stateTurnOff(in ProcessToken token);

    void onProcessKilled(in ProcessToken token);
    void onProcessRescuedFromKill(in ProcessToken token);
    void onProcessKillCanceled(in ProcessToken token);
}