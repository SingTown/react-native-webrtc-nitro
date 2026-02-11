package com.webrtc

import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.core.Promise
import com.margelo.nitro.webrtc.HybridMicrophoneSpec

@Keep
@DoNotStrip
class HybridMicrophone : HybridMicrophoneSpec() {
    private var pipeId: String = ""

    external fun startNativeMic(pipeId: String): Boolean
    external fun stopNativeMic()

    override fun open(pipeId: String): Promise<Unit> {
        this.pipeId = pipeId
        return Promise.async {
            if (!startNativeMic(pipeId)) {
                throw RuntimeException("Failed to start native microphone")
            }
        }
    }

    override fun dispose() {
        if (pipeId.isNotEmpty()) {
            stopNativeMic()
            pipeId = ""
        }
    }
}
