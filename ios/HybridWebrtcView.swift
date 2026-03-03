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
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        displayLayer.videoGravity = .resizeAspectFill
        displayLayer.frame = bounds
        layer.addSublayer(displayLayer)
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        displayLayer.videoGravity = .resizeAspectFill
        displayLayer.frame = bounds
        layer.addSublayer(displayLayer)
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
    private static let sharedAudioSession = AVAudioSession.sharedInstance()
    private static var sharedAudioEngine = AVAudioEngine()
    private static let routeQueue = DispatchQueue(label: "com.speaker.manager.route.queue")
    private var audioPlayer = AVAudioPlayerNode()
    private var speakerQueue = DispatchQueue(label: "com.speaker.manager.audio.queue")
    private var routeObserver: NSObjectProtocol?
    private var pendingRouteRefreshWorkItem: DispatchWorkItem?
    var subscriptionId: Int32 = -1
    static var instanceNumber: Int = 0
    
    init(pipeId: String) throws {
        Self.instanceNumber += 1
        
        try configureAudioSession()
        try ensurePlayerReady()
        
        subscriptionId = FramePipeWrapper.speakerSubscribeAudio(
            audioPlayer, pipeId: pipeId, queue: speakerQueue
        )
        
        routeObserver = NotificationCenter.default.addObserver(
            forName: AVAudioSession.routeChangeNotification,
            object: Self.sharedAudioSession,
            queue: nil
        ) { [weak self] notification in
            self?.refreshRoute(notification: notification)
        }
    }
    
    deinit {
        pendingRouteRefreshWorkItem?.cancel()
        pendingRouteRefreshWorkItem = nil
        if let routeObserver = routeObserver {
            NotificationCenter.default.removeObserver(routeObserver)
            self.routeObserver = nil
        }
        Self.instanceNumber -= 1
        if Self.instanceNumber == 0 {
            Self.sharedAudioEngine.stop()
        }
        audioPlayer.stop()
        Self.sharedAudioEngine.disconnectNodeOutput(audioPlayer)
        Self.sharedAudioEngine.detach(audioPlayer)
        FramePipeWrapper.unsubscribe(subscriptionId)
    }
    
    private func refreshRoute(notification: Notification? = nil) {
        Self.routeQueue.async {
            self.pendingRouteRefreshWorkItem?.cancel()
            let work = DispatchWorkItem { [weak self] in
                guard let self = self else { return }
                do {
                    try self.configureAudioSession()
                    try self.applyOutputOverrideForCurrentRoute()
                    try self.restartPlayerGraphWithoutResubscribe()
                } catch {
                }
            }
            self.pendingRouteRefreshWorkItem = work
            Self.routeQueue.asyncAfter(deadline: .now() + 0.12, execute: work)
        }
    }
    
    private func configureAudioSession() throws {
        try Self.sharedAudioSession.setCategory(
            .playAndRecord,
            mode: .videoChat,
            options: [.defaultToSpeaker, .allowBluetooth, .allowBluetoothA2DP]
        )
        try Self.sharedAudioSession.setActive(true)
    }
    
    private func restartPlayerGraphWithoutResubscribe() throws {
        if audioPlayer.isPlaying {
            audioPlayer.stop()
        }
        audioPlayer.reset()
        
        if Self.sharedAudioEngine.isRunning {
            Self.sharedAudioEngine.stop()
        }
        
        Self.sharedAudioEngine.disconnectNodeOutput(audioPlayer)
        try ensurePlayerReady()
    }
    
    private func ensurePlayerReady() throws {
        if audioPlayer.engine == nil {
            Self.sharedAudioEngine.attach(audioPlayer)
        }
        Self.sharedAudioEngine.disconnectNodeOutput(audioPlayer)
        Self.sharedAudioEngine.connect(
            audioPlayer,
            to: Self.sharedAudioEngine.mainMixerNode,
            format: nil
        )
        if !Self.sharedAudioEngine.isRunning {
            try Self.sharedAudioEngine.start()
        }
        if !audioPlayer.isPlaying {
            audioPlayer.play()
        }
    }
    
    private func applyOutputOverrideForCurrentRoute() throws {
        let hasExternalOutput = Self.sharedAudioSession.currentRoute.outputs.contains { output in
            switch output.portType {
            case .headphones, .bluetoothA2DP, .bluetoothLE, .bluetoothHFP, .usbAudio, .airPlay:
                return true
            default:
                return false
            }
        }
    }
    
}

class HybridWebrtcView: HybridWebrtcViewSpec {
    var view: UIView = WebrtcDisplayView()
    var speaker: SpeakerManager? = nil
    
    var videoSubscriptionId: Int32 = -1
    fileprivate static let lock = NSLock()
    
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
