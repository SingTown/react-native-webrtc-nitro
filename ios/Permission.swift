import AVFoundation
import Foundation
import NitroModules

public func requestPermission(for mediaType: AVMediaType) async throws {
    let status = AVCaptureDevice.authorizationStatus(for: mediaType)
    switch status {
    case .authorized:
        return
    case .notDetermined:
        let granted = await withCheckedContinuation { (continuation) in
            AVCaptureDevice.requestAccess(for: mediaType) { ok in
                continuation.resume(returning: ok)
            }
        }
        if granted {
            return
        } else {
            throw RuntimeError.error(withMessage: "Permission not allowed")
        }
    case .denied, .restricted:
        throw RuntimeError.error(withMessage: "Permission not allowed")
    @unknown default:
        throw RuntimeError.error(withMessage: "Permission not allowed")
    }
}
