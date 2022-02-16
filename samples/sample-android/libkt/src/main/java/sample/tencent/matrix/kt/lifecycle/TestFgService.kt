package sample.tencent.matrix.kt.lifecycle

import android.app.*
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Build.VERSION
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Process
import com.tencent.matrix.util.MatrixLog
import sample.tencent.matrix.kt.R
import java.lang.reflect.InvocationHandler
import java.lang.reflect.Method

/**
 * Created by Yves on 2021/11/22
 */
class TestFgService : Service() {

    companion object {
        private const val TAG = "Matrix.sample.TestFgService"

        @JvmStatic
        fun test(context: Context) {
            val intent = Intent(context, TestFgService::class.java)
            context.startService(intent)
        }

        fun getNotificationChannelIdCompat(context: Context): String? {
            if (VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val channelName = "com.tencent.matrix.resource.processor.ManualDumpProcessor"
                val notificationManager =
                    context.getSystemService(NOTIFICATION_SERVICE) as NotificationManager
                var notificationChannel = notificationManager.getNotificationChannel(channelName)
                if (notificationChannel == null) {
                    MatrixLog.d(TAG, "create channel")
                    notificationChannel = NotificationChannel(
                        channelName,
                        channelName,
                        NotificationManager.IMPORTANCE_HIGH
                    )
                    notificationManager.createNotificationChannel(notificationChannel)
                }
                return channelName
            }
            return null
        }
    }

    private class FgServiceHandler(val origin: Any?) : InvocationHandler {

        fun onStartForeground() {
            MatrixLog.d(TAG, "hack onStartForeground: ")
        }

        fun onStopForeground() {
            MatrixLog.d(TAG, "hack onStopForeground: ")
        }

        override fun invoke(proxy: Any?, method: Method?, vararg args: Any): Any? {
            MatrixLog.d(TAG, "hack invoked : ${method?.name}, ${args.contentToString()}")
            return try {
                val ret = method?.invoke(
                    origin,
                    *args
                )

                if (method?.name == "setServiceForeground") {
                    MatrixLog.d(TAG, "real invoked setServiceForeground")
                    if (args.size == 6 && args[5] == 0) {
                        onStopForeground()
                    } else {
                        onStartForeground()
                    }
                }

                ret
            } catch (e: Throwable) {
                MatrixLog.printErrStackTrace(TAG, e, "invoke err")
                null
            }
        }
    }

    override fun onCreate() {

        super.onCreate()

        var builder: Notification.Builder? = null
        builder = if (VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, getNotificationChannelIdCompat(this))
        } else {
            Notification.Builder(this)
        }

        builder.setContentTitle(TAG)
            .setPriority(Notification.PRIORITY_DEFAULT)
            .setStyle(Notification.BigTextStyle().bigText(TAG))
            .setAutoCancel(true)
            .setSmallIcon(R.drawable.ic_launcher)
            .setWhen(System.currentTimeMillis())

        val notification = builder.build()
        startForeground(0x233, notification)
        MatrixLog.d(TAG, "onStartCommand: ")
        MatrixLog.d(TAG, "onStartCommand: start foreground")
        startForeground(0x233, notification)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {

        Handler(Looper.getMainLooper()).postDelayed({
            MatrixLog.d(TAG, "onStartCommand: stop foreground")
            stopForeground(true)
        }, 10000)

        return super.onStartCommand(intent, flags, startId)
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }
}