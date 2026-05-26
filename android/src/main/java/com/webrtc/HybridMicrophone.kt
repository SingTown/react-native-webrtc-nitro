package com.webrtc

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.audiofx.AcousticEchoCanceler
import android.media.audiofx.NoiseSuppressor
import android.util.Log
import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.core.Promise
import com.margelo.nitro.webrtc.HybridMicrophoneSpec
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking

@Keep
@DoNotStrip
class HybridMicrophone : HybridMicrophoneSpec() {
    private var audioRecord: AudioRecord? = null
    private var recordingJob: Job? = null
    private var pipeId: String = ""
    private val scope = CoroutineScope(Dispatchers.Default + SupervisorJob())
    private var acousticEchoCanceler: AcousticEchoCanceler? = null
    private var noiseSuppressor: NoiseSuppressor? = null

    companion object {
        private const val TAG = "HybridMicrophone"
        private const val SAMPLE_RATE = 48000
        private const val CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_MONO
        private const val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT
        private const val READ_BUFFER_DURATION_MS = 20
        private const val BYTES_PER_SAMPLE = 2
        private const val RECORD_BUFFER_MULTIPLIER = 4
        private const val READ_BUFFER_SIZE =
            SAMPLE_RATE * BYTES_PER_SAMPLE * READ_BUFFER_DURATION_MS / 1000
        private const val RECORD_BUFFER_SIZE = READ_BUFFER_SIZE * RECORD_BUFFER_MULTIPLIER
    }

    external fun publishAudio(pipeId: String, data: ByteArray, size: Int)
    external fun resetAudioFilter(pipeId: String)

    override fun open(pipeId: String): Promise<Unit> {
        return Promise.async {
            startRecording(pipeId)
        }
    }

    override fun dispose() {
        stopRecording()
    }

    @Synchronized
    private fun startRecording(nextPipeId: String) {
        if (nextPipeId.isEmpty()) {
            throw RuntimeException("Microphone pipe id is empty")
        }

        stopRecording()
        pipeId = nextPipeId

        try {
            val minBufferSize =
                AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)
            when (minBufferSize) {
                AudioRecord.ERROR, AudioRecord.ERROR_BAD_VALUE -> {
                    throw RuntimeException("AudioRecord.getMinBufferSize failed: $minBufferSize")
                }
            }
            val recordBufferSize = maxOf(RECORD_BUFFER_SIZE, minBufferSize)

            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.VOICE_COMMUNICATION,
                SAMPLE_RATE,
                CHANNEL_CONFIG,
                AUDIO_FORMAT,
                recordBufferSize
            )

            val recorder = audioRecord
                ?: throw RuntimeException("AudioRecord is null")

            if (recorder.state != AudioRecord.STATE_INITIALIZED) {
                throw RuntimeException("AudioRecord initialization failed")
            }

            val isAcousticEchoCancelerAvailable = AcousticEchoCanceler.isAvailable()
            Log.i(TAG, "AcousticEchoCanceler.isAvailable=$isAcousticEchoCancelerAvailable")
            if (isAcousticEchoCancelerAvailable) {
                acousticEchoCanceler = AcousticEchoCanceler.create(recorder.audioSessionId)?.apply {
                    enabled = true
                }
            }

            val isNoiseSuppressorAvailable = NoiseSuppressor.isAvailable()
            Log.i(TAG, "NoiseSuppressor.isAvailable=$isNoiseSuppressorAvailable")
            if (isNoiseSuppressorAvailable) {
                noiseSuppressor = NoiseSuppressor.create(recorder.audioSessionId)?.apply {
                    enabled = true
                }
            }

            recorder.startRecording()
            recordingJob = scope.launch {
                val buffer = ByteArray(READ_BUFFER_SIZE)
                while (isActive) {
                    val readResult = recorder.read(buffer, 0, buffer.size)
                    if (!isActive) {
                        break
                    }
                    if (readResult > 0) {
                        publishAudio(nextPipeId, buffer, readResult)
                    } else if (readResult < 0) {
                        break
                    }
                }
            }
        } catch (error: Throwable) {
            releaseRecorder()
            resetAudioFilter(nextPipeId)
            if (pipeId == nextPipeId) {
                pipeId = ""
            }
            throw error
        }
    }

    @Synchronized
    private fun stopRecording() {
        val stoppedPipeId = pipeId
        val job = recordingJob
        recordingJob = null
        job?.cancel()

        releaseRecorder()

        if (job != null) {
            runBlocking {
                job.join()
            }
        }

        if (stoppedPipeId.isNotEmpty()) {
            resetAudioFilter(stoppedPipeId)
        }
        pipeId = ""
    }

    private fun releaseRecorder() {
        acousticEchoCanceler?.release()
        acousticEchoCanceler = null

        noiseSuppressor?.release()
        noiseSuppressor = null

        audioRecord?.apply {
            try {
                if (recordingState == AudioRecord.RECORDSTATE_RECORDING) {
                    stop()
                }
            } catch (_: IllegalStateException) {
            }
            release()
        }
        audioRecord = null
    }
}
