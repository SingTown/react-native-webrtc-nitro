package com.webrtc

import android.graphics.Matrix
import android.graphics.SurfaceTexture
import android.view.Surface
import android.view.TextureView
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.view.ViewOutlineProvider
import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.facebook.react.uimanager.ThemedReactContext
import com.margelo.nitro.webrtc.HybridWebrtcViewSpec
import com.margelo.nitro.webrtc.ResizeMode

@Keep
@DoNotStrip
class HybridWebrtcView(val context: ThemedReactContext) : HybridWebrtcViewSpec() {
    // View
    override val view: TextureView = TextureView(context)
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
    private var _resizeMode: ResizeMode = ResizeMode.CONTAIN
    private var videoSubscriptionId: Int = -1
    private var audioSubscriptionId: Int = -1
    private var videoWidth: Int = 0
    private var videoHeight: Int = 0
    private var videoSurface: Surface? = null

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
        view.outlineProvider = ViewOutlineProvider.BOUNDS
        view.clipToOutline = true
        view.surfaceTextureListener = object : TextureView.SurfaceTextureListener {
            override fun onSurfaceTextureAvailable(
                surfaceTexture: SurfaceTexture,
                width: Int,
                height: Int
            ) {
                videoSurface = Surface(surfaceTexture)
                updateVideoPipeId(_videoPipeId, videoSurface)
                applyTransform()
            }

            override fun onSurfaceTextureSizeChanged(
                surfaceTexture: SurfaceTexture,
                width: Int,
                height: Int
            ) {
                applyTransform()
            }

            override fun onSurfaceTextureDestroyed(surfaceTexture: SurfaceTexture): Boolean {
                updateVideoPipeId(_videoPipeId, null)
                videoSurface?.release()
                videoSurface = null
                return true
            }

            override fun onSurfaceTextureUpdated(surfaceTexture: SurfaceTexture) {
            }
        }

        view.addOnLayoutChangeListener { _, _, _, _, _, _, _, _, _ ->
            applyTransform()
        }
    }

    private fun updateVideoPipeId(newVideoPipeId: String?, surface: Surface?) {
        if (this.videoSubscriptionId > 0) {
            this.unsubscribe(this.videoSubscriptionId)
        }
        this._videoPipeId = newVideoPipeId
        if (surface == null) {
            return;
        }
        if (newVideoPipeId.isNullOrEmpty()) {
            return;
        }
        this.videoSubscriptionId = subscribeVideo(newVideoPipeId, surface)
    }

    override var videoPipeId: String?
        get() = _videoPipeId
        set(value) {
            updateVideoPipeId(value, videoSurface)
        }

    override var resizeMode: ResizeMode?
        get() = _resizeMode
        set(value) {
            _resizeMode = value ?: ResizeMode.CONTAIN
            applyTransform()
        }

    @Keep
    @DoNotStrip
    fun onFrameSizeChanged(width: Int, height: Int) {
        if (width <= 0 || height <= 0) {
            return
        }
        view.post {
            if (videoWidth == width && videoHeight == height) {
                return@post
            }
            videoWidth = width
            videoHeight = height
            if (videoSurface != null) {
                updateVideoPipeId(_videoPipeId, videoSurface)
            }
            applyTransform()
        }
    }

    private fun applyTransform() {
        val w = view.width
        val h = view.height
        if (w <= 0 || h <= 0 || videoWidth <= 0 || videoHeight <= 0) {
            return
        }

        val sx = w.toFloat() / videoWidth.toFloat()
        val sy = h.toFloat() / videoHeight.toFloat()

        val scaleX: Float
        val scaleY: Float
        when (_resizeMode) {
            ResizeMode.FILL -> {
                scaleX = 1f
                scaleY = 1f
            }
            ResizeMode.CONTAIN -> {
                if (sx < sy) {
                    scaleX = 1f
                    scaleY = sx / sy
                } else {
                    scaleX = sy / sx
                    scaleY = 1f
                }
            }
            ResizeMode.COVER -> {
                if (sx < sy) {
                    scaleX = sy / sx
                    scaleY = 1f
                } else {
                    scaleX = 1f
                    scaleY = sx / sy
                }
            }
        }

        val matrix = Matrix()
        matrix.setScale(scaleX, scaleY, w / 2f, h / 2f)
        view.setTransform(matrix)
        view.invalidate()
    }
}
