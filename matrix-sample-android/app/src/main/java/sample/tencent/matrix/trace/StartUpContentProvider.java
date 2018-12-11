package sample.tencent.matrix.trace;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.tencent.matrix.util.MatrixLog;

public class StartUpContentProvider extends ContentProvider {
    private static final String TAG = "Matrix.StartUpContentProvider";
    public static final String AUTHORITIES = "sample.tencent.matrix.trace.StartUpContentProvider";
    public static final UriMatcher uriMatcher;
    public static final int INCOMING_USER_COLLECTION = 1;

    static {
        uriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        uriMatcher.addURI(StartUpContentProvider.AUTHORITIES, "/user", INCOMING_USER_COLLECTION);
    }

    @Override
    public boolean onCreate() {

        return false;
    }

    @Nullable
    @Override
    public Cursor query(@NonNull Uri uri, @Nullable String[] projection, @Nullable String selection, @Nullable String[] selectionArgs, @Nullable String sortOrder) {
        return null;
    }

    @Nullable
    @Override
    public String getType(@NonNull Uri uri) {
        return null;
    }

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
        return null;
    }

    @Override
    public int delete(@NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(@NonNull Uri uri, @Nullable ContentValues values, @Nullable String selection, @Nullable String[] selectionArgs) {
        return 0;
    }

    @Nullable
    @Override
    public Bundle call(@NonNull String method, @Nullable String arg, @Nullable Bundle extras) {
        MatrixLog.d(TAG, "CALL");
        Bundle bundle = new Bundle();
        bundle.putString("returnCall", "successfully!");
        return bundle;
    }
}
