package com.webrtc

import android.content.Context
import androidx.annotation.Keep
import android.Manifest
import android.content.pm.PackageManager
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CaptureRequest
import android.media.Image
import android.media.ImageReader
import android.graphics.ImageFormat
import android.os.Handler
import android.os.HandlerThread
import android.view.Surface
import android.view.WindowManager
import android.util.Log
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.webrtc.FacingMode
import com.margelo.nitro.webrtc.HybridCameraSpec
import com.margelo.nitro.core.Promise
import com.margelo.nitro.NitroModules


object Camera {
    private const val DEFAULT_WIDTH = 1280
    private const val DEFAULT_HEIGHT = 720

    private var cameraDevice: CameraDevice? = null
    private var captureSession: CameraCaptureSession? = null
    private var backgroundThread = HandlerThread("CameraBackground")
    private var imageReader =
        ImageReader.newInstance(DEFAULT_WIDTH, DEFAULT_HEIGHT, ImageFormat.YUV_420_888, 2)
    private val pipeIds = mutableSetOf<String>()
    private var facingMode: FacingMode = FacingMode.USER
    private var sensorOrientation: Int = 0
    private var isFrontFacing: Boolean = true

    external fun publishVideo(pipeIds: Array<String>, image: Image, rotation: Int, mirror: Boolean)

    private val cameraStateCallback = object : CameraDevice.StateCallback() {
        override fun onOpened(camera: CameraDevice) {
            cameraDevice = camera
            val captureRequestBuilder = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
            captureRequestBuilder.addTarget(imageReader.surface)

            captureRequestBuilder.set(
                CaptureRequest.CONTROL_AF_MODE,
                CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE
            )

            captureRequestBuilder.set(
                CaptureRequest.CONTROL_AE_MODE,
                CaptureRequest.CONTROL_AE_MODE_ON
            )

            captureRequestBuilder.set(
                CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE,
                android.util.Range(30, 30)
            )
            val captureRequest = captureRequestBuilder.build()

            val sessionStateCallback = object : CameraCaptureSession.StateCallback() {
                override fun onConfigured(session: CameraCaptureSession) {
                    captureSession = session
                    session.setRepeatingRequest(
                        captureRequest,
                        null,
                        Handler(backgroundThread.looper)
                    )
                }

                override fun onConfigureFailed(session: CameraCaptureSession) {
                    throw RuntimeException("Camera configuration failed")
                }
            }
            camera.createCaptureSession(
                listOf(imageReader.surface),
                sessionStateCallback,
                Handler(backgroundThread.looper)
            )
        }

        override fun onDisconnected(camera: CameraDevice) {
            camera.close()
        }

        override fun onError(camera: CameraDevice, error: Int) {
            camera.close()
            throw RuntimeException("Camera run Error!")
        }
    }

    @Synchronized
    fun addPipeId(pipeId: String) {
        if (pipeIds.contains(pipeId)) {
            return
        }
        pipeIds.add(pipeId)
        if (pipeIds.size == 1) {
            startCamera()
        }
    }

    @Synchronized
    fun removePipeId(pipeId: String) {
        if (!pipeIds.contains(pipeId)) {
            return
        }
        pipeIds.remove(pipeId)
        if (pipeIds.isEmpty()) {
            stopCamera()
        }
    }

    @Synchronized
    fun switchCamera(facingMode: FacingMode) {
        if (facingMode == this.facingMode) return
        this.facingMode = facingMode
        if (pipeIds.isNotEmpty()) {
            stopCamera()
            startCamera()
        }
    }

    private fun startCamera() {
        val context = NitroModules.applicationContext
            ?: throw RuntimeException("ReactApplicationContext is not available")

        val cameraManager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
            ?: throw RuntimeException("CameraManager is not available")

        backgroundThread = HandlerThread("CameraBackground")
        backgroundThread.start()

        val cameraId = cameraManager.cameraIdList.firstOrNull { id ->
            try {
                val characteristics = cameraManager.getCameraCharacteristics(id)
                val facing = characteristics.get(CameraCharacteristics.LENS_FACING)
                if (facingMode == FacingMode.USER) {
                    facing == CameraCharacteristics.LENS_FACING_FRONT
                } else {
                    facing == CameraCharacteristics.LENS_FACING_BACK
                }
            } catch (e: Exception) {
                false
            }
        } ?: cameraManager.cameraIdList.firstOrNull()
            ?: throw RuntimeException("Camera not exist!")

        try {
            val characteristics = cameraManager.getCameraCharacteristics(cameraId)
            sensorOrientation =
                characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION) ?: 0
            isFrontFacing =
                characteristics.get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_FRONT
        } catch (e: Exception) {
            sensorOrientation = 0
            isFrontFacing = facingMode == FacingMode.USER
        }

        val onImageAvailableListener = ImageReader.OnImageAvailableListener { reader ->
            val image = reader.acquireNextImage()
            image.use {
                val rotation = getFrameRotationDegrees(context)
                val mirror = isFrontFacing
                publishVideo(pipeIds.toTypedArray(), it, rotation, mirror)
            }
        }
        imageReader.setOnImageAvailableListener(
            onImageAvailableListener,
            Handler(backgroundThread.looper)
        )

        cameraManager.openCamera(cameraId, cameraStateCallback, Handler(backgroundThread.looper))

    }

    private fun stopCamera() {
        captureSession?.stopRepeating()
        captureSession?.close()
        captureSession = null
        cameraDevice?.close()
        cameraDevice = null
        backgroundThread.quitSafely()
        backgroundThread.join()
    }

    private fun getFrameRotationDegrees(context: Context): Int {
        val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as? WindowManager
        val rotation = windowManager?.defaultDisplay?.rotation ?: Surface.ROTATION_0
        val displayDegrees = when (rotation) {
            Surface.ROTATION_90 -> 90
            Surface.ROTATION_180 -> 180
            Surface.ROTATION_270 -> 270
            else -> 0
        }
        return if (isFrontFacing) {
            (sensorOrientation + displayDegrees) % 360
        } else {
            (sensorOrientation - displayDegrees + 360) % 360
        }
    }

}


@Keep
@DoNotStrip
class HybridCamera : HybridCameraSpec() {
    private var pipeId: String = ""

    override fun switchCamera(facingMode: FacingMode): Promise<Unit> {
        return Promise.async {
            Camera.switchCamera(facingMode)
        }
    }

    override fun open(pipeId: String): Promise<Unit> {
        this.pipeId = pipeId
        return Promise.async {
            Camera.addPipeId(pipeId)
        }
    }

    override fun dispose() {
        Camera.removePipeId(pipeId)
        pipeId = ""
    }
}
