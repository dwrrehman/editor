//
//  ContentView.swift
//  editor
//
//  Created by Daniel Rehman on 2010047.
//

//import SwiftUI

//
//
//struct KeyEventHandling: NSViewRepresentable {
//    class KeyView: NSView {
//        override var acceptsFirstResponder: Bool { true }
//        override func keyDown(with event: NSEvent) {
//            super.keyDown(with: event)
//            print(">> key \(event.charactersIgnoringModifiers ?? "")")
//        }
//    }
//
//    func makeNSView(context: Context) -> NSView {
//        let view = KeyView()
//        DispatchQueue.main.async { // wait till next event cycle
//            view.window?.makeFirstResponder(view)
//        }
//        return view
//    }
//
//    func updateNSView(_ nsView: NSView, context: Context) {
//    }
//}
//
//struct TestKeyboardEventHandling: View {
//    var body: some View {
//        Text("Hello, World!")
//            .background(KeyEventHandling())
//    }
//}

//
//
//struct ContentView: View {
//
//    var body: some View {
//        Text("hello there from space.")
//            .frame(maxWidth: .infinity, maxHeight: .infinity)
//            .foregroundColor(.red)
//            .font(.custom("Menlo", size: 30.0))
//
//    }
//
//}
//
//struct ContentView_Previews: PreviewProvider {
//    static var previews: some View {
//        ContentView()
//    }
//}




