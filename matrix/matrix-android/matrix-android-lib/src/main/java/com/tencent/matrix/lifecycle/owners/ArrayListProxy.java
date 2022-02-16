package com.tencent.matrix.lifecycle.owners;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;

/**
 * Implement in Java for the override limitation of Kotlin
 *
 * Created by Yves on 2021/12/30
 */
class ArrayListProxy<T> extends ArrayList<T> {

    private static final String TAG = "Matrix.ArrayListProxy";

    private final ArrayList<T> mOrigin;

    interface OnDataChangedListener {
        void onAdded(Object o);
        void onRemoved(Object o);
    }

    private final OnDataChangedListener mListener;

    ArrayListProxy(ArrayList<T> origin, OnDataChangedListener listener) {
        mOrigin = origin;
        mListener = listener;
    }

    @Override
    public int size() {
        return mOrigin.size();
    }

    @Override
    public boolean isEmpty() {
        return mOrigin.isEmpty();
    }

    @Override
    public boolean contains(@Nullable Object o) {
        return mOrigin.contains(o);
    }

    @NonNull
    @Override
    public Iterator<T> iterator() {
        return mOrigin.iterator();
    }

    @NonNull
    @Override
    public Object[] toArray() {
        return mOrigin.toArray();
    }

    @NonNull
    @Override
    public <T1> T1[] toArray(@NonNull T1[] a) {
        return mOrigin.toArray(a);
    }

    @Override
    public boolean add(T t) {
        boolean ret = mOrigin.add(t);

        try {
            mListener.onAdded(t);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }

        return ret;
    }

    @Override
    public boolean remove(@Nullable Object o) {
        boolean ret = mOrigin.remove(o);

        try {
            mListener.onRemoved(o);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }

        return ret;
    }

    @Override
    public boolean containsAll(@NonNull Collection<?> c) {
        return mOrigin.containsAll(c);
    }

    @Override
    public boolean addAll(@NonNull Collection<? extends T> c) {
        boolean ret = mOrigin.addAll(c);
        try {
            for (T t : c) {
                mListener.onAdded(t);
            }
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }

        return ret;
    }

    @Override
    public boolean addAll(int index, @NonNull Collection<? extends T> c) {
        boolean ret =  mOrigin.addAll(index, c);
        try {
            for (T t : c) {
                mListener.onAdded(t);
            }
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
        return ret;
    }

    @Override
    public boolean removeAll(@NonNull Collection<?> c) {
        boolean ret = mOrigin.removeAll(c);

        try {
            for (Object o : c) {
                mListener.onRemoved(o);
            }
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }

        return ret;
    }

    @Override
    public boolean retainAll(@NonNull Collection<?> c) {
        return mOrigin.retainAll(c);
    }

    @Override
    public void clear() {
        mOrigin.clear();
    }

    @Override
    public T get(int index) {
        return mOrigin.get(index);
    }

    @Override
    public T set(int index, T element) {
        return mOrigin.set(index, element);
    }

    @Override
    public void add(int index, T element) {
        mOrigin.add(index, element);
        try {
            mListener.onAdded(element);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
    }

    @Override
    public T remove(int index) {
        T ret = mOrigin.remove(index);
        try {
            mListener.onRemoved(ret);
        } catch (Throwable e) {
            MatrixLog.printErrStackTrace(TAG, e, "");
        }
        return ret;
    }

    @Override
    public int indexOf(@Nullable Object o) {
        return mOrigin.indexOf(o);
    }

    @Override
    public int lastIndexOf(@Nullable Object o) {
        return mOrigin.lastIndexOf(o);
    }

    @NonNull
    @Override
    public ListIterator<T> listIterator() {
        return mOrigin.listIterator();
    }

    @NonNull
    @Override
    public ListIterator<T> listIterator(int index) {
        return mOrigin.listIterator(index);
    }

    @NonNull
    @Override
    public List<T> subList(int fromIndex, int toIndex) {
        return mOrigin.subList(fromIndex, toIndex);
    }

}
