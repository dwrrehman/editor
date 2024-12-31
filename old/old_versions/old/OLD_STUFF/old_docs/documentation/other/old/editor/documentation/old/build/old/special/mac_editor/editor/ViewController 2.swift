//
//  ViewController.swift
//  editor
//
//  Created by Daniel Rehman on 2010047.
//

import Cocoa

class ViewController: NSViewController {
    
    private var font_size: CGFloat = 20.0
    private var at: Int = 0
    private lazy var text = NSText(frame: NSRect(x: 0, y: -20, width: 500, height: 500))
    
    override func loadView() {
        self.view = NSView(frame: NSRect(x: 0, y:-20, width: 500, height: 500))
            
    }
    
    private func highlight() {
        text.textColor = .white
        
        if (text.string.count >= 10) {
            text.setTextColor(.init(red: 0.5, green: 0.5, blue: 0.5, alpha: 1.0), range: NSRange(location: 0, length: 10))
        }
    }
    
    private func set_text(_ str: String) {
        text.string = str
        highlight()
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        view.addSubview(text)
        
        text.wantsLayer = true
        text.isEditable = false
        text.backgroundColor = .black
        text.font = NSFont(name: "Menlo", size: font_size)
        
        set_text("")
        
        NSEvent.addLocalMonitorForEvents(matching: .keyDown) {
            if self.keydown(with: $0) { return nil }
            else { return $0 }
        }
    }
    
    func keydown(with event: NSEvent) -> Bool {
        guard let locWindow = self.view.window, NSApplication.shared.keyWindow === locWindow else { return false }
        
        if event.charactersIgnoringModifiers == String(UnicodeScalar(NSDeleteCharacter)!) {
            set_text(String(text.string.dropLast()))
            return true
        }
        
        if event.keyCode == 123 {
            print("left")
            text.moveLeft(self)
            return true
        }
        
        if event.keyCode == 124 {
            print("right")
            text.moveRight(self)
            return true
        }
        
        if event.keyCode == 125 {
            print("down")
            text.moveDown(self)
            return true
        }
        
        if event.keyCode == 126 {
            print("up")
            text.moveUp(self)
            return true
        }
        
        let str = event.characters ?? "";
        
        if event.modifierFlags.contains(.command) {
            
            if str == "q" {
                exit(0)
            }
            if str == "w" {
                exit(0)
            }
            
            if str == "s" {
                print(text.string)
            }
            
            if str == "=" {
                if (font_size < 200) { font_size += 1 }
                text.font = NSFont(name: "Menlo", size: font_size)
            }
            
            if str == "-" {
                if font_size > 2 { font_size -= 1 }
                text.font = NSFont(name: "Menlo", size: font_size)
            }
        } else {
            set_text(text.string + str)
        }
        
        print(event.keyCode)
        print(text.string.count, text.string)
        return true
    }
}
