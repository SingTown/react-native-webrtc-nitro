package com.webrtc

import android.view.Surface
import android.view.SurfaceView
import android.view.SurfaceHolder
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioDeviceCallback
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
    private var audioRouteApplied = false
    private var audioDeviceCallbackRegistered = false
    private var previousAudioMode: Int? = null
    private var previousSpeakerphoneOn: Boolean? = null
    private val audioDeviceCallback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(addedDevices: Array<out AudioDeviceInfo>) {
            view.post { refreshAudioRouteIfNeeded() }
        }

        override fun onAudioDevicesRemoved(removedDevices: Array<out AudioDeviceInfo>) {
            view.post { refreshAudioRouteIfNeeded() }
        }
    }

    companion object {
        private const val SAMPLE_RATE = 48000
        private const val CHANNEL_OUT_CONFIG = AudioFormat.CHANNEL_OUT_STEREO
        private const val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT
        private const val OUTPUT_CHANNEL_COUNT = 2
        private const val BYTES_PER_SAMPLE = 2
        private const val PLAYBACK_BUFFER_DURATION_MS = 80
        private const val PLAYBACK_BUFFER_SIZE =
            SAMPLE_RATE * OUTPUT_CHANNEL_COUNT * BYTES_PER_SAMPLE *
                PLAYBACK_BUFFER_DURATION_MS / 1000

        private var audioTrack = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setSampleRate(SAMPLE_RATE)
                    .setChannelMask(CHANNEL_OUT_CONFIG)
                    .setEncoding(AUDIO_FORMAT)
                    .build()
            )
            .setBufferSizeInBytes(PLAYBACK_BUFFER_SIZE)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
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
            applyAudioRoute()
            this.audioSubscriptionId = subscribeAudio(value, audioTrack)
            this._audioPipeId = value
            audioTrack.play()
        }


    init {
        audioManager.registerAudioDeviceCallback(audioDeviceCallback, null)
        audioDeviceCallbackRegistered = true
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
        if (audioDeviceCallbackRegistered) {
            audioManager.unregisterAudioDeviceCallback(audioDeviceCallback)
            audioDeviceCallbackRegistered = false
        }
        if (this.audioSubscriptionId > 0) {
            this.unsubscribe(this.audioSubscriptionId)
            this.audioSubscriptionId = -1
        }
        audioTrack.pause()
        audioTrack.flush()
        restoreAudioRoute()
    }

    private fun applyAudioRoute() {
        if (audioRouteApplied) {
            return
        }
        previousAudioMode = audioManager.mode
        previousSpeakerphoneOn = audioManager.isSpeakerphoneOn
        audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
        val targetOutput = selectPreferredOutputDevice()
        routeAudioTo(targetOutput)
        audioRouteApplied = true
    }

    private fun restoreAudioRoute() {
        if (!audioRouteApplied) {
            return
        }
        audioTrack.setPreferredDevice(null)
        audioManager.isSpeakerphoneOn = previousSpeakerphoneOn ?: false
        previousSpeakerphoneOn = null
        audioManager.mode = previousAudioMode ?: AudioManager.MODE_NORMAL
        previousAudioMode = null
        audioRouteApplied = false
    }

    private fun refreshAudioRouteIfNeeded() {
        if (!audioRouteApplied || audioSubscriptionId <= 0 || _audioPipeId.isNullOrEmpty()) {
            return
        }
        val targetOutput = selectPreferredOutputDevice()
        routeAudioTo(targetOutput)
    }

    private fun routeAudioTo(targetOutput: AudioDeviceInfo?) {
        audioManager.isSpeakerphoneOn =
            targetOutput?.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
        audioTrack.setPreferredDevice(targetOutput)
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
        return devices.firstOrNull { it.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER }
    }
}
