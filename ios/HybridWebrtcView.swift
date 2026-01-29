//
//  HybridWebrtcView.swift
//  Pods
//
//  Created by kaizhi-singtown on 2025/11/27.
//

import AVFoundation
import Foundation
import UIKit

class WebrtcDisplayView: UIView {
    var displayLayer: AVSampleBufferDisplayLayer = AVSampleBufferDisplayLayer()

    var resizeMode: ResizeMode? = .contain {
        didSet {
            updateVideoGravity()
        }
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        updateVideoGravity()
        displayLayer.frame = bounds
        layer.addSublayer(displayLayer)
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        updateVideoGravity()
        displayLayer.frame = bounds
        layer.addSublayer(displayLayer)
    }

    private func updateVideoGravity() {
        switch resizeMode {
        case .contain, .none:
            displayLayer.videoGravity = AVLayerVideoGravity.resizeAspect
        case .cover:
            displayLayer.videoGravity = AVLayerVideoGravity.resizeAspectFill
        case .fill:
            displayLayer.videoGravity = AVLayerVideoGravity.resize
        }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        displayLayer.frame = bounds
    }
    
    deinit {
        displayLayer.flushAndRemoveImage()
    }
}

class SpeakerManager {
    private static var sharedAudioSession = AVAudioSession()
    private static var sharedAudioEngine = AVAudioEngine()
    private var audioPlayer = AVAudioPlayerNode()
    private var speakerQueue = DispatchQueue(label: "com.speaker.manager.audio.queue")
    let subscriptionId: Int32
    static var instanceNumber: Int = 0
    
    init(pipeId: String) throws {
        Self.instanceNumber += 1
        
        try Self.sharedAudioSession.setCategory(
            .playAndRecord, mode: .videoChat, options: [.defaultToSpeaker])
        try Self.sharedAudioSession.setActive(true)
        
        Self.sharedAudioEngine.attach(audioPlayer)
        
        let outputFormat = Self.sharedAudioEngine.mainMixerNode.outputFormat(forBus: 0)
        
        Self.sharedAudioEngine.connect(
            audioPlayer, to: Self.sharedAudioEngine.mainMixerNode, format: outputFormat)
        
        try Self.sharedAudioEngine.start()
        audioPlayer.play()
        
        subscriptionId = FramePipeWrapper.speakerSubscribeAudio(
            audioPlayer, pipeId: pipeId, queue: speakerQueue
        )
    }
    
    deinit {
        Self.instanceNumber -= 1
        if Self.instanceNumber == 0 {
            Self.sharedAudioEngine.stop()
        }
        audioPlayer.stop()
        Self.sharedAudioEngine.disconnectNodeOutput(audioPlayer)
        Self.sharedAudioEngine.detach(audioPlayer)
        FramePipeWrapper.unsubscribe(subscriptionId)
    }
}

class HybridWebrtcView: HybridWebrtcViewSpec {
    var view: UIView = WebrtcDisplayView()
    var speaker: SpeakerManager? = nil
    
    var videoSubscriptionId: Int32 = -1
    fileprivate static let lock = NSLock()

    var resizeMode: ResizeMode? {
        didSet {
            if let displayView = view as? WebrtcDisplayView {
                displayView.resizeMode = resizeMode
            }
        }
    }

    var audioPipeId: String? {
        didSet {
            Self.lock.lock()
            defer { Self.lock.unlock() }
            
            speaker = nil
            if audioPipeId != nil {
                speaker = try? SpeakerManager(pipeId: audioPipeId!)
            }
        }
    }
    
    var videoPipeId: String? {
        didSet {
            FramePipeWrapper.unsubscribe(videoSubscriptionId)
            videoSubscriptionId = -1
            guard let videoPipeId = videoPipeId else {
                return
            }
            if let displayView = view as? WebrtcDisplayView {
                videoSubscriptionId = FramePipeWrapper.viewSubscribeVideo(
                    displayView.displayLayer, pipeId: videoPipeId)
            }
        }
    }
    
    deinit {
        Self.lock.lock()
        defer { Self.lock.unlock() }
        
        if videoSubscriptionId != -1 {
            FramePipeWrapper.unsubscribe(videoSubscriptionId)
        }
        
    }
}
