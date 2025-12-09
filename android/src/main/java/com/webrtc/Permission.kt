package com.webrtc

import android.content.pm.PackageManager
import androidx.core.content.ContextCompat
import com.facebook.react.modules.core.PermissionAwareActivity
import com.facebook.react.modules.core.PermissionListener
import kotlin.random.Random
import kotlinx.coroutines.CompletableDeferred
import com.margelo.nitro.NitroModules

suspend fun requestPermission(permission: String) {
    val context = NitroModules.applicationContext
        ?: throw RuntimeException("ReactApplicationContext is not available")

    val currentActivity = context.currentActivity
        ?: throw RuntimeException("No current Activity")

    if (ContextCompat.checkSelfPermission(context, permission)
        == PackageManager.PERMISSION_GRANTED
    ) {
        return
    }

    if (currentActivity !is PermissionAwareActivity) {
        throw RuntimeException("Current activity doesn't support permissions")
    }

    val code = Random.nextInt(0, 65535)
    val deferred = CompletableDeferred<Unit>()

    val listener = object : PermissionListener {
        override fun onRequestPermissionsResult(
            requestCode: Int,
            permissions: Array<String>,
            grantResults: IntArray
        ): Boolean {
            if (requestCode != code) return false

            if (permissions.isEmpty() || grantResults.isEmpty() ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED
            ) {
                deferred.completeExceptionally(
                    RuntimeException("Permission denied by user")
                )
            } else {
                deferred.complete(Unit)
            }
            return true
        }
    }

    currentActivity.requestPermissions(
        arrayOf(permission),
        code,
        listener
    )
    deferred.await()
}
