//
//  HybridCamera.swift
//  Pods
//
//  Created by kaizhi-singtown on 2025/11/27.
//

import AVFoundation
import Foundation
import NitroModules

private class CameraManager: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    static let shared = CameraManager()
    
    let captureSession = AVCaptureSession()
    let videoOutput = AVCaptureVideoDataOutput()
    let videoQueue = DispatchQueue(label: "com.camera.shared.video.queue")
    
    private var activePipeIds: Set<String> = []
    private let pipeIdsLock = NSLock()
    
    var facingMode: FacingMode = .user
    private let cameraLock = NSLock()
    
    private override init() {
        super.init()
        captureSession.sessionPreset = .high
        
        videoOutput.videoSettings = [
            kCVPixelBufferPixelFormatTypeKey as String:
                kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
        ]
        videoOutput.setSampleBufferDelegate(self, queue: videoQueue)
        if captureSession.canAddOutput(videoOutput) {
            captureSession.addOutput(videoOutput)
        }
        
    }
    
    func prepare() throws {
        let position: AVCaptureDevice.Position = facingMode == .user ? .front : .back
        guard let videoDevice = AVCaptureDevice.default(
            .builtInWideAngleCamera,
            for: .video,
            position: position) else {
            throw RuntimeError.error(withMessage: "Unable to access camera")
        }
        let deviceInput = try AVCaptureDeviceInput(device: videoDevice)
        
        let videoDeviceInput = deviceInput
        if captureSession.canAddInput(videoDeviceInput) {
            captureSession.addInput(videoDeviceInput)
        }
    }
    
    private func setFacingMode(_ mode: FacingMode) {
        cameraLock.lock()
        defer { cameraLock.unlock() }
        
        if facingMode == mode {
            return
        }
        
        facingMode = mode
        
        pipeIdsLock.lock()
        let hasActivePipes = !activePipeIds.isEmpty
        pipeIdsLock.unlock()
        
        if hasActivePipes {
            captureSession.stopRunning()
            captureSession.inputs.forEach { input in
                captureSession.removeInput(input)
            }
            
            DispatchQueue.global(qos: .userInitiated).async {
                do {
                    try self.prepare()
                    self.captureSession.startRunning()
                } catch {
                    NSLog("Failed to switch camera: \(error)")
                }
            }
        }
    }

    func switchCamera(facingMode: FacingMode) throws {
        setFacingMode(facingMode)
    }
    
    // AVCaptureVideoDataOutputSampleBufferDelegate
    func captureOutput(
        _ output: AVCaptureOutput,
        didOutput sampleBuffer: CMSampleBuffer,
        from connection: AVCaptureConnection
    ) {
        if connection.isVideoOrientationSupported {
            connection.videoOrientation = .portrait
        }
        
        if connection.isVideoMirroringSupported {
            connection.isVideoMirrored = facingMode == .user
        }
        
        pipeIdsLock.lock()
        let pipeIds = Array(activePipeIds)
        pipeIdsLock.unlock()
        
        FramePipeWrapper.cameraPublishVideo(sampleBuffer, pipeIds: pipeIds)
    }
    
    func addActivePipeId(_ pipeId: String) {
        pipeIdsLock.lock()
        activePipeIds.insert(pipeId)
        pipeIdsLock.unlock()
        
        if !captureSession.isRunning {
            DispatchQueue.global(qos: .userInitiated).async {
                self.captureSession.startRunning()
            }
        }
    }
    
    func removeActivePipeId(_ pipeId: String) {
        pipeIdsLock.lock()
        activePipeIds.remove(pipeId)
        let hasActivePipes = !activePipeIds.isEmpty
        pipeIdsLock.unlock()
        
        if !hasActivePipes && captureSession.isRunning {
            captureSession.stopRunning()
        }
    }
}

public class HybridCamera: HybridCameraSpec {
    private let cameraManager = CameraManager.shared
    public var pipeId: String = ""
    
    public func open(pipeId: String) throws -> Promise<Void> {
        guard !pipeId.isEmpty else {
            throw RuntimeError.error(withMessage: "Pipe ID cannot be empty")
        }
        
        return Promise.async {
            try self.cameraManager.prepare()
            self.pipeId = pipeId
            self.cameraManager.addActivePipeId(pipeId)
        }
    }

    public func switchCamera(facingMode: FacingMode) throws -> Promise<Void> {
        return Promise.async {
            try self.cameraManager.switchCamera(facingMode: facingMode)
        }
    }

    public func dispose() {
        cameraManager.removeActivePipeId(pipeId)
        pipeId = ""
    }
}
