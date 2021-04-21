package com.tencent.matrix.batterycanary.utils;

/**
 * Compat version of {@link java.util.function.Consumer}
 * @param <T> the type of the input to the operation
 */
public interface Consumer<T> {

    /**
     * Performs this operation on the given argument.
     *
     * @param t the input argument
     */
    void accept(T t);
}
