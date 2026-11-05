package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"image/png"
	"runtime"
	"syscall"
	"time"
	"unsafe"
)

//go:embed assets/background.png
var backdrop []byte
var gdi = syscall.NewLazyDLL("gdi32.dll")

type rect struct{ L, T, R, B int32 }
type point struct{ X, Y int32 }
type winMessage struct {
	Window  uintptr
	ID      uint32
	W, L    uintptr
	Time    uint32
	Point   point
	Private uint32
}
type windowClass struct {
	Size, Style                   uint32
	Proc                          uintptr
	ClassExtra, WindowExtra       int32
	Instance, Icon, Cursor, Brush uintptr
	Menu, Name                    *uint16
	SmallIcon                     uintptr
}
type paintStruct struct {
	DC              uintptr
	Erase           int32
	Area            rect
	Restore, Update int32
	Reserved        [32]byte
}
type bitmapInfo struct {
	Size                   uint32
	Width, Height          int32
	Planes, Bits           uint16
	Compression, ImageSize uint32
	XPels, YPels           int32
	Used, Important        uint32
}
type drawItem struct {
	Type, ID, Item, Action, State uint32
	Window, DC                    uintptr
	Area                          rect
	Data                          uintptr
}

type launcherWindow struct {
	app                                             *application
	prefs                                           preferences
	hwnd, notes, play, auto, language, install      uintptr
	font, titleFont, smallFont, brush               uintptr
	width, height                                   int32
	scale                                           float64
	pixels                                          []byte
	bitmap                                          bitmapInfo
	view                                            view
	lastNotes                                       string
	lastOffer                                       int
	lastInstall, lastButton, lastAuto, lastLanguage string
	readyAt                                         time.Time
	wantPlay, closing                               bool
}

var activeWindow *launcherWindow

func (w *launcherWindow) s(n int) int32 { return int32(float64(n) * w.scale) }
func (w *launcherWindow) tr(fr, en string) string {
	if w.prefs.Language == "fr" {
		return fr
	}
	return en
}
func (w *launcherWindow) status() string { return statusText(w.view, w.prefs.Language) }
func (w *launcherWindow) text(dc uintptr, text string, r rect, font, color uintptr, flags uintptr) {
	old, _, _ := gdi.NewProc("SelectObject").Call(dc, font)
	gdi.NewProc("SetTextColor").Call(dc, color)
	gdi.NewProc("SetBkMode").Call(dc, 1)
	user.NewProc("DrawTextW").Call(dc, ptr(text), ^uintptr(0), uintptr(unsafe.Pointer(&r)), flags)
	gdi.NewProc("SelectObject").Call(dc, old)
}
func (w *launcherWindow) box(dc uintptr, r rect, color uintptr) {
	b, _, _ := gdi.NewProc("CreateSolidBrush").Call(color)
	user.NewProc("FillRect").Call(dc, uintptr(unsafe.Pointer(&r)), b)
	gdi.NewProc("DeleteObject").Call(b)
}
func (w *launcherWindow) paint() {
	var ps paintStruct
	dc, _, _ := user.NewProc("BeginPaint").Call(w.hwnd, uintptr(unsafe.Pointer(&ps)))
	memory, _, _ := gdi.NewProc("CreateCompatibleDC").Call(dc)
	bitmap, _, _ := gdi.NewProc("CreateCompatibleBitmap").Call(dc, uintptr(w.width), uintptr(w.height))
	old, _, _ := gdi.NewProc("SelectObject").Call(memory, bitmap)
	gdi.NewProc("SetStretchBltMode").Call(memory, 4)
	gdi.NewProc("StretchDIBits").Call(memory, 0, 0, uintptr(w.width), uintptr(w.height), 0, 0, uintptr(w.bitmap.Width), uintptr(-w.bitmap.Height), uintptr(unsafe.Pointer(&w.pixels[0])), uintptr(unsafe.Pointer(&w.bitmap)), 0, 0x00cc0020)
	gold := uintptr(0x9fcef1)
	muted := uintptr(0x83999e)
	w.text(memory, w.status(), rect{w.s(60), w.s(238), w.s(840), w.s(270)}, w.titleFont, gold, 1|4|0x20)
	// Thin brass progress track; an indeterminate moving segment while checking.
	track := rect{w.s(80), w.s(282), w.s(820), w.s(287)}
	w.box(memory, track, 0x26303c)
	if !w.view.Ready {
		if w.view.Phase == "download" || w.view.Phase == "uap-install" {
			track.R = track.L + int32(int64(track.R-track.L)*int64(w.view.Percent)/100)
		} else {
			width := track.R - track.L
			track.L += int32(time.Now().UnixMilli() / 12 % int64(width-w.s(95)))
			track.R = track.L + w.s(95)
		}
	} else {
		track.R = track.L + (track.R - track.L)
	}
	w.box(memory, track, 0x6199c1)
	label := w.view.heading(w.prefs.Language)
	w.text(memory, label, rect{w.s(82), w.s(307), w.s(820), w.s(332)}, w.smallFont, muted, 0x20)
	w.text(memory, fmt.Sprintf(w.tr("Version installée : %s", "Installed version: %s"), w.view.Installed), rect{w.s(80), w.s(576), w.s(820), w.s(600)}, w.smallFont, muted, 0x20)
	if !w.readyAt.IsZero() && w.prefs.AutoPlay && w.view.autoStart() {
		remaining := 4 - int(time.Since(w.readyAt).Seconds())
		if remaining < 0 {
			remaining = 0
		}
		w.text(memory, fmt.Sprintf(w.tr("Lancement dans %d s", "Starting in %d s"), remaining), rect{w.s(480), w.s(576), w.s(820), w.s(600)}, w.smallFont, gold, 2|0x20)
	}
	gdi.NewProc("BitBlt").Call(dc, 0, 0, uintptr(w.width), uintptr(w.height), memory, 0, 0, 0x00cc0020)
	gdi.NewProc("SelectObject").Call(memory, old)
	gdi.NewProc("DeleteObject").Call(bitmap)
	gdi.NewProc("DeleteDC").Call(memory)
	user.NewProc("EndPaint").Call(w.hwnd, uintptr(unsafe.Pointer(&ps)))
}
func (w *launcherWindow) drawButton(d *drawItem) {
	color := uintptr(0x182832)
	if d.State&1 != 0 {
		color = 0x293c4c
	}
	w.box(d.DC, d.Area, color)
	r := d.Area
	border, _, _ := gdi.NewProc("CreateSolidBrush").Call(0x5c829c)
	user.NewProc("FrameRect").Call(d.DC, uintptr(unsafe.Pointer(&r)), border)
	gdi.NewProc("DeleteObject").Call(border)
	text := ""
	switch d.ID {
	case 101:
		text = w.view.playLabel(w.prefs.Language)
	case 105:
		text = w.tr("Installer", "Install")
	case 103:
		text = "FR  /  EN"
	}
	tint := uintptr(0x9fcef1)
	if d.State&4 != 0 {
		tint = 0x677477
	}
	w.text(d.DC, text, r, w.font, tint, 1|4|0x20)
	if d.State&0x10 != 0 {
		r.L += 4
		r.T += 4
		r.R -= 4
		r.B -= 4
		user.NewProc("DrawFocusRect").Call(d.DC, uintptr(unsafe.Pointer(&r)))
	}
}
func (w *launcherWindow) refresh() {
	previous := w.view
	w.view = w.app.snapshot()
	if w.view.Phase == "uap-failed" && previous.Phase != "uap-failed" {
		w.wantPlay = false
	}
	note := w.view.notes(w.prefs.Language)
	if note != w.lastNotes {
		user.NewProc("SetWindowTextW").Call(w.notes, ptr(note))
		w.lastNotes = note
	}
	autoText := w.tr("Lancer le jeu automatiquement", "Launch the game automatically")
	if autoText != w.lastAuto {
		user.NewProc("SetWindowTextW").Call(w.auto, ptr(autoText))
		w.lastAuto = autoText
	}
	button := w.view.playLabel(w.prefs.Language)
	if button != w.lastButton {
		user.NewProc("SetWindowTextW").Call(w.play, ptr(button))
		w.lastButton = button
	}
	enabled := uintptr(1)
	if w.view.busy() || w.view.Fatal {
		enabled = 0
	}
	user.NewProc("EnableWindow").Call(w.play, enabled)
	if w.lastOffer != w.view.Offer {
		showAuto, showInstall := uintptr(1), uintptr(0)
		if w.view.Offer != 0 {
			showAuto, showInstall = 0, 1
			w.wantPlay = false
		}
		user.NewProc("ShowWindow").Call(w.auto, showAuto)
		user.NewProc("ShowWindow").Call(w.install, showInstall)
		w.lastOffer = w.view.Offer
	}
	installText := w.tr("Installer", "Install")
	if w.lastInstall != installText {
		user.NewProc("SetWindowTextW").Call(w.install, ptr(installText))
		w.lastInstall = installText
	}
	if !w.view.Ready {
		w.readyAt = time.Time{}
	}
	if w.view.Ready && w.readyAt.IsZero() {
		w.readyAt = time.Now()
	}
	if previous != w.view || w.lastLanguage != w.prefs.Language || w.view.Ready || w.view.Phase == "check" || w.view.Phase == "verify" || w.view.Phase == "install" {
		user.NewProc("InvalidateRect").Call(w.hwnd, 0, 0)
	}
	w.lastLanguage = w.prefs.Language
	if w.closing && w.view.Ready {
		user.NewProc("DestroyWindow").Call(w.hwnd)
		return
	}
	if w.view.Ready && !w.view.Fatal && (w.wantPlay || (w.view.autoStart() && w.prefs.AutoPlay && time.Since(w.readyAt) >= 4*time.Second)) {
		user.NewProc("KillTimer").Call(w.hwnd, 1)
		user.NewProc("ShowWindow").Call(w.hwnd, 0)
		if err := w.app.launch(); err != nil {
			message(err.Error())
		}
		user.NewProc("DestroyWindow").Call(w.hwnd)
	}
}
func windowProcedure(hwnd uintptr, msg uint32, wp, lp uintptr) uintptr {
	w := activeWindow
	if w == nil {
		r, _, _ := user.NewProc("DefWindowProcW").Call(hwnd, uintptr(msg), wp, lp)
		return r
	}
	switch msg {
	case 0x14:
		return 1 // background painted in a single buffered pass
	case 0xf:
		w.paint()
		return 0
	case 0x113:
		w.refresh()
		return 0
	case 0x2b:
		w.drawButton((*drawItem)(unsafe.Pointer(lp)))
		return 1
	case 0x138, 0x133:
		gdi.NewProc("SetTextColor").Call(wp, 0xb0c9dc)
		gdi.NewProc("SetBkColor").Call(wp, 0x0c1117)
		return w.brush
	case 0x111:
		switch wp & 0xffff {
		case 1, 101:
			if w.view.Offer != 0 {
				w.app.respond(false)
			} else if !w.view.Fatal && !w.view.busy() {
				w.wantPlay = true
				w.app.cancel()
			}
		case 2:
			user.NewProc("PostMessageW").Call(hwnd, 0x10, 0, 0)
		case 105:
			w.app.respond(true)
		case 102:
			w.prefs.AutoPlay = !w.prefs.AutoPlay
			if !w.app.preview {
				savePreferences(w.app.root, w.prefs)
			}
			w.readyAt = time.Now()
		case 103:
			if w.prefs.Language == "fr" {
				w.prefs.Language = "en"
			} else {
				w.prefs.Language = "fr"
			}
			if !w.app.preview {
				savePreferences(w.app.root, w.prefs)
			}
		}
		w.refresh()
		return 0
	case 0x10:
		if w.view.busy() {
			return 0
		}
		w.closing = true
		w.app.stop()
		if w.app.preview {
			user.NewProc("DestroyWindow").Call(hwnd)
		} else {
			w.refresh()
		}
		return 0
	case 2:
		user.NewProc("PostQuitMessage").Call(0)
		return 0
	}
	r, _, _ := user.NewProc("DefWindowProcW").Call(hwnd, uintptr(msg), wp, lp)
	return r
}
func showLauncher(a *application, p preferences) error {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	user.NewProc("SetProcessDPIAware").Call()
	w := &launcherWindow{app: a, prefs: p, scale: 1, view: a.snapshot(), lastOffer: -1}
	activeWindow = w
	defer func() { activeWindow = nil }()
	image, err := png.Decode(bytes.NewReader(backdrop))
	if err != nil {
		return err
	}
	bounds := image.Bounds()
	w.bitmap = bitmapInfo{Size: 40, Width: int32(bounds.Dx()), Height: -int32(bounds.Dy()), Planes: 1, Bits: 32}
	w.pixels = make([]byte, bounds.Dx()*bounds.Dy()*4)
	for y := 0; y < bounds.Dy(); y++ {
		for x := 0; x < bounds.Dx(); x++ {
			r, g, b, _ := image.At(x, y).RGBA()
			i := (y*bounds.Dx() + x) * 4
			w.pixels[i] = byte(b >> 8)
			w.pixels[i+1] = byte(g >> 8)
			w.pixels[i+2] = byte(r >> 8)
			w.pixels[i+3] = 255
		}
	}
	screenW, _, _ := user.NewProc("GetSystemMetrics").Call(0)
	screenH, _, _ := user.NewProc("GetSystemMetrics").Call(1)
	dc, _, _ := user.NewProc("GetDC").Call(0)
	dpi, _, _ := gdi.NewProc("GetDeviceCaps").Call(dc, 90)
	user.NewProc("ReleaseDC").Call(0, dc)
	w.scale = float64(dpi) / 96
	if max := float64(screenH-90) / 620; w.scale > max {
		w.scale = max
	}
	if max := float64(screenW-60) / 900; w.scale > max {
		w.scale = max
	}
	w.width = w.s(900)
	w.height = w.s(620)
	makeFont := func(height, weight int) uintptr {
		h, _, _ := gdi.NewProc("CreateFontW").Call(uintptr(-w.s(height)), 0, 0, 0, uintptr(weight), 0, 0, 0, 1, 0, 0, 5, 0, ptr("Segoe UI"))
		return h
	}
	w.font = makeFont(16, 400)
	w.titleFont = makeFont(20, 400)
	w.smallFont = makeFont(13, 400)
	w.brush, _, _ = gdi.NewProc("CreateSolidBrush").Call(0x0c1117)
	defer func() {
		for _, h := range []uintptr{w.font, w.titleFont, w.smallFont, w.brush} {
			gdi.NewProc("DeleteObject").Call(h)
		}
	}()
	instance, _, _ := kernel.NewProc("GetModuleHandleW").Call(0)
	cursor, _, _ := user.NewProc("LoadCursorW").Call(0, 32512)
	wc := windowClass{Instance: instance, Cursor: cursor, Name: wide("RenaissanceLauncherWindow"), Proc: syscall.NewCallback(windowProcedure)}
	wc.Size = uint32(unsafe.Sizeof(wc))
	if n, _, e := user.NewProc("RegisterClassExW").Call(uintptr(unsafe.Pointer(&wc))); n == 0 {
		return e
	}
	style := uintptr(0x00ca0000 | 0x02000000)
	r := rect{R: w.width, B: w.height}
	user.NewProc("AdjustWindowRect").Call(uintptr(unsafe.Pointer(&r)), style, 0)
	hwnd, _, e := user.NewProc("CreateWindowExW").Call(0, ptr("RenaissanceLauncherWindow"), ptr("Arcanum Renaissance"), style, (screenW-uintptr(r.R-r.L))/2, (screenH-uintptr(r.B-r.T))/2, uintptr(r.R-r.L), uintptr(r.B-r.T), 0, 0, instance, 0)
	if hwnd == 0 {
		return e
	}
	w.hwnd = hwnd
	dark := int32(1)
	syscall.NewLazyDLL("dwmapi.dll").NewProc("DwmSetWindowAttribute").Call(hwnd, 20, uintptr(unsafe.Pointer(&dark)), 4)
	create := func(class, text string, style uintptr, x, y, width, height, id int) uintptr {
		h, _, _ := user.NewProc("CreateWindowExW").Call(0, ptr(class), ptr(text), style|0x50000000, uintptr(w.s(x)), uintptr(w.s(y)), uintptr(w.s(width)), uintptr(w.s(height)), hwnd, uintptr(id), instance, 0)
		user.NewProc("SendMessageW").Call(h, 0x30, w.font, 1)
		return h
	}
	w.notes = create("EDIT", "", 0x00200000|0x00010000|0x0800|0x0040|0x0004, 82, 343, 736, 164, 104)
	user.NewProc("SendMessageW").Call(w.notes, 0x00d3, 3, uintptr(w.s(10))|(uintptr(w.s(10))<<16))
	w.play = create("BUTTON", w.tr("Jouer sans mise à jour", "Play without updating"), 0x00010000|0x000b, 562, 525, 256, 42, 101)
	w.install = create("BUTTON", w.tr("Installer", "Install"), 0x00010000|0x000b, 82, 525, 210, 42, 105)
	user.NewProc("ShowWindow").Call(w.install, 0)
	w.auto = create("BUTTON", w.tr("Lancer le jeu automatiquement", "Launch the game automatically"), 0x00010000|0x0003, 82, 535, 370, 24, 102)
	checked := uintptr(0)
	if p.AutoPlay {
		checked = 1
	}
	user.NewProc("SendMessageW").Call(w.auto, 0xf1, checked, 0)
	w.language = create("BUTTON", "FR / EN", 0x00010000|0x000b, 735, 33, 82, 28, 103)
	user.NewProc("ShowWindow").Call(hwnd, 1)
	user.NewProc("SetTimer").Call(hwnd, 1, 100, 0)
	w.refresh()
	var msg winMessage
	for {
		n, _, _ := user.NewProc("GetMessageW").Call(uintptr(unsafe.Pointer(&msg)), 0, 0, 0)
		if n == 0 {
			break
		}
		if int32(n) == -1 {
			return fmt.Errorf("window message error")
		}
		handled, _, _ := user.NewProc("IsDialogMessageW").Call(hwnd, uintptr(unsafe.Pointer(&msg)))
		if handled == 0 {
			user.NewProc("TranslateMessage").Call(uintptr(unsafe.Pointer(&msg)))
			user.NewProc("DispatchMessageW").Call(uintptr(unsafe.Pointer(&msg)))
		}
	}
	return nil
}
