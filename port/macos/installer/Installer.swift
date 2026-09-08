import AppKit
import Foundation

private let releaseBase = "https://github.com/VHSMODDING/HitAndRun-macOS/releases/download/v0.5.0"
private let parts = [
    ("HitAndRun-v0.5.0-macOS-arm64.dmg", "161fafd5685ac8baa1d3c45cc4b77532c0516dd5777aa538b386a8bf00667a06"),
    ("HitAndRun-v0.5.0-macOS-arm64.002.dmgpart", "221199f45fa7de22260660f9d5d8acb7b28fa61946fcd4ea26a3d0262973b3ed")
]

private func run(_ executable: String, _ arguments: [String]) throws -> Data {
    let process = Process()
    let output = Pipe()
    process.executableURL = URL(fileURLWithPath: executable)
    process.arguments = arguments
    process.standardOutput = output
    process.standardError = output
    try process.run()
    process.waitUntilExit()
    let data = output.fileHandleForReading.readDataToEndOfFile()
    guard process.terminationStatus == 0 else {
        throw NSError(domain: "HitAndRunInstaller", code: Int(process.terminationStatus),
                      userInfo: [NSLocalizedDescriptionKey: String(data: data, encoding: .utf8) ?? "Command failed"])
    }
    return data
}

final class InstallerDelegate: NSObject, NSApplicationDelegate {
    private var window: NSWindow!
    private let status = NSTextField(labelWithString: "Ready to install")
    private let detail = NSTextField(wrappingLabelWithString: "Downloads and installs The Simpsons Hit & Run v0.5.0 in your Applications folder.")
    private let progress = NSProgressIndicator()
    private let button = NSButton(title: "Install", target: nil, action: nil)

    func applicationDidFinishLaunching(_ notification: Notification) {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 520, height: 300),
                          styleMask: [.titled, .closable], backing: .buffered, defer: false)
        window.title = "Hit & Run Installer"
        window.center()

        let icon = NSImageView(image: NSApplication.shared.applicationIconImage)
        icon.translatesAutoresizingMaskIntoConstraints = false
        icon.imageScaling = .scaleProportionallyUpOrDown
        status.font = .boldSystemFont(ofSize: 20)
        detail.textColor = .secondaryLabelColor
        detail.alignment = .center
        detail.maximumNumberOfLines = 3
        progress.style = .bar
        progress.isIndeterminate = true
        progress.isHidden = true
        button.bezelStyle = .rounded
        button.target = self
        button.action = #selector(install)

        let stack = NSStackView(views: [icon, status, detail, progress, button])
        stack.orientation = .vertical
        stack.alignment = .centerX
        stack.spacing = 14
        stack.edgeInsets = NSEdgeInsets(top: 20, left: 28, bottom: 22, right: 28)
        stack.translatesAutoresizingMaskIntoConstraints = false
        window.contentView?.addSubview(stack)
        NSLayoutConstraint.activate([
            icon.widthAnchor.constraint(equalToConstant: 82), icon.heightAnchor.constraint(equalToConstant: 82),
            detail.widthAnchor.constraint(equalToConstant: 450), progress.widthAnchor.constraint(equalToConstant: 430),
            stack.leadingAnchor.constraint(equalTo: window.contentView!.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: window.contentView!.trailingAnchor),
            stack.topAnchor.constraint(equalTo: window.contentView!.topAnchor),
            stack.bottomAnchor.constraint(equalTo: window.contentView!.bottomAnchor)
        ])
        window.makeKeyAndOrderFront(nil)
        NSApplication.shared.activate(ignoringOtherApps: true)
    }

    @objc private func install() {
        button.isEnabled = false
        progress.isHidden = false
        progress.startAnimation(nil)
        update("Downloading game data", "This is approximately 3.3 GB and may take some time.")
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do { try self?.performInstallation() }
            catch { self?.finish(error.localizedDescription) }
        }
    }

    private func performInstallation() throws {
        let fm = FileManager.default
        let cache = fm.urls(for: .cachesDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("com.vhsmodding.hitandrun.installer", isDirectory: true)
        try fm.createDirectory(at: cache, withIntermediateDirectories: true)
        for (index, part) in parts.enumerated() {
            update("Downloading part \(index + 1) of \(parts.count)", part.0)
            let destination = cache.appendingPathComponent(part.0)
            if fm.fileExists(atPath: destination.path) { try fm.removeItem(at: destination) }
            _ = try run("/usr/bin/curl", ["--location", "--fail", "--silent", "--show-error", "--retry", "3", "--output", destination.path,
                                         "\(releaseBase)/\(part.0)"])
            update("Verifying part \(index + 1) of \(parts.count)", part.0)
            let digest = String(data: try run("/usr/bin/shasum", ["-a", "256", destination.path]), encoding: .utf8) ?? ""
            guard digest.lowercased().hasPrefix(part.1) else {
                throw NSError(domain: "HitAndRunInstaller", code: 10,
                              userInfo: [NSLocalizedDescriptionKey: "Checksum verification failed for \(part.0)."])
            }
        }

        update("Opening package", "Preparing the application…")
        let dmg = cache.appendingPathComponent(parts[0].0)
        let plistData = try run("/usr/bin/hdiutil", ["attach", "-readonly", "-nobrowse", "-plist", dmg.path])
        guard let plist = try PropertyListSerialization.propertyList(from: plistData, options: [], format: nil) as? [String: Any],
              let entities = plist["system-entities"] as? [[String: Any]],
              let mount = entities.compactMap({ $0["mount-point"] as? String }).first else {
            throw NSError(domain: "HitAndRunInstaller", code: 11,
                          userInfo: [NSLocalizedDescriptionKey: "The downloaded disk image could not be mounted."])
        }
        defer { _ = try? run("/usr/bin/hdiutil", ["detach", mount]) }

        let source = URL(fileURLWithPath: mount).appendingPathComponent("The Simpsons Hit & Run.app")
        let applications = fm.homeDirectoryForCurrentUser.appendingPathComponent("Applications", isDirectory: true)
        let destination = applications.appendingPathComponent("The Simpsons Hit & Run.app")
        try fm.createDirectory(at: applications, withIntermediateDirectories: true)
        guard !fm.fileExists(atPath: destination.path) else {
            throw NSError(domain: "HitAndRunInstaller", code: 12,
                          userInfo: [NSLocalizedDescriptionKey: "The application already exists in ~/Applications. Move or remove it, then retry."])
        }
        update("Installing", "Copying the application to ~/Applications…")
        _ = try run("/usr/bin/ditto", [source.path, destination.path])
        for part in parts { try? fm.removeItem(at: cache.appendingPathComponent(part.0)) }
        DispatchQueue.main.async {
            self.progress.stopAnimation(nil)
            self.progress.isHidden = true
            self.status.stringValue = "Installation complete"
            self.detail.stringValue = "The game is installed in your Applications folder."
            self.button.title = "Open Game"
            self.button.action = #selector(self.openGame)
            self.button.isEnabled = true
        }
    }

    private func update(_ title: String, _ message: String) {
        DispatchQueue.main.async { self.status.stringValue = title; self.detail.stringValue = message }
    }

    private func finish(_ message: String) {
        DispatchQueue.main.async {
            self.progress.stopAnimation(nil); self.progress.isHidden = true
            self.status.stringValue = "Installation failed"; self.detail.stringValue = message
            self.button.title = "Retry"; self.button.isEnabled = true
        }
    }

    @objc private func openGame() {
        let url = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Applications/The Simpsons Hit & Run.app")
        NSWorkspace.shared.openApplication(at: url, configuration: NSWorkspace.OpenConfiguration())
    }
}

let app = NSApplication.shared
let delegate = InstallerDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
