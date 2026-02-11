package com.webrtc

import android.view.Surface
import android.view.SurfaceView
import android.view.SurfaceHolder
import android.media.AudioFormat
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.media.AudioTrack
import android.content.Context
import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.facebook.react.uimanager.ThemedReactContext
import com.margelo.nitro.webrtc.HybridWebrtcViewSpec

@Keep
@DoNotStrip
class HybridWebrtcView(val context: ThemedReactContext) : HybridWebrtcViewSpec() {
    // View
    override val view: SurfaceView = SurfaceView(context)

    private val audioManager: AudioManager =
        context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private var previousMode: Int? = null
    private var previousSpeakerphoneOn: Boolean? = null
    private var communicationRouteApplied = false

    companion object {
        private var audioTrack = AudioTrack(
            AudioManager.STREAM_VOICE_CALL,
            48000,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_16BIT,
            AudioTrack.getMinBufferSize(
                48000,
                AudioFormat.CHANNEL_OUT_STEREO,
                AudioFormat.ENCODING_PCM_16BIT
            ) * 4,
            AudioTrack.MODE_STREAM
        )
    }

    external fun unsubscribe(subscriptionId: Int)
    external fun subscribeAudio(pipeId: String, track: AudioTrack): Int
    external fun subscribeVideo(pipeId: String, surface: Surface): Int

    private var _audioPipeId: String? = null
    private var _videoPipeId: String? = null
    private var videoSubscriptionId: Int = -1
    private var audioSubscriptionId: Int = -1

    override var audioPipeId: String?
        get() = _audioPipeId
        set(value) {
            if (this.audioSubscriptionId > 0) {
                this.unsubscribe(this.audioSubscriptionId)
                this.audioSubscriptionId = -1
            }
            if (value.isNullOrEmpty()) {
                _audioPipeId = null
                audioTrack.pause()
                audioTrack.flush()
                restoreAudioRoute()
                return;
            }
            applyCommunicationAudioRoute()
            this.audioSubscriptionId = subscribeAudio(value, audioTrack)
            this._audioPipeId = value
            audioTrack.play()
        }


    init {
        view.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                updateVideoPipeId(_videoPipeId, holder.surface)
            }

            override fun surfaceChanged(
                holder: SurfaceHolder,
                format: Int,
                width: Int,
                height: Int
            ) {
                updateVideoPipeId(_videoPipeId, holder.surface)
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                updateVideoPipeId(_videoPipeId, null)
            }
        })
    }

    private fun updateVideoPipeId(newVideoPipeId: String?, surface: Surface?) {
        if (this.videoSubscriptionId > 0) {
            this.unsubscribe(this.videoSubscriptionId)
        }
        if (surface == null) {
            return;
        }
        if (newVideoPipeId.isNullOrEmpty()) {
            return;
        }
        this.videoSubscriptionId = subscribeVideo(newVideoPipeId, surface)
        this._videoPipeId = newVideoPipeId
    }

    override var videoPipeId: String?
        get() = _videoPipeId
        set(value) {
            updateVideoPipeId(value, view.holder.surface)
        }

    override fun dispose() {
        if (this.audioSubscriptionId > 0) {
            this.unsubscribe(this.audioSubscriptionId)
            this.audioSubscriptionId = -1
        }
        audioTrack.pause()
        audioTrack.flush()
        restoreAudioRoute()
    }

    private fun applyCommunicationAudioRoute() {
        if (communicationRouteApplied) {
            return
        }
        previousMode = audioManager.mode
        previousSpeakerphoneOn = audioManager.isSpeakerphoneOn
        audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
        @Suppress("DEPRECATION")
        run {
            audioManager.isSpeakerphoneOn = false
        }
        routeAudioTrackToPreferredOutput()
        communicationRouteApplied = true
    }

    private fun restoreAudioRoute() {
        if (!communicationRouteApplied) {
            return
        }
        audioTrack.setPreferredDevice(null)
        previousSpeakerphoneOn?.let { audioManager.isSpeakerphoneOn = it }
        previousMode?.let { audioManager.mode = it }
        previousSpeakerphoneOn = null
        previousMode = null
        communicationRouteApplied = false
    }

    private fun routeAudioTrackToPreferredOutput() {
        val target = selectPreferredOutputDevice()
        if (target != null) {
            audioTrack.setPreferredDevice(target)
        } else {
            audioTrack.setPreferredDevice(null)
        }
    }

    private fun selectPreferredOutputDevice(): AudioDeviceInfo? {
        val devices = audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
        val preferredTypes = intArrayOf(
            AudioDeviceInfo.TYPE_WIRED_HEADPHONES,
            AudioDeviceInfo.TYPE_WIRED_HEADSET,
            AudioDeviceInfo.TYPE_BLUETOOTH_A2DP,
            AudioDeviceInfo.TYPE_BLE_HEADSET,
            AudioDeviceInfo.TYPE_BLUETOOTH_SCO,
            AudioDeviceInfo.TYPE_USB_HEADSET,
            AudioDeviceInfo.TYPE_HEARING_AID
        )
        for (type in preferredTypes) {
            val match = devices.firstOrNull { it.type == type }
            if (match != null) {
                return match
            }
        }
        return null
    }
}
