package com.webrtc

import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.core.Promise
import com.margelo.nitro.webrtc.HybridMicrophoneSpec
import com.margelo.nitro.webrtc.MicrophoneAndroidTuning

@Keep
@DoNotStrip
class HybridMicrophone : HybridMicrophoneSpec() {
    private var pipeId: String = ""

    external fun startNativeMic(
        pipeId: String,
        agcTargetRms: Float,
        nearMaxGain: Float,
        farMaxGain: Float,
        noiseGateOpenRatio: Float,
        farThresholdRatio: Float
    ): Boolean
    external fun stopNativeMic()

    override fun open(pipeId: String, tuning: MicrophoneAndroidTuning?): Promise<Unit> {
        this.pipeId = pipeId
        val agcTargetRms = tuning?.agcTargetRms?.toFloat() ?: 6000.0f
        val nearMaxGain = tuning?.nearMaxGain?.toFloat() ?: 6.0f
        val farMaxGain = tuning?.farMaxGain?.toFloat() ?: 16.0f
        val noiseGateOpenRatio = tuning?.noiseGateOpenRatio?.toFloat() ?: 2.2f
        val farThresholdRatio = tuning?.farThresholdRatio?.toFloat() ?: 3.5f
        return Promise.async {
            val started = startNativeMic(
                pipeId,
                agcTargetRms,
                nearMaxGain,
                farMaxGain,
                noiseGateOpenRatio,
                farThresholdRatio
            )
            if (!started) {
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
