require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "Webrtc"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported, :visionos => 1.0 }
  s.source       = { :git => "https://github.com/SingTown/react-native-webrtc-nitro.git", :tag => "#{s.version}" }

  s.source_files = [
    # Implementation (Swift)
    "ios/**/*.{swift}",
    # Autolinking/Registration (Objective-C++)
    "ios/**/*.{h,m,mm}",
    # Implementation (C++ objects)
    "cpp/*.{h,hpp,cpp}",
    "cpp/FFmpeg/*.{h,hpp,cpp}",
    "cpp/Hybrid/*.{h,hpp,cpp}",
  ]

  s.public_header_files = "ios/**/*.h"
  s.vendored_frameworks = "3rdparty/output/ios/*.xcframework"
  s.pod_target_xcconfig = {
    'HEADER_SEARCH_PATHS' => [
      '$(PODS_TARGET_SRCROOT)/3rdparty/output/ios/ffmpeg.xcframework/ios-arm64/Headers',
      '$(PODS_TARGET_SRCROOT)/3rdparty/output/ios/libdatachannel.xcframework/ios-arm64/Headers',
    ].join(' ')
  }
  s.frameworks = 'AVFoundation', 'CoreMedia', 'CoreVideo', 'VideoToolbox'
  s.libraries  = 'z'

  load 'nitrogen/generated/ios/Webrtc+autolinking.rb'
  add_nitrogen_files(s)

  s.dependency 'React-jsi'
  s.dependency 'React-callinvoker'
  install_modules_dependencies(s)
end
