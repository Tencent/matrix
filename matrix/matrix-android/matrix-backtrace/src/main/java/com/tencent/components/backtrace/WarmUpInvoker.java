package com.tencent.components.backtrace;

public interface WarmUpInvoker {
    boolean warmUp(String pathOfElf, int offset);
}
