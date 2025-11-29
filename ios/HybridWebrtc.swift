//
//  HybridWebrtc.swift
//  Pods
//
//  Created by kaizhi-singtown on 2025/11/29.
//

import Foundation
import UIKit

class HybridWebrtcView : HybridWebrtcViewSpec {
  // UIView
  var view: UIView = UIView()

  // Props
  var isRed: Bool = false {
    didSet {
      view.backgroundColor = isRed ? .red : .black
    }
  }
}
