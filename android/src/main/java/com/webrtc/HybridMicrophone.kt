package com.webrtc

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.webrtc.HybridMicrophoneSpec
import kotlinx.coroutines.*
import com.margelo.nitro.core.Promise
import android.media.audiofx.AcousticEchoCanceler

@Keep
@DoNotStrip
class HybridMicrophone : HybridMicrophoneSpec() {
    private var audioRecord: AudioRecord? = null
    private var recordingJob: Job? = null
    private var pipeId: String = ""
    private val scope = CoroutineScope(Dispatchers.Default + SupervisorJob())
    private var aec: AcousticEchoCanceler? = null

    companion object {
        private const val SAMPLE_RATE = 44100
        private const val CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_MONO
        private const val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT
        private val BUFFER_SIZE = AudioRecord.getMinBufferSize(
            SAMPLE_RATE,
            CHANNEL_CONFIG,
            AUDIO_FORMAT
        )
    }

    external fun publishAudio(pipeId: String, data: ByteArray, size: Int)

    override fun open(pipeId: String): Promise<Unit> {
        this.pipeId = pipeId
        return Promise.async {
            requestPermission(android.Manifest.permission.RECORD_AUDIO)

            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.VOICE_COMMUNICATION,
                SAMPLE_RATE,
                CHANNEL_CONFIG,
                AUDIO_FORMAT,
                BUFFER_SIZE
            )

            val recorder = audioRecord
                ?: throw RuntimeException("AudioRecord is null")

            if (recorder.state != AudioRecord.STATE_INITIALIZED) {
                throw RuntimeException("AudioRecord initialization failed")
            }

            if (AcousticEchoCanceler.isAvailable()) {
                aec = AcousticEchoCanceler.create(recorder.audioSessionId)
                aec?.enabled = true
            }

            recorder.startRecording()

            recordingJob = scope.launch {
                val buffer = ByteArray(BUFFER_SIZE)
                while (isActive) {
                    val readResult = recorder.read(buffer, 0, buffer.size)
                    if (readResult > 0) {
                        publishAudio(pipeId, buffer, readResult)
                    }
                }
            }
        }
    }

    override fun dispose() {
        recordingJob?.cancel()
        recordingJob = null

        audioRecord?.apply {
            stop()
            release()
        }
        audioRecord = null
        scope.cancel()
    }
}
