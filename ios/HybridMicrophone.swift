//
//  HybridMicrophone.swift
//  Pods
//
//  Created by kaizhi-singtown on 2025/11/27.
//

import AVFoundation
import Foundation
import NitroModules
import AudioToolbox

private class MicrophoneManager {
    static let shared = MicrophoneManager()
    private let audioSession = AVAudioSession.sharedInstance()
    fileprivate var audioUnit: AudioUnit?
    private var activePipeIds: Set<String> = []
    private let pipeIdsLock = NSLock()
    private var sampleRate: Double = 44100
    private let channels: UInt32 = 1
    private var cachedFormatDesc: CMAudioFormatDescription?
    private let formatDescLock = NSLock()
    
    func prepare() throws {
        try audioSession.setCategory(
            .playAndRecord, mode: .videoChat, options: [.defaultToSpeaker, .allowBluetooth]
        )
        try audioSession.setActive(true)
        try setPreferredInputDevice()
        var desc = AudioComponentDescription(
            componentType: kAudioUnitType_Output,
            componentSubType: kAudioUnitSubType_VoiceProcessingIO,
            componentManufacturer: kAudioUnitManufacturer_Apple,
            componentFlags: 0,
            componentFlagsMask: 0
        )
        guard let comp = AudioComponentFindNext(nil, &desc) else {
            throw NSError(domain: "VoiceProcessingIO", code: -1)
        }
        AudioComponentInstanceNew(comp, &audioUnit)
        enableIO()
        setupFormat()
        setupInputCallback()
        AudioUnitInitialize(audioUnit!)
        setupSampleRate()
    }
    
    func addActivePipeId(_ pipeId: String) {
        pipeIdsLock.lock()
        let wasEmpty = activePipeIds.isEmpty
        activePipeIds.insert(pipeId)
        pipeIdsLock.unlock()
        if wasEmpty {
            guard let au = audioUnit else { return }
            AudioOutputUnitStart(au)
        }
    }
    
    func removeActivePipeId(_ pipeId: String) {
        pipeIdsLock.lock()
        activePipeIds.remove(pipeId)
        let empty = activePipeIds.isEmpty
        pipeIdsLock.unlock()
        if empty {
            teardownAudioUnit()
        }
    }

    private func teardownAudioUnit() {
        guard let au = audioUnit else { return }
        AudioOutputUnitStop(au)
        AudioUnitUninitialize(au)
        AudioComponentInstanceDispose(au)
        audioUnit = nil
    }
    
    private func setPreferredInputDevice() throws {
        guard let inputs = audioSession.availableInputs, !inputs.isEmpty else { return }
        if let externalMic = inputs.first(where: {
            $0.portType == .headsetMic || $0.portType == .bluetoothHFP || $0.portType == .usbAudio
        }) {
            try audioSession.setPreferredInput(externalMic)
            return
        }
        if let builtInMic = inputs.first(where: { $0.portType == .builtInMic }) {
            try audioSession.setPreferredInput(builtInMic)
        }
    }
    
    private func enableIO() {
        var one: UInt32 = 1
        // mic input bus = 1
        AudioUnitSetProperty(
            audioUnit!,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input,
            1,
            &one,
            UInt32(MemoryLayout.size(ofValue: one))
        )
        // speaker output bus = 0
        AudioUnitSetProperty(
            audioUnit!,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Output,
            0,
            &one,
            UInt32(MemoryLayout.size(ofValue: one))
        )
    }
    
    private func setupFormat() {
        var format = AudioStreamBasicDescription(
            mSampleRate: sampleRate,
            mFormatID: kAudioFormatLinearPCM,
            mFormatFlags: kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
            mBytesPerPacket: 2,
            mFramesPerPacket: 1,
            mBytesPerFrame: 2,
            mChannelsPerFrame: channels,
            mBitsPerChannel: 16,
            mReserved: 0
        )
        // mic output -> app
        AudioUnitSetProperty(
            audioUnit!,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            1,
            &format,
            UInt32(MemoryLayout.size(ofValue: format))
        )
        // app -> speaker input
        AudioUnitSetProperty(
            audioUnit!,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            0,
            &format,
            UInt32(MemoryLayout.size(ofValue: format))
        )
    }
    
    private func setupSampleRate() {
        var asbd = AudioStreamBasicDescription()
        var size = UInt32(MemoryLayout.size(ofValue: asbd))
        AudioUnitGetProperty(
            audioUnit!,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            1,
            &asbd,
            &size
        )
        sampleRate = asbd.mSampleRate
    }
    
    private func setupInputCallback() {
        var callback = AURenderCallbackStruct(
            inputProc: recordingCallback,
            inputProcRefCon: UnsafeMutableRawPointer(Unmanaged.passUnretained(self).toOpaque())
        )
        AudioUnitSetProperty(
            audioUnit!,
            kAudioOutputUnitProperty_SetInputCallback,
            kAudioUnitScope_Global,
            1,
            &callback,
            UInt32(MemoryLayout.size(ofValue: callback))
        )
    }
    
    fileprivate func publishPCM(_ pcm: UnsafeMutableRawPointer, frames: UInt32) {
        pipeIdsLock.lock()
        let pipeIds = Array(activePipeIds)
        pipeIdsLock.unlock()
        if pipeIds.isEmpty { return }
        let formatDesc: CMAudioFormatDescription? = {
            if let fd = cachedFormatDesc { return fd }
            formatDescLock.lock()
            defer { formatDescLock.unlock() }
            if let fd = cachedFormatDesc { return fd }
            var asbd = AudioStreamBasicDescription(
                mSampleRate: sampleRate,
                mFormatID: kAudioFormatLinearPCM,
                mFormatFlags: kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
                mBytesPerPacket: 2,
                mFramesPerPacket: 1,
                mBytesPerFrame: 2,
                mChannelsPerFrame: channels,
                mBitsPerChannel: 16,
                mReserved: 0
            )
            var fdOut: CMAudioFormatDescription?
            let st = CMAudioFormatDescriptionCreate(
                allocator: kCFAllocatorDefault,
                asbd: &asbd,
                layoutSize: 0,
                layout: nil,
                magicCookieSize: 0,
                magicCookie: nil,
                extensions: nil,
                formatDescriptionOut: &fdOut
            )
            if st == noErr {
                cachedFormatDesc = fdOut
            }
            return fdOut
        }()
        guard let fd = formatDesc else { return }
        let dataSize = Int(frames * 2)
        var blockBuffer: CMBlockBuffer?
        let stBB = CMBlockBufferCreateWithMemoryBlock(
            allocator: kCFAllocatorDefault,
            memoryBlock: nil,
            blockLength: dataSize,
            blockAllocator: kCFAllocatorDefault,
            customBlockSource: nil,
            offsetToData: 0,
            dataLength: dataSize,
            flags: 0,
            blockBufferOut: &blockBuffer
        )
        guard stBB == kCMBlockBufferNoErr, let bb = blockBuffer else { return }
        // copy bytes
        let copySt: OSStatus = pcm.withMemoryRebound(to: UInt8.self, capacity: dataSize) { srcBytes in
            CMBlockBufferReplaceDataBytes(
                with: srcBytes,
                blockBuffer: bb,
                offsetIntoDestination: 0,
                dataLength: dataSize
            )
        }
        guard copySt == kCMBlockBufferNoErr else { return }
        var sampleBuffer: CMSampleBuffer?
        let stSB = CMSampleBufferCreate(
            allocator: kCFAllocatorDefault,
            dataBuffer: bb,
            dataReady: true,
            makeDataReadyCallback: nil,
            refcon: nil,
            formatDescription: fd,
            sampleCount: CMItemCount(frames),
            sampleTimingEntryCount: 0,
            sampleTimingArray: nil,
            sampleSizeEntryCount: 0,
            sampleSizeArray: nil,
            sampleBufferOut: &sampleBuffer
        )
        guard stSB == noErr, let sb = sampleBuffer else { return }
        FramePipeWrapper.microphonePublishAudio(sb, pipeIds: pipeIds)
    }
}

private let recordingCallback: AURenderCallback = {
    inRefCon,
    ioActionFlags,
    inTimeStamp,
    inBusNumber,
    inNumberFrames,
    ioData in
    let manager =
    Unmanaged<MicrophoneManager>
        .fromOpaque(inRefCon)
        .takeUnretainedValue()
    let byteSize = Int(inNumberFrames * 2)
    var bufferList = AudioBufferList(
        mNumberBuffers: 1,
        mBuffers: AudioBuffer(
            mNumberChannels: 1,
            mDataByteSize: UInt32(byteSize),
            mData: malloc(byteSize)
        )
    )
    let status = AudioUnitRender(
        manager.audioUnit!,
        ioActionFlags,
        inTimeStamp,
        1,
        inNumberFrames,
        &bufferList
    )
    if status == noErr {
        manager.publishPCM(
            bufferList.mBuffers.mData!,
            frames: inNumberFrames
        )
    }
    free(bufferList.mBuffers.mData)
    return noErr
}

public class HybridMicrophone: HybridMicrophoneSpec {
    private let microphoneManager = MicrophoneManager.shared
    public var pipeId: String = ""
    
    public func open(pipeId: String) throws -> Promise<Void> {
        guard !pipeId.isEmpty else {
            throw RuntimeError.error(withMessage: "Pipe ID cannot be empty")
        }
        return Promise.async {
            try self.microphoneManager.prepare()
            self.pipeId = pipeId
            self.microphoneManager.addActivePipeId(pipeId)
        }
    }
    
    public func dispose() {
        microphoneManager.removeActivePipeId(pipeId)
        pipeId = ""
    }
}
