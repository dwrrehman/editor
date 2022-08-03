//
//  AppDelegate.swift
//  editor
//
//  Created by Daniel Rehman on 2010047.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    private var window: NSWindow!

    func applicationDidFinishLaunching(_ aNotification: Notification) {
         
        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 500, height: 500),
            styleMask: [ .titled, .closable, .miniaturizable, .resizable, .fullSizeContentView],
            backing: .buffered, defer: false)
                        
        window.setFrameAutosaveName("editor")
        window.isReleasedWhenClosed = true
        window.titlebarAppearsTransparent = true
        window.backgroundColor = .black
        window.title = "hi"
        window.contentViewController = ViewController()
        
        window.makeKeyAndOrderFront(nil)
    }

    func applicationWillTerminate(_ aNotification: Notification) {
       exit(0)
    }
}
