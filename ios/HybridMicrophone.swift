//
//  HybridMicrophone.swift
//  Pods
//
//  Created by kaizhi-singtown on 2025/11/27.
//

import AVFoundation
import Foundation
import NitroModules

private class MicrophoneManager: NSObject, AVCaptureAudioDataOutputSampleBufferDelegate {
    static let shared = MicrophoneManager()
    
    let audioSession = AVAudioSession.sharedInstance()
    let captureSession = AVCaptureSession()
    let audioOutput = AVCaptureAudioDataOutput()
    let audioQueue = DispatchQueue(label: "com.microphone.shared.audio.queue")
    
    private var activePipeIds: Set<String> = []
    private let pipeIdsLock = NSLock()
    
    func prepare() throws {
        captureSession.sessionPreset = .high
        try audioSession.setCategory(
            .playAndRecord, mode: .videoChat, options: [.defaultToSpeaker])
        try audioSession.setActive(true)
        
        guard let audioDevice = AVCaptureDevice.default(for: .audio) else {
            throw RuntimeError.error(withMessage: "No audio device found")
        }
        let audioInput = try AVCaptureDeviceInput(device: audioDevice)
        if captureSession.canAddInput(audioInput) {
            captureSession.addInput(audioInput)
        }
        audioOutput.setSampleBufferDelegate(self, queue: audioQueue)
        if captureSession.canAddOutput(audioOutput) {
            captureSession.addOutput(audioOutput)
        }
    }
    
    // AVCaptureAudioDataOutputSampleBufferDelegate
    func captureOutput(
        _ output: AVCaptureOutput,
        didOutput sampleBuffer: CMSampleBuffer,
        from connection: AVCaptureConnection
    ) {
        pipeIdsLock.lock()
        let pipeIds = Array(activePipeIds)
        pipeIdsLock.unlock()
        
        FramePipeWrapper.microphonePublishAudio(sampleBuffer, pipeIds: pipeIds)
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
            DispatchQueue.global(qos: .userInitiated).async {
                self.captureSession.stopRunning()
            }
        }
    }
}

public class HybridMicrophone: HybridMicrophoneSpec {
    private let microphoneManager = MicrophoneManager.shared
    public var pipeId: String = ""
    
    public func open(pipeId: String) throws -> Promise<Void> {
        guard !pipeId.isEmpty else {
            throw RuntimeError.error(withMessage: "Pipe ID cannot be empty")
        }
        
        try microphoneManager.prepare()
        
        return Promise.async {
            try await requestPermission(for: .audio)
            self.pipeId = pipeId
            self.microphoneManager.addActivePipeId(pipeId)
        }
    }
    
    public func dispose() {
        microphoneManager.removeActivePipeId(pipeId)
        pipeId = ""
    }
}
