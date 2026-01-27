package com.webrtc

import android.view.Surface
import android.view.SurfaceView
import android.view.SurfaceHolder
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.facebook.react.uimanager.ThemedReactContext
import com.margelo.nitro.webrtc.HybridWebrtcViewSpec

@Keep
@DoNotStrip
class HybridWebrtcView(val context: ThemedReactContext) : HybridWebrtcViewSpec() {
    // View
    override val view: SurfaceView = SurfaceView(context)
    companion object {
        private var audioTrack = AudioTrack(
            AudioManager.STREAM_MUSIC,
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
            }
            if (value.isNullOrEmpty()) {
                return;
            }
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
}
