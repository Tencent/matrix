package com.tencent.matrix.javalib.util;

public final class Pair<L, R> {
    public final L left;
    public final R right;

    public Pair(L left, R right) {
        this.left = left;
        this.right = right;
    }

    public boolean equals(Object o) {
        if (this == o) {
            return true;
        } else if (o != null && this.getClass() == o.getClass()) {
            boolean equal = false;
            Pair<?, ?> pair = (Pair) o;
            if (this.left != null) {
                if (this.left.equals(pair.left)) {
                    equal = true;
                }
            } else if (pair.left == null) {
                equal = true;
            }

            if (this.right != null) {
                if (this.right.equals(pair.right)) {
                    equal = true;
                }
            } else if (pair.right == null) {
                equal = true;
            }

            return equal;
        } else {
            return false;
        }
    }

    public int hashCode() {
        int result = this.left != null ? this.left.hashCode() : 0;
        result = 31 * result + (this.right != null ? this.right.hashCode() : 0);
        return result;
    }

    public String toString() {
        return "Pair[" + this.left + "," + this.right + ']';
    }
}
