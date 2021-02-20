package com.tencent.matrix.batterycanary.asm

import android.app.PendingIntent
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.location.Criteria
import android.location.LocationListener
import android.location.LocationManager
import android.net.wifi.WifiManager
import android.os.Looper
import java.util.*

/**
 * Snippet for ASM configs
 *
 * @author Kaede
 * @since  2021/2/7
 */

class KotlinTesting {
    fun testBlueTooth(context: Context) {
        val scanFilters = Arrays.asList(ScanFilter.Builder().build())
        val scanSettings: ScanSettings? = null
        val scanCallback: ScanCallback? = null
        val pendingIntent: PendingIntent? = null
        val leScanCallback: BluetoothAdapter.LeScanCallback? = null

        val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter = manager.adapter
        adapter.startDiscovery()
        adapter.startLeScan(leScanCallback)
        adapter.startLeScan(arrayOf(UUID(0L, 0L)), leScanCallback)

        val scanner = adapter.bluetoothLeScanner
        scanner.startScan(scanCallback)
        scanner.startScan(scanFilters, scanSettings, scanCallback)
        scanner.startScan(scanFilters, scanSettings, pendingIntent)
    }

    fun testWifi(context: Context) {
        val wifiManager = context.getSystemService(Context.WIFI_SERVICE) as WifiManager
        wifiManager.startScan()
        wifiManager.scanResults
    }

    fun testGps(context: Context) {
        var listener : LocationListener? = null
        var criteria : Criteria? = null
        var pendingIntent: PendingIntent? = null
        val locationManager = context.getSystemService(Context.LOCATION_SERVICE) as LocationManager
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0L, 0F, listener)
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0L, 0F, listener, Looper.getMainLooper())
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0L, 0F, pendingIntent)
        locationManager.requestLocationUpdates(0L, 0F, criteria, pendingIntent)
        locationManager.requestLocationUpdates(0L, 0F, criteria, listener, Looper.getMainLooper())
    }
}