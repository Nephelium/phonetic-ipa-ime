import AppKit
import CoreText
import ImageIO

let output = URL(fileURLWithPath: CommandLine.arguments[1])
CTFontManagerRegisterFontsForURL(output.deletingLastPathComponent().appendingPathComponent("DoulosSIL-Regular.ttf") as CFURL, .process, nil)
var pages: [NSBitmapImageRep] = []
for pixels in [16, 32] {
    let image = NSImage(size: NSSize(width: pixels, height: pixels))
    image.lockFocus()
    let font = NSFont(name: "DoulosSIL", size: CGFloat(pixels) * 1.35) ?? NSFont.systemFont(ofSize: CGFloat(pixels))
    let glyph = NSAttributedString(string: "ɑ", attributes: [.font: font, .foregroundColor: NSColor.black])
    let size = glyph.size()
    glyph.draw(at: NSPoint(x: (CGFloat(pixels) - size.width) / 2, y: (CGFloat(pixels) - size.height) / 2))
    image.unlockFocus()
    let rep = NSBitmapImageRep(data: image.tiffRepresentation!)!
    rep.size = NSSize(width: 16, height: 16)
    pages.append(rep)
}
let destination = CGImageDestinationCreateWithURL(output as CFURL, "public.tiff" as CFString, pages.count, nil)!
for (index, page) in pages.enumerated() {
    CGImageDestinationAddImage(destination, page.cgImage!, [kCGImagePropertyDPIWidth: 72 * (index + 1), kCGImagePropertyDPIHeight: 72 * (index + 1)] as CFDictionary)
}
guard CGImageDestinationFinalize(destination) else { fatalError("Cannot write menu icon") }
