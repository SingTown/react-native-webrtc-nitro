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
  private static var sharedAudioSession = AVAudioSession()
  private static var sharedAudioEngine = AVAudioEngine()
  private var audioPlayer = AVAudioPlayerNode()
  private var speakerQueue = DispatchQueue(label: "com.speaker.manager.audio.queue")
  let subscriptionId: Int32
  static var instanceNumber: Int = 0

  init(pipeId: String) {
    Self.instanceNumber += 1

    do {
      try Self.sharedAudioSession.setCategory(
        .playAndRecord, mode: .videoChat, options: [.defaultToSpeaker, .allowBluetooth])
      try Self.sharedAudioSession.setActive(true)
    } catch {
      print("[Speaker] Audio session setup failed: \(error)")
    }

    Self.sharedAudioEngine.attach(audioPlayer)

    // Get the output format from the main mixer node
    let outputFormat = Self.sharedAudioEngine.mainMixerNode.outputFormat(forBus: 0)
    print("[Speaker] Engine output format: \(outputFormat)")

    Self.sharedAudioEngine.connect(
      audioPlayer, to: Self.sharedAudioEngine.mainMixerNode, format: outputFormat)

    do {
      try Self.sharedAudioEngine.start()
      audioPlayer.play()
      print("[Speaker] Audio engine started successfully")
    } catch {
      print("[Speaker] Failed to start audio engine: \(error)")
    }

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

  var audioPipeId: String? {
    didSet {
      Self.lock.lock()
      defer { Self.lock.unlock() }

      speaker = nil
      if audioPipeId != nil {
        speaker = SpeakerManager(pipeId: audioPipeId!)
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
